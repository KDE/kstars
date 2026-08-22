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

# Raised from 0.9: that bound could clip a real, well-determined kappa before
# curve_fit even gets a chance to converge on it. The pinned-at-bounds check
# below (kappa_result > KAPPA_MAX - 0.02) is what actually judges whether a
# result is trustworthy, so the bound itself doesn't need to double as that
# judgment. Note kappa->1 is degenerate, not just large: d(0) = P*(1-kappa),
# so once a curve's first sample sits near 0, P and kappa trade off freely and
# curve_fit's own covariance on kappa blows up (confirmed on SAL-33 data) --
# pinning at any bound near 1 is a sign of an unidentified fit, not a measured
# spring constant.
KAPPA_MAX = 0.98


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

    def _finish(kappa, tau, fit_records, sign_consistent, used_warm_only=False,
               n_warm_fits=0, n_total_fits=0):
        if not return_fits:
            return kappa, tau
        return kappa, tau, {
            "fits": fit_records, "sign_consistent": sign_consistent,
            "used_warm_only": used_warm_only, "n_warm_fits": n_warm_fits,
            "n_total_fits": n_total_fits,
        }

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
                bounds = ([-50.0, 0.0, 0.1, -2.0, -10.0], [50.0, KAPPA_MAX, 10.0, 2.0, 10.0])
            else:
                def model(t, P, kappa, tau, c):
                    return P * (1.0 - kappa * np.exp(-t / tau)) + c
                p0 = [pos_arr[-1], 0.3, 1.5, 0.0]
                bounds = ([-100.0, 0.0, 0.1, -10.0], [100.0, KAPPA_MAX, 10.0, 10.0])
            popt, _ = scipy.optimize.curve_fit(model, t_arr, pos_arr, p0=p0,
                                               bounds=bounds, maxfev=10000)
            residual_std = float(np.std(pos_arr - model(t_arr, *popt)))
            return popt[0], popt[1], popt[2], residual_std
        except (RuntimeError, ValueError):
            return None

    # Cold vs warm: AIGuideProtocol fires 3 consecutive same-direction pulses per
    # axis/direction/magnitude combo (worm_gear.cpp / harmonic.cpp / direct_drive.cpp
    # phase tables) -- only the first is a true reversal ("cold", pulse_is_reversal=True),
    # the next two are "warm" (already engaged, no direction change since the last pulse).
    # On a continuously-tracking axis (RA on any mount) this distinction barely matters --
    # the motor is always turning, so a cold and a warm pulse see essentially the same
    # mechanics. On a non-tracking axis (DEC on a GEM: stationary between corrections, no
    # motor holding position) it matters a great deal: verified 2026-08-21 on a live
    # WORM_GEAR/EQMod rig that the FIRST DEC pulse of a whole session produced a clean,
    # unambiguous step (+1.14px), while every subsequent pulse -- cold or warm alike --
    # came back indistinguishable from noise (<0.55px net, random sign). Backlash
    # (position-dependent, cleared once and stays cleared until reversal) and static
    # friction from a full stop (time-dependent, recurs whenever the axis has been idle,
    # including between "warm" pulses ~30s+ apart in this protocol) are different physical
    # effects; this protocol's cold/warm labeling was designed for the former and doesn't
    # fully protect against the latter, but warm data is still never worse than cold+warm
    # mixed together, so it's preferred whenever there's enough of it to fit from.
    kappas, taus, fit_signs, paired_signs = [], [], [], set()
    kappas_warm, taus_warm, fit_signs_warm, paired_signs_warm = [], [], [], set()
    skipped_noise = 0
    fit_records = []

    def is_warm(s):
        # Default True (pulse_is_reversal absent) matches AIGuideProtocol's own
        # default for ProtocolPhase::pulseWarm=false i.e. "cold" -- but sessions
        # collected before this field existed have no way to know, and treating
        # them as warm (rather than silently cold, which would bias the warm-only
        # path toward nothing) is the safer default for old data.
        return not s.get("pulse_is_reversal", False)

    def accept_fit(kappa_fit, tau_fit, t_first, warm):
        # tau at the upper bound: exponential degenerate with the drift term
        if tau_fit > 9.8:
            return
        targets = [(kappas, taus)]
        if warm:
            targets.append((kappas_warm, taus_warm))
        # spring released before the first sample is indistinguishable from none
        value = 0.0 if tau_fit < t_first else kappa_fit
        for k_list, tau_list in targets:
            k_list.append(value)
            if value != 0.0:
                tau_list.append(tau_fit)

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
            # Sample on whichever curve's own grid started later, interpolating the
            # other one onto it. Always basing this on tp (as a naive tp>=tn[0] mask
            # would) drops tp's entire leading frame whenever tp merely happened to
            # start a fraction of a frame before tn -- an alignment coincidence
            # between two independently-fired pulses, not a real resolution limit --
            # which was silently halving the usable lead time on roughly half of all
            # pairs at fast cadence.
            if tp[0] >= tn[0]:
                t_arr_full, base_v, other_t, other_v, base_is_pos = tp, pp, tn, pn, True
            else:
                t_arr_full, base_v, other_t, other_v, base_is_pos = tn, pn, tp, pp, False
            mask = (t_arr_full >= other_t[0]) & (t_arr_full <= other_t[-1])
            if mask.sum() < 5:
                continue
            t_arr = t_arr_full[mask]
            interp_v = np.interp(t_arr, other_t, other_v)
            diff = (base_v[mask] - interp_v) if base_is_pos else (interp_v - base_v[mask])
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
            # Both legs of a pair should share warm/cold status by protocol design
            # (cold-pos paired with cold-neg, warm-pos with warm-neg); require both
            # to agree rather than assume it, in case a session got dropped upstream
            # and zip() paired mismatched legs.
            pair_warm = is_warm(sp) and is_warm(sn)
            sign = 1.0 if P_fit > 0 else -1.0
            paired_signs.add(sign)
            if pair_warm:
                paired_signs_warm.add(sign)
            accept_fit(kappa_fit, tau_fit, t_arr[0], pair_warm)
            fit_records.append({
                "pulse_magnitude_ms": float(pulse_mag), "P_fit_px": float(P_fit),
                "tau_fit_s": float(tau_fit), "residual_std_px": float(residual_std),
                "t_first_s": float(t_arr[0]), "t_arr": t_arr, "pos_arr": diff,
                "is_warm": pair_warm,
            })
            if verbose:
                print(f"  [{axis}] Pulse {pulse_mag}ms paired {pos_dir}-{neg_dir} "
                      f"({'warm' if pair_warm else 'cold'}): "
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
            s_warm = is_warm(s)
            sign_entry = (s.get("pulse_direction", "?"), np.sign(P_fit))
            accept_fit(kappa_fit, tau_fit, t_arr[0], s_warm)
            fit_signs.append(sign_entry)
            if s_warm:
                fit_signs_warm.append(sign_entry)
            fit_records.append({
                "pulse_magnitude_ms": float(pulse_mag), "P_fit_px": float(P_fit),
                "tau_fit_s": float(tau_fit), "residual_std_px": float(residual_std),
                "t_first_s": float(t_arr[0]), "t_arr": t_arr, "pos_arr": pos_arr,
                "is_warm": s_warm,
            })
            if verbose:
                print(f"  [{axis}] Pulse {pulse_mag}ms {s.get('pulse_direction', '?')} "
                      f"({'warm' if s_warm else 'cold'}): "
                      f"κ={kappa_fit:.3f}, τ={tau_fit:.2f}s (P={P_fit:.2f}px, noise={residual_std:.2f}px)")

    if not kappas:
        if verbose:
            print(f"  [{axis}] No pulse response measurable above noise "
                  f"({skipped_noise} skipped). Using defaults (κ=0.2, τ=1.5s). "
                  f"Consider larger protocol pulses.")
        return _finish(*DEFAULTS, fit_records, None)

    # Prefer warm-only data when there's enough of it: a cold (reversal) pulse's response
    # is contaminated by whatever dead-time/backlash exists on that axis, which is real
    # mechanics for a non-tracking axis (DEC on a GEM) but not representative of the
    # steady-state gain a P-controller actually needs. Falls back to the combined (cold+warm)
    # set when warm data alone isn't enough to fit from, same as this always did before.
    use_warm = len(kappas_warm) >= 1
    if use_warm:
        kappas_use, taus_use = kappas_warm, taus_warm
        paired_signs_use, fit_signs_use = paired_signs_warm, fit_signs_warm
    else:
        kappas_use, taus_use = kappas, taus
        paired_signs_use, fit_signs_use = paired_signs, fit_signs
    if verbose:
        print(f"  [{axis}] Using {'warm-only' if use_warm else 'combined cold+warm'} data: "
              f"{len(kappas_use)} of {len(kappas)} total fits "
              f"({len(kappas_warm)} warm available)")

    # Real responses have consistent signs per direction; paired diffs share one sign
    by_dir = {}
    for direction, sign in fit_signs_use:
        by_dir.setdefault(direction, set()).add(sign)
    dir_signs = [next(iter(s)) for s in by_dir.values() if len(s) == 1]
    consistent = (len(paired_signs_use) <= 1 and
                  all(len(s) == 1 for s in by_dir.values()) and
                  (len(by_dir) < 2 or len(set(dir_signs)) == len(by_dir)))
    if not consistent:
        if verbose:
            print(f"  [{axis}] WARNING: response signs inconsistent across pulse directions "
                  f"({'warm-only' if use_warm else 'combined'} data) — fits are noise, not "
                  f"mechanics. Using defaults (κ=0.2, τ=1.5s).")
        return _finish(*DEFAULTS, fit_records, False, use_warm, len(kappas_warm), len(kappas))

    kappa_result = float(np.median(kappas_use))
    tau_result = float(np.median(taus_use)) if taus_use and kappa_result > 0.0 else DEFAULTS[1]

    # A median within ~2% of the fit bounds means the model chased noise/drift, not physics.
    if kappa_result > KAPPA_MAX - 0.02 or tau_result > 9.8:
        if verbose:
            print(f"  [{axis}] WARNING: fit pinned at bounds (κ={kappa_result:.3f}, "
                  f"τ={tau_result:.2f}s) — unphysical. Using defaults (κ=0.2, τ=1.5s).")
        return _finish(*DEFAULTS, fit_records, consistent, use_warm, len(kappas_warm), len(kappas))

    if verbose:
        print(f"  [{axis}] Final: κ={kappa_result:.3f} (from {len(kappas_use)} "
              f"{'warm' if use_warm else 'cold+warm'} fits), τ={tau_result:.2f}s")

    return _finish(kappa_result, tau_result, fit_records, consistent,
                   use_warm, len(kappas_warm), len(kappas))
