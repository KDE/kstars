"""
offline_trainer/pulse_response_fit.py — generic step-response curve fitting for
pulse_response sysid sessions, shared across all three mount-type trainers.

fit_pulse_response() has no mount-specific logic: it pairs opposite-direction
pulses (so PE/drift cancel in the difference), fits an exponential-approach
model per pulse magnitude/direction, and applies the same noise/sign-
consistency acceptance gates regardless of mount type. Two independent
consumers read its output:

  - train_harmonic.py uses (kappa, tau) directly as the Harmonic Drive
    Kalman filter's spring/time-constant parameters.
  - pid_autotune.py (all three mount types) uses the per-fit P_fit/tau_fit/
    residual_std/t_arr records (return_fits=True) as FOPDT step-response
    data for the SIMC-style PID gain recommendation.

Originally lived only in train_harmonic.py (the only mount type that
collected pulse_response data); extracted here once WORM_GEAR and
DIRECT_DRIVE gained their own pulse-response phases, since the fitting logic
was already 100% mount-agnostic.

SPDX-License-Identifier: GPL-2.0-or-later
"""

import numpy as np
import scipy.optimize


def fit_pulse_response(sysid: dict, axis: str, guide_exp: float, verbose: bool,
                       return_fits: bool = False):
    """
    Fit spring constant κ and time constant τ from pulse_response sessions.

    Model: d(t) = P * (1 - κ * exp(-t/τ)) + v*t + c. Fits whose |P| is not
    significantly above the residual noise are skipped.

    Returns: (kappa, tau_seconds), or (kappa, tau_seconds, fit_info) if
    return_fits is True. fit_info["fits"] is the list of per-fit records
    (P_fit_px, tau_fit_s, residual_std_px, t_first_s, t_arr, pos_arr) this
    function already computes and would otherwise discard — reused by
    pid_autotune.recommend_pid_gains() as the step-response data for PID
    auto-tune, a second, independent consumer of the same pulse_response
    sessions. fit_info["sign_consistent"] mirrors the gate this function
    itself uses to decide the fits are real mechanics rather than noise.
    """
    # Unmeasured means unmodeled: the default kappa stays 0
    DEFAULTS = (0.0, 1.5)

    def _finish(kappa, tau, fit_records, sign_consistent):
        if not return_fits:
            return kappa, tau
        return kappa, tau, {"fits": fit_records, "sign_consistent": sign_consistent}

    pulse_sessions = [
        s for s in sysid["sessions"]
        if s.get("type") == "pulse_response" and s.get("pulse_axis", "").upper() == axis.upper()
    ]

    if not pulse_sessions:
        if verbose:
            print(f"  [{axis}] No pulse_response sessions found. Using defaults (κ=0.2, τ=1.5s)")
        return _finish(*DEFAULTS, [], None)

    axis_key = "ra_raw_px" if axis.upper() == "RA" else "dec_raw_px"

    def session_curve(s):
        """(t, signed displacement from baseline) for one pulse session, or None."""
        frames = s.get("response_frames", [])
        if len(frames) < 5:
            return None
        base = s.get("baseline_frames", [])
        if base:
            # New protocol: dedicated pre-pulse baseline; t is true seconds since the pulse.
            baseline = float(np.mean([f.get(axis_key, 0.0) for f in base]))
            t_vals = [f.get("t", (i + 1) * guide_exp) for i, f in enumerate(frames)]
            pos_vals = [f.get(axis_key, 0.0) - baseline for f in frames]
        else:
            # Legacy: first frame doubles as the baseline.
            baseline = frames[0].get(axis_key, 0.0)
            t0 = frames[0].get("t", 0.0)
            t_vals, pos_vals = [], []
            for i, f in enumerate(frames):
                if i == 0:
                    continue
                t = f.get("t", 0.0) - t0
                if t <= 0:
                    t = i * guide_exp
                t_vals.append(t)
                pos_vals.append(f.get(axis_key, 0.0) - baseline)
        if len(t_vals) < 5:
            return None
        return np.array(t_vals, dtype=float), np.array(pos_vals, dtype=float)

    def fit_curve(t_arr, pos_arr, with_drift):
        """Fit the spring model; returns (P, kappa, tau, residual_std) or None."""
        try:
            if with_drift:
                def model(t, P, kappa, tau, v, c):
                    return P * (1.0 - kappa * np.exp(-t / tau)) + v * t + c
                slope0 = (pos_arr[-1] - pos_arr[0]) / max(t_arr[-1] - t_arr[0], 1e-3)
                p0 = [pos_arr[-1] - slope0 * t_arr[-1], 0.3, 1.5, slope0, 0.0]
                bounds = ([-50.0, 0.0, 0.1, -2.0, -10.0], [50.0, 0.9, 10.0, 2.0, 10.0])
            else:
                def model(t, P, kappa, tau, c):
                    return P * (1.0 - kappa * np.exp(-t / tau)) + c
                p0 = [pos_arr[-1], 0.3, 1.5, 0.0]
                bounds = ([-100.0, 0.0, 0.1, -10.0], [100.0, 0.9, 10.0, 10.0])
            popt, _ = scipy.optimize.curve_fit(model, t_arr, pos_arr, p0=p0,
                                               bounds=bounds, maxfev=10000)
            residual_std = float(np.std(pos_arr - model(t_arr, *popt)))
            return popt[0], popt[1], popt[2], residual_std
        except (RuntimeError, ValueError):
            return None

    kappas = []
    taus = []
    fit_signs = []
    paired_signs = set()
    skipped_noise = 0
    fit_records = []

    def accept_fit(kappa_fit, tau_fit, t_first):
        # tau at the upper bound: exponential degenerate with the drift term
        if tau_fit > 9.8:
            return
        # spring released before the first sample is indistinguishable from none
        if tau_fit < t_first:
            kappas.append(0.0)
        else:
            kappas.append(kappa_fit)
            taus.append(tau_fit)

    # Pair opposite-direction sessions: the difference doubles the response
    pos_dir, neg_dir = ("EAST", "WEST") if axis.upper() == "RA" else ("NORTH", "SOUTH")
    by_mag = {}
    for s in pulse_sessions:
        by_mag.setdefault(s.get("pulse_magnitude_ms", 100.0), []).append(s)

    for pulse_mag, group in sorted(by_mag.items()):
        pos_list = [s for s in group if s.get("pulse_direction", "").upper() == pos_dir]
        neg_list = [s for s in group if s.get("pulse_direction", "").upper() == neg_dir]
        paired = list(zip(pos_list, neg_list))
        leftovers = pos_list[len(paired):] + neg_list[len(paired):]

        for sp, sn in paired:
            cp, cn = session_curve(sp), session_curve(sn)
            if cp is None or cn is None:
                continue
            tp, pp = cp
            tn, pn = cn
            mask = (tp >= tn[0]) & (tp <= tn[-1])
            if mask.sum() < 5:
                continue
            t_arr = tp[mask]
            diff = pp[mask] - np.interp(t_arr, tn, pn)
            # Sessions are minutes apart so PE does not cancel exactly; v absorbs the leak
            fit = fit_curve(t_arr, diff, with_drift=True)
            if fit is None:
                if verbose:
                    print(f"  [{axis}] Pulse {pulse_mag}ms paired: curve_fit failed")
                continue
            P_fit, kappa_fit, tau_fit, residual_std = fit
            if abs(P_fit) < 2.0 * residual_std:
                skipped_noise += 1
                if verbose:
                    print(f"  [{axis}] Pulse {pulse_mag}ms paired: |P|={abs(P_fit):.2f}px "
                          f"below noise ({residual_std:.2f}px) — skipped")
                continue
            paired_signs.add(1.0 if P_fit > 0 else -1.0)
            accept_fit(kappa_fit, tau_fit, t_arr[0])
            fit_records.append({
                "pulse_magnitude_ms": float(pulse_mag), "P_fit_px": float(P_fit),
                "tau_fit_s": float(tau_fit), "residual_std_px": float(residual_std),
                "t_first_s": float(t_arr[0]), "t_arr": t_arr, "pos_arr": diff,
            })
            if verbose:
                print(f"  [{axis}] Pulse {pulse_mag}ms paired {pos_dir}-{neg_dir}: "
                      f"κ={kappa_fit:.3f}, τ={tau_fit:.2f}s (P={P_fit:.2f}px, noise={residual_std:.2f}px)")

        for s in leftovers:
            c = session_curve(s)
            if c is None:
                continue
            t_arr, pos_arr = c
            fit = fit_curve(t_arr, pos_arr, with_drift=True)
            if fit is None:
                if verbose:
                    print(f"  [{axis}] Pulse {pulse_mag}ms: curve_fit failed")
                continue
            P_fit, kappa_fit, tau_fit, residual_std = fit
            if abs(P_fit) < 2.0 * residual_std:
                skipped_noise += 1
                if verbose:
                    print(f"  [{axis}] Pulse {pulse_mag}ms {s.get('pulse_direction', '?')}: "
                          f"response |P|={abs(P_fit):.2f}px below noise ({residual_std:.2f}px) — skipped")
                continue
            accept_fit(kappa_fit, tau_fit, t_arr[0])
            fit_signs.append((s.get("pulse_direction", "?"), np.sign(P_fit)))
            fit_records.append({
                "pulse_magnitude_ms": float(pulse_mag), "P_fit_px": float(P_fit),
                "tau_fit_s": float(tau_fit), "residual_std_px": float(residual_std),
                "t_first_s": float(t_arr[0]), "t_arr": t_arr, "pos_arr": pos_arr,
            })
            if verbose:
                print(f"  [{axis}] Pulse {pulse_mag}ms {s.get('pulse_direction', '?')}: "
                      f"κ={kappa_fit:.3f}, τ={tau_fit:.2f}s (P={P_fit:.2f}px, noise={residual_std:.2f}px)")

    if not kappas:
        if verbose:
            print(f"  [{axis}] No pulse response measurable above noise "
                  f"({skipped_noise} skipped). Using defaults (κ=0.2, τ=1.5s). "
                  f"Consider larger protocol pulses.")
        return _finish(*DEFAULTS, fit_records, None)

    # Real responses have consistent signs per direction; paired diffs share one sign
    by_dir = {}
    for direction, sign in fit_signs:
        by_dir.setdefault(direction, set()).add(sign)
    dir_signs = [next(iter(s)) for s in by_dir.values() if len(s) == 1]
    consistent = (len(paired_signs) <= 1 and
                  all(len(s) == 1 for s in by_dir.values()) and
                  (len(by_dir) < 2 or len(set(dir_signs)) == len(by_dir)))
    if not consistent:
        if verbose:
            print(f"  [{axis}] WARNING: response signs inconsistent across pulse directions "
                  f"— fits are noise, not mechanics. Using defaults (κ=0.2, τ=1.5s).")
        return _finish(*DEFAULTS, fit_records, False)

    kappa_result = float(np.median(kappas))
    tau_result = float(np.median(taus)) if taus and kappa_result > 0.0 else DEFAULTS[1]

    # A median within ~2% of the fit bounds means the model chased noise/drift, not physics.
    if kappa_result > 0.88 or tau_result > 9.8:
        if verbose:
            print(f"  [{axis}] WARNING: fit pinned at bounds (κ={kappa_result:.3f}, "
                  f"τ={tau_result:.2f}s) — unphysical. Using defaults (κ=0.2, τ=1.5s).")
        return _finish(*DEFAULTS, fit_records, consistent)

    if verbose:
        print(f"  [{axis}] Final: κ={kappa_result:.3f} (from {len(kappas)} fits), "
              f"τ={tau_result:.2f}s")

    return _finish(kappa_result, tau_result, fit_records, consistent)
