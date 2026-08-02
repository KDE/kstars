"""
offline_trainer/train_harmonic.py — Neural Kalman Filter trainer for harmonic drives.

Uses curve-fitting (scipy.optimize) on pulse_response sysid sessions to extract
the spring constant κ and time constant τ. Detects PE period via FFT/Lomb-Scargle
on free-drift data (searching 0.1-0.5 Hz for harmonic drive PE periods of 2-10s).
Then trains a small Q-net MLP on closed-loop sessions.

SPDX-License-Identifier: GPL-2.0-or-later
"""

import json
import numpy as np
import scipy.signal
import scipy.stats
import scipy.optimize
from datetime import datetime
import sys

try:
    import torch
    import torch.nn as nn
    import torch.optim as optim
    from torch.utils.data import TensorDataset, DataLoader
    TORCH_AVAILABLE = True
except ImportError:
    TORCH_AVAILABLE = False



# Default SIMC design parameter for the PID auto-tune recommendation (§ Phase 1b
# below): lambda = max(tau, SIMC_LAMBDA_L_FACTOR * dead_time). Larger -> slower/
# more robust closed loop; smaller -> faster/less margin. Kept as an explicit,
# overridable constant per the plan's "expose lambda as a documented knob, not a
# hidden constant" (pid_autotune_plan.md §3.2).
SIMC_LAMBDA_L_FACTOR = 3.0

# KStars' "integral gain" (Options::rAIntegralGain()/dECIntegralGain()) multiplies
# a ~100-frame moving average of drift (gmath.cpp::processAxis(), drift_integral[k]),
# not a classical accumulating integrator. SIMC's tau_I (a reset *time*) has no
# principled mapping onto that EMA-style term, so rather than inventing one, the
# integral recommendation is a conservative fixed fraction of the proportional
# recommendation (pid_autotune_plan.md §1: "keep I conservative"). tau_I is still
# reported in the output for reference.
INTEGRAL_GAIN_CONSERVATIVE_FRACTION = 0.25

# Below this many usable step-response fits, the recommendation is flagged "low"
# confidence rather than withheld outright -- still informative, not to be applied
# unattended (pid_autotune_plan.md §4).
MIN_FITS_FOR_MEDIUM_CONFIDENCE = 6


def _effective_pixel_scale(sysid):
    """Pixel scale in arcsec/px. Older exports recorded it without the binning factor."""
    eq = sysid.get("equipment", {})
    ps = float(eq.get("pixel_scale_arcsec_per_px", 1.0) or 1.0)
    if not eq.get("pixel_scale_includes_binning", False):
        b = str(sysid.get("model_fingerprint", {}).get("guide_binning", "1x1"))
        try:
            bf = max(1, int(b.split("x")[0]))
        except ValueError:
            bf = 1
        if bf > 1:
            print(f"  [scale] correcting pixel scale for binning {bf}x: "
                  f"{ps:.3f} -> {ps * bf:.3f} arcsec/px")
            ps *= bf
    return ps

def train_harmonic(sysid: dict,
                   gpu: bool = False,
                   epochs: int = None,
                   verbose: bool = False,
                   pid_lambda_factor: float = SIMC_LAMBDA_L_FACTOR) -> dict:
    """
    Fit κ/τ spring parameters, detect PE, fit drift, and train Q-net
    for a harmonic drive mount.
    Returns a weights dict compatible with HarmonicGuider::loadWeights().

    Also computes a "pid_autotune" recommendation block (RA/DEC proportional +
    conservative integral gain, derived from the same pulse_response sessions
    via a SIMC/IMC-style step-response rule -- pid_autotune_plan.md §3-5). This
    is advisory only: nothing in this module or HarmonicGuider ever reads it
    back or applies it automatically. A human reviews the numbers and manually
    updates Options::setRAProportionalGain()/etc. in KStars, the same
    human-in-the-loop workflow already used for loading a new weights.json.
    """
    eq = sysid["equipment"]
    pixel_scale = _effective_pixel_scale(sysid)
    guide_exp   = eq.get("guide_exposure_ms", 1000.0) / 1000.0

    if verbose:
        print(f"\n--- Phase 1: Spring Parameter Fitting ---")

    # ── Step 1: Fit spring parameters from pulse_response sessions ─────────
    kappa_ra, tau_ra   = _fit_spring_params(sysid, "RA",  guide_exp, verbose)
    kappa_dec, tau_dec = _fit_spring_params(sysid, "DEC", guide_exp, verbose)

    if verbose:
        print(f"  κ_ra={kappa_ra:.3f}  τ_ra={tau_ra:.2f}s")
        print(f"  κ_dec={kappa_dec:.3f}  τ_dec={tau_dec:.2f}s")
        print(f"\n--- Phase 1b: PID Auto-tune (recommendation only, not applied) ---")

    # ── Step 1b: Recommend base P(+I) guide gains from the same step-response data ──
    pid_autotune = _recommend_pid_gains(sysid, guide_exp, verbose, pid_lambda_factor)

    if verbose:
        print(f"\n--- Phase 2: PE Period Detection ---")

    # ── Step 2: Detect PE period from free-drift data ──────────────────────
    pe_period, pe_amplitude, pe_lines = _estimate_pe(sysid, guide_exp, verbose)

    if verbose:
        if pe_period > 0:
            print(f"  PE period: {pe_period:.2f}s  amplitude: {pe_amplitude:.4f} px")
        else:
            print(f"  No significant PE detected — PE states will be inactive")
        print(f"\n--- Phase 3: Drift Parameter Fitting ---")

    # ── Step 3: Fit drift parameters from free_drift sessions ──────────────
    drift_ra, drift_dec, d_polar, k_ref, k_ref_dec = _fit_drift_params(
        sysid, guide_exp, verbose)

    # Altitude range the drift/refraction fit is valid for (runtime clamps to it).
    fit_alts = [s.get("altitude_deg", 45.0) for s in sysid["sessions"]
                if s.get("type") == "free_drift" and len(s.get("frames", [])) >= 10]
    fit_alt_min = min(fit_alts) if fit_alts else 35.0
    fit_alt_max = max(fit_alts) if fit_alts else 65.0

    if verbose:
        print(f"  drift_ra={drift_ra:.6e} px/s  drift_dec={drift_dec:.6e} px/s")
        print(f"  d_polar={d_polar:.6e} px/s")
        print(f"  k_ref={k_ref:.6e}  k_ref_dec={k_ref_dec:.6e}")
        print(f"\n--- Phase 4: Q-net Training ---")

    # ── Step 4: Train Q-net on closed-loop sessions ────────────────────────
    qnet_weights = _train_qnet(sysid, kappa_ra, tau_ra, kappa_dec, tau_dec,
                               pe_period, gpu, epochs, verbose)

    def _recommended(axis_result, field):
        return axis_result[field] if axis_result["confidence"] != "unavailable" else None

    from train_direct_drive import _build_fingerprint
    return {
        "format_version":    "1.0",
        "mount_type":        "HARMONIC_DRIVE",
        "trained_at":        datetime.utcnow().isoformat() + "Z",
        "mount_name":        eq.get("mount_name", "unknown"),
        "pixel_scale":       pixel_scale,
        "model_fingerprint": _build_fingerprint(sysid),
        # Advisory PID auto-tune recommendation -- see train_harmonic()'s docstring.
        # recommended_* fields are None when confidence is "unavailable" (don't apply).
        "recommended_ra_proportional_gain":  _recommended(pid_autotune["ra"], "proportional_gain"),
        "recommended_ra_integral_gain":      _recommended(pid_autotune["ra"], "integral_gain"),
        "recommended_dec_proportional_gain": _recommended(pid_autotune["dec"], "proportional_gain"),
        "recommended_dec_integral_gain":     _recommended(pid_autotune["dec"], "integral_gain"),
        "pid_autotune": pid_autotune,
        "physical": {
            "kappa_ra":      float(kappa_ra),
            "tau_ra":        float(tau_ra),
            "kappa_dec":     float(kappa_dec),
            "tau_dec":       float(tau_dec),
            "pe_period":     float(pe_period),
            "pe_amplitude":  float(pe_amplitude),
            "pe_lines":      pe_lines,
            "drift_ra":      float(drift_ra),
            "drift_dec":     float(drift_dec),
            "k_ref":         float(k_ref),
            "d_polar":       float(d_polar),
            "k_ref_dec":     float(k_ref_dec),
            "fit_alt_min":   float(fit_alt_min),
            "fit_alt_max":   float(fit_alt_max),
        },
        "qnet": qnet_weights,
    }


# ═══════════════════════════════════════════════════════════════════════════════
# Phase 1: Spring parameter fitting from pulse_response sessions
# ═══════════════════════════════════════════════════════════════════════════════

def _fit_spring_params(sysid: dict, axis: str, guide_exp: float, verbose: bool,
                       return_fits: bool = False):
    """
    Fit spring constant κ and time constant τ from pulse_response sessions.

    Model: d(t) = P * (1 - κ * exp(-t/τ)) + v*t + c. Fits whose |P| is not
    significantly above the residual noise are skipped.

    Returns: (kappa, tau_seconds), or (kappa, tau_seconds, fit_info) if
    return_fits is True. fit_info["fits"] is the list of per-fit records
    (P_fit_px, tau_fit_s, residual_std_px, t_first_s, t_arr, pos_arr) this
    function already computes and would otherwise discard — reused by
    _recommend_pid_gains() as the step-response data for PID auto-tune, a
    second, independent consumer of the same pulse_response sessions.
    fit_info["sign_consistent"] mirrors the gate this function itself uses
    to decide the fits are real mechanics rather than noise.
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


# ═══════════════════════════════════════════════════════════════════════════════
# Phase 1b: PID auto-tune -- SIMC step-response gain recommendation
#
# Offline, calibration-time-only calculation (pid_autotune_plan.md). Reuses the
# per-fit P_fit/tau_fit/residual_std/t_arr values _fit_spring_params() already
# computes from pulse_response sessions -- no new data collection, just a second
# consumer of the existing step-response fits.
# ═══════════════════════════════════════════════════════════════════════════════

def _estimate_dead_time_s(t_arr: np.ndarray, pos_arr: np.ndarray, residual_std: float) -> float:
    """
    First t at which |pos(t)| exceeds ~2.5x the fit's residual noise -- a proxy
    for FOPDT dead time L (pid_autotune_plan.md §3.1). If even the first sample
    already exceeds threshold (the common case at ~2-3s/frame cadence), this
    returns t_arr[0]: L is only known to be <= the first sample, not resolved any
    finer -- see _recommend_axis_pid_gain()'s "resolution_limited" flag.
    """
    threshold = max(2.5 * residual_std, 1e-6)
    for t, p in zip(t_arr, pos_arr):
        if abs(p) > threshold:
            return float(t)
    return float(t_arr[-1])


def _calibration_ms_per_arcsec(sysid: dict, axis: str) -> float:
    """
    Median calibrated ms-per-arcsec pulse rate for this axis, from whichever
    sessions recorded it (standard_guiding sessions carry ra_ms_per_arcsec/
    dec_ms_per_arcsec). This is Calibration::ra/decPulseMillisecondsPerArcsecond()
    at collection time -- the same normalization gmath.cpp::processAxis() applies
    to proportional_gain, needed to convert a physical SIMC Kc (ms/arcsec) back
    into KStars' dimensionless aggressiveness (pid_autotune_plan.md §3.3).
    """
    key = "ra_ms_per_arcsec" if axis.upper() == "RA" else "dec_ms_per_arcsec"
    values = [s[key] for s in sysid["sessions"] if s.get(key, 0.0) and s[key] > 0.0]
    return float(np.median(values)) if values else 0.0


def _recommend_axis_pid_gain(sysid: dict, axis: str, guide_exp: float,
                             lambda_l_factor: float, verbose: bool) -> dict:
    """
    Derive a recommended base proportional/integral gain for one axis from its
    pulse_response step-response fits, via a conservative SIMC/IMC-style PI rule
    (pid_autotune_plan.md §3.2). Returns a dict; confidence "unavailable" means
    the numbers (if present at all) should not be used.
    """
    _, _, fit_info = _fit_spring_params(sysid, axis, guide_exp, False, return_fits=True)
    fits = fit_info["fits"]

    if fit_info["sign_consistent"] is False:
        return {"confidence": "unavailable",
                "reason": "pulse-response signs inconsistent across directions -- fits are noise, not mechanics"}
    if not fits:
        return {"confidence": "unavailable", "reason": "no usable pulse-response step-response fits"}

    cal_ms_per_arcsec = _calibration_ms_per_arcsec(sysid, axis)
    if cal_ms_per_arcsec <= 0.0:
        return {"confidence": "unavailable",
                "reason": "no calibrated ms_per_arcsec recorded for this axis (need a standard_guiding session)"}

    pixel_scale = _effective_pixel_scale(sysid)

    K_samples, tau_samples, L_samples, t_first_samples = [], [], [], []
    for f in fits:
        if f["pulse_magnitude_ms"] <= 0.0:
            continue
        K_samples.append(abs(f["P_fit_px"]) * pixel_scale / f["pulse_magnitude_ms"])
        t_first_samples.append(f["t_first_s"])
        L_samples.append(_estimate_dead_time_s(f["t_arr"], f["pos_arr"], f["residual_std_px"]))
        if f["tau_fit_s"] <= 9.8:  # same "pinned at bound, degenerate with drift" guard as the spring fit
            tau_samples.append(f["tau_fit_s"])

    if not K_samples:
        return {"confidence": "unavailable", "reason": "no fit had a usable pulse magnitude"}

    K = float(np.median(K_samples))  # arcsec of steady-state response per ms of pulse
    L = float(np.median(L_samples))
    # tau can't be resolved below the sampling floor either: a fit whose tau_fit
    # landed below t_first ("spring already released") is only known to be
    # <= t_first, not physically ~0 -- floor it the same way L is floored, so a
    # spuriously tiny fitted tau can't produce a dangerously aggressive Kc.
    tau_raw = float(np.median(tau_samples)) if tau_samples else float(np.median(t_first_samples))
    tau = max(tau_raw, L)
    resolution_limited = bool(np.isclose(L, float(np.median(t_first_samples)), rtol=0.05))

    lam = max(tau, lambda_l_factor * L)
    Kc = (1.0 / K) * tau / (lam + L)               # ms of pulse per arcsec of error
    tau_i = min(tau, 4.0 * (lam + L))              # SIMC reset time, reported only -- see constant doc above

    proportional_gain = Kc / cal_ms_per_arcsec
    integral_gain = INTEGRAL_GAIN_CONSERVATIVE_FRACTION * proportional_gain
    confidence = "low" if (resolution_limited or len(fits) < MIN_FITS_FOR_MEDIUM_CONFIDENCE) else "medium"

    if verbose:
        print(f"  [{axis} PID] K={K:.5f} arcsec/ms  tau={tau:.2f}s  L={L:.2f}s  lambda={lam:.2f}s "
              f"(n={len(fits)} fits, cal={cal_ms_per_arcsec:.1f}ms/arcsec)")
        print(f"  [{axis} PID] Recommended proportional_gain={proportional_gain:.3f}  "
              f"integral_gain={integral_gain:.3f}  confidence={confidence}")

    return {
        "confidence":                 confidence,
        "proportional_gain":          float(np.clip(proportional_gain, 0.0, 1.0)),
        "integral_gain":              float(np.clip(integral_gain, 0.0, 1.0)),
        "process_gain_arcsec_per_ms": K,
        "tau_s":                      tau,
        "dead_time_s":                L,
        "lambda_s":                   lam,
        "tau_i_s":                    tau_i,
        "calibration_ms_per_arcsec":  cal_ms_per_arcsec,
        "n_fits":                     len(fits),
        "resolution_limited":         resolution_limited,
    }


def _recommend_pid_gains(sysid: dict, guide_exp: float, verbose: bool,
                         lambda_l_factor: float = SIMC_LAMBDA_L_FACTOR) -> dict:
    """
    Recommend base RA/DEC proportional (+ conservative integral) gains from the
    pulse_response step-response data, via a conservative SIMC/IMC-style PI rule
    (pid_autotune_plan.md §3). Calibration-time only, never auto-applied -- see
    train_harmonic()'s docstring for how these surface in the weights JSON.
    """
    return {
        "ra":  _recommend_axis_pid_gain(sysid, "RA",  guide_exp, lambda_l_factor, verbose),
        "dec": _recommend_axis_pid_gain(sysid, "DEC", guide_exp, lambda_l_factor, verbose),
    }


# ═══════════════════════════════════════════════════════════════════════════════
# Phase 2: PE period detection from free-drift data
# ═══════════════════════════════════════════════════════════════════════════════

def _pulse_correction_px(pulse_ms, cal_rate_ms_per_arcsec, pixel_scale):
    """Signed pulse displacement in pixels (same helper as train_worm_gear)."""
    if pulse_ms == 0.0 or cal_rate_ms_per_arcsec <= 0.0:
        return 0.0
    return (pulse_ms / cal_rate_ms_per_arcsec) / pixel_scale


def _pe_candidate_series(sysid: dict, guide_exp: float):
    """
    Build (t, position, label) series for PE search.

    Free drifts give the uncorrected trajectory directly. Standard-guiding
    sessions are much longer (8 min vs 2 min) and reach the wave-generator
    periods (~300-900s), so reconstruct their uncorrected trajectory by
    adding the applied pulses back (same compensated formula as train_worm_gear:
    JSON RA pulses are ADDED, DEC pulses SUBTRACTED).
    """
    pixel_scale = _effective_pixel_scale(sysid)
    series = []
    for s in sysid["sessions"]:
        frames = s.get("frames", [])
        if len(frames) < 20:
            continue
        if s["type"] == "free_drift":
            t, pos = 0.0, []
            t_vals = []
            for f in frames:
                t += f.get("dt", guide_exp)
                t_vals.append(t)
                pos.append(f["ra_raw_px"])
            series.append((np.array(t_vals), np.array(pos), f"free_drift {s.get('session_id', '?')}"))
        elif s["type"] == "standard_guiding":
            cal = s.get("ra_ms_per_arcsec", 0.0)
            if cal <= 0.0 or pixel_scale <= 0.0:
                continue
            t, p = 0.0, 0.0
            t_vals, pos = [], []
            for i in range(1, len(frames)):
                t += frames[i].get("dt", guide_exp)
                p += (frames[i]["ra_raw_px"] - frames[i - 1]["ra_raw_px"]
                      + _pulse_correction_px(frames[i - 1].get("ra_pulse_ms", 0.0), cal, pixel_scale))
                t_vals.append(t)
                pos.append(p)
            series.append((np.array(t_vals), np.array(pos), f"standard {s.get('session_id', '?')}"))
    return series


def _estimate_pe(sysid: dict, guide_exp: float, verbose: bool):
    """
    Detect PE lines via Lomb-Scargle over every usable session.
    Collects every significant peak, applies the band-edge and free-drift
    confirmation guards per peak, and selects the STRONGEST surviving line
    (by amplitude) as the primary — not merely the first the window resolves.
    Returns (period_seconds, amplitude_pixels, lines) where lines is a list of
    {"period_s", "amplitude_px", "snr"} for all surviving lines, primary first.
    """
    candidates = _pe_candidate_series(sysid, guide_exp)
    if not candidates:
        if verbose:
            print("  No usable sessions found. PE detection skipped.")
        return 0.0, 0.0, []

    peaks = []         # (snr, period, amplitude, label, at_edge)
    drift_peaks = []   # (span, period, snr) from free-drift series
    series_bands = []  # (label, band_min_s, band_max_s, [peak periods]) per series
    for t_arr, ra_arr, label in candidates:
        span = t_arr[-1] - t_arr[0]
        # Nyquist from the real frame cadence, not the exposure: download/processing
        # overhead makes dt >> exposure and sub-cadence peaks are aliases.
        dt_med = float(np.median(np.diff(t_arr))) if len(t_arr) > 1 else guide_exp
        nyquist = 0.5 / max(dt_med, guide_exp)
        f_min = max(2.0 / span, 0.002)  # need >= 2 observed cycles
        f_max = min(0.5, nyquist * 0.9)
        if f_min >= f_max or span <= 0:
            continue

        slope, intercept, _, _, _ = scipy.stats.linregress(t_arr, ra_arr)
        ra_detrended = ra_arr - (slope * t_arr + intercept)

        f_search = np.geomspace(f_min, f_max, 4000)
        omega = 2 * np.pi * f_search
        Pxx = scipy.signal.lombscargle(t_arr, ra_detrended, omega, precenter=True)
        noise_floor = np.median(Pxx) + 1e-10

        # Every significant local maximum, not just the argmax
        series_peaks = []
        # A line longer than the band rises monotonically to the edge and never forms
        # a local maximum — record the endpoint so the leakage guard can see it.
        if Pxx[0] / noise_floor >= 10.0 and Pxx[0] >= Pxx[1]:
            series_peaks.append((Pxx[0] / noise_floor, 1.0 / f_search[0],
                                 np.sqrt(4 * Pxx[0] / len(t_arr)), label, True))
        for i in range(1, len(Pxx) - 1):
            if Pxx[i] > Pxx[i - 1] and Pxx[i] > Pxx[i + 1] and Pxx[i] / noise_floor >= 10.0:
                period = 1.0 / f_search[i]
                snr = Pxx[i] / noise_floor
                amplitude = np.sqrt(4 * Pxx[i] / len(t_arr))
                at_edge = f_search[i] <= f_min * 1.05
                series_peaks.append((snr, period, amplitude, label, at_edge))
                if label.startswith("free_drift"):
                    drift_peaks.append((span, period, snr))
        peaks.extend(series_peaks)
        series_bands.append((label, 1.0 / f_max, min(1.0 / f_min, span / 2.0),
                             [(p[1], p[2]) for p in series_peaks]))
        if verbose:
            tops = sorted(series_peaks, key=lambda p: -p[2])[:3]
            desc = ", ".join(f"{p[1]:.0f}s (amp {p[2]:.2f}px, SNR {p[0]:.0f})" for p in tops)
            print(f"  [LS] {label}: span={span:.0f}s, band {1/f_max:.1f}-{1/f_min:.0f}s, "
                  f"peaks: {desc if desc else 'none significant'}")

    drift_spans = [s for s, _, _ in drift_peaks]
    survivors = []
    edge_max_amp = 0.0
    edge_max_period = 0.0
    for snr, period, amplitude, label, at_edge in peaks:
        if at_edge:
            if amplitude > edge_max_amp:
                edge_max_amp, edge_max_period = amplitude, period
            if verbose:
                print(f"  [LS] dropping {period:.0f}s from {label}: at band edge — true "
                      f"period unresolved (need a session of {2.5 * period:.0f}s+)")
            continue
        # Cross-series consistency: real mount PE must appear in EVERY other series
        # whose band covers its period; a line only one series sees is an artifact
        # of that block (guiding oscillation, wind, settling).
        coverers = [b for b in series_bands
                    if b[0] != label and b[1] <= period <= b[2]]
        if coverers:
            # Confirmation needs matching period AND compatible amplitude (within 3x):
            # a 20x amplitude mismatch is two different phenomena, not one line.
            confirmed_x = any(any(abs(p - period) / period < 0.15
                                  and max(a, amplitude) / max(min(a, amplitude), 1e-6) <= 3.0
                                  for p, a in b[3])
                              for b in coverers)
            if not confirmed_x:
                if verbose:
                    print(f"  [LS] dropping {period:.0f}s (amp {amplitude:.2f}px) from {label}: "
                          f"not consistently seen by other series covering that band")
                continue
        # Reconstructed (standard-guiding) series lie when pulses did not physically
        # act; a peak a free drift could have resolved must be confirmed by one.
        if not label.startswith("free_drift"):
            coverable = [s for s in drift_spans if period <= s / 2.0]
            confirmed = any(abs(p - period) / period < 0.2 and dsnr >= 10.0
                            for _, p, dsnr in drift_peaks)
            if coverable and not confirmed:
                if verbose:
                    print(f"  [LS] dropping {period:.0f}s (SNR {snr:.0f}): reconstruction-only, "
                          f"not confirmed by free drift — likely pulse back-out artifact")
                continue
        survivors.append((snr, period, amplitude))

    if not survivors:
        if verbose:
            print("  [LS] No significant PE line survived the guards. Disabling PE states.")
        return 0.0, 0.0, []

    # Dedupe lines within 15% of each other (keep the strongest), order by amplitude
    survivors.sort(key=lambda p: -p[2])
    lines = []
    for snr, period, amplitude in survivors:
        if any(abs(period - l["period_s"]) / l["period_s"] < 0.15 for l in lines):
            continue
        lines.append({"period_s": float(np.clip(period, 1.5, 1500.0)),
                      "amplitude_px": float(np.clip(amplitude, 0.01, 50.0)),
                      "snr": float(snr)})
        if len(lines) >= 4:
            break

    primary = lines[0]
    # A dominant unresolved line leaks sidelobes into the band; when it dwarfs every
    # resolved line, the resolved ones cannot be trusted either.
    if edge_max_amp > 1.5 * primary["amplitude_px"]:
        if verbose:
            print(f"  [LS] WARNING: unresolved PE at the band edge (~{edge_max_period:.0f}s, "
                  f"amp {edge_max_amp:.2f}px) dominates every resolved line — its leakage "
                  f"would masquerade as PE. Collect a session of at least "
                  f"{2.5 * edge_max_period:.0f}s and retrain. Disabling PE states.")
        return 0.0, 0.0, []
    if verbose:
        print(f"  [LS] Selected strongest line: {primary['period_s']:.1f}s, "
              f"amp {primary['amplitude_px']:.3f}px"
              + (f"; secondary lines: " + ", ".join(f"{l['period_s']:.0f}s" for l in lines[1:])
                 if len(lines) > 1 else ""))

    return primary["period_s"], primary["amplitude_px"], lines


# ═══════════════════════════════════════════════════════════════════════════════
# Phase 3: Drift parameter fitting
# ═══════════════════════════════════════════════════════════════════════════════

def _fit_drift_params(sysid: dict, guide_exp: float, verbose: bool):
    """
    Fit RA/DEC drift rates, polar drift, and refraction coefficients
    from free_drift sessions at multiple altitudes.

    Returns: (drift_ra, drift_dec, d_polar, k_ref, k_ref_dec)
    """
    free_drift_sessions = [s for s in sysid["sessions"] if s["type"] == "free_drift"]

    ra_rates = []
    dec_rates = []
    cos2_alts = []  # 1/cos²(alt) for each session
    q_factors = []  # sin(q)/cos²(alt) for each session
    q_angles = []

    for s in free_drift_sessions:
        frames = s["frames"]
        if len(frames) < 10:
            continue

        # A truncated drift (stopped by the excursion guard) right after a slew measures
        # settling motion, not drift — one such point poisons the whole refraction fit.
        requested = float(s.get("duration_s", 0.0))
        span = sum(f.get("dt", guide_exp) for f in frames[1:])
        if requested > 0.0 and span < 0.5 * requested:
            if verbose:
                print(f"  [drift] skipping {s.get('session_id', '?')}: only {span:.0f}s of "
                      f"{requested:.0f}s requested — truncated, not a drift measurement")
            continue

        alt = s.get("altitude_deg", 45.0)

        t = 0.0
        t_vals, ra_vals, dec_vals = [], [], []
        q_sum = 0.0
        for f in frames:
            t += f.get("dt", guide_exp)
            t_vals.append(t)
            ra_vals.append(f["ra_raw_px"])
            dec_vals.append(f.get("dec_raw_px", 0.0))
            q_sum += f.get("parallactic_angle_deg", 0.0)

        ra_slope, _, _, _, _ = scipy.stats.linregress(t_vals, ra_vals)
        dec_slope, _, _, _, _ = scipy.stats.linregress(t_vals, dec_vals)
        ra_rates.append(ra_slope)
        dec_rates.append(dec_slope)

        cos_alt = max(abs(np.cos(np.radians(alt))), 1e-4)
        cos2_alts.append(1.0 / (cos_alt ** 2))

        avg_q_deg = q_sum / len(frames)
        q_angles.append(avg_q_deg)
        q_rad = np.radians(avg_q_deg)
        q_factors.append(np.sin(q_rad) / (cos_alt ** 2))

    if not ra_rates:
        return 0.0, 0.0, 0.0, 0.0, 0.0

    # RA: rate = k_ref / cos²(alt) + drift_ra_extra
    if len(ra_rates) >= 2:
        k_ref, drift_ra = scipy.stats.linregress(cos2_alts, ra_rates)[:2]
    else:
        k_ref = 0.0
        drift_ra = ra_rates[0]

    # DEC: rate = d_polar + k_ref_dec * sin(q)/cos²(alt)
    q_range = max(q_angles) - min(q_angles) if len(q_angles) >= 2 else 0.0

    if len(dec_rates) >= 2 and q_range >= 20.0:
        k_ref_dec, d_polar = scipy.stats.linregress(q_factors, dec_rates)[:2]
        if verbose:
            print(f"  [DEC] Parallactic angle range: {q_range:.1f}° (sufficient)")
    elif len(dec_rates) >= 2:
        if verbose:
            print(f"  [DEC] Parallactic angle range: {q_range:.1f}° < 20° — "
                  f"falling back k_ref_dec = k_ref")
        k_ref_dec = k_ref
        d_polar = float(np.mean([r - k_ref * qf for r, qf in zip(dec_rates, q_factors)]))
    else:
        d_polar = dec_rates[0] if dec_rates else 0.0
        k_ref_dec = 0.0

    drift_dec = 0.0  # Absorbed into d_polar

    return float(drift_ra), float(drift_dec), float(d_polar), float(k_ref), float(k_ref_dec)


# ═══════════════════════════════════════════════════════════════════════════════
# Phase 4: Q-net MLP training
# ═══════════════════════════════════════════════════════════════════════════════

def _train_qnet(sysid, kappa_ra, tau_ra, kappa_dec, tau_dec,
                pe_period, gpu, epochs, verbose) -> dict:
    """
    Train Q-net MLP on closed-loop (standard_guiding) sessions.

    Architecture: 5 inputs [snr, snr_delta, |innov_ra|, |innov_dec|, dt]
                → 8 hidden (ReLU) → 2 outputs [log_Q_ra, log_Q_dec]

    NOTE: this does NOT run the Kalman filter forward. As a lightweight proxy for
    the innovation it uses the raw frame-to-frame change of the tracking error,
    innov = |ra_raw_px[i] - ra_raw_px[i-1]|. The C++ runtime (HarmonicGuider::predict)
    feeds the Q-net the SAME quantity (m_qFeatRA/Dec) so train and serve match; if you
    change this feature, change it on both sides.

    The Q-net learns to output higher process noise when the frame-to-frame error
    change is large (mechanical event) and lower process noise when it is small
    (steady tracking).

    Loss: MSE against log(innov²), i.e. the Q-net regresses the log-variance of the
    per-frame error change.
    """
    if not TORCH_AVAILABLE:
        if verbose:
            print("  PyTorch not available. Using zero Q-net weights (moderate process noise).")
        return _zero_qnet_weights()

    guided_sessions = [
        s for s in sysid["sessions"]
        if s.get("type") in ("standard_guiding", "closed_loop", "guided")
    ]

    if not guided_sessions:
        if verbose:
            print("  No closed-loop sessions found. Using zero Q-net weights.")
        return _zero_qnet_weights()

    guide_exp = sysid["equipment"].get("guide_exposure_ms", 1000.0) / 1000.0

    # Collect training data: for each frame, compute the innovation using
    # a Kalman filter with fixed Q, then train the Q-net to predict optimal Q
    inputs = []   # [snr_norm, snr_delta_norm, |innov_ra|, |innov_dec|, dt_norm]
    targets = []  # [log_Q_ra, log_Q_dec] — derived from innovation magnitude

    for s in guided_sessions:
        frames = s["frames"]
        if len(frames) < 20:
            continue

        prev_snr = frames[0].get("snr", 30.0)
        prev_ra = frames[0].get("ra_raw_px", 0.0)
        prev_dec = frames[0].get("dec_raw_px", 0.0)

        for i in range(1, len(frames)):
            f = frames[i]
            snr = f.get("snr", 30.0)
            dt = f.get("dt", guide_exp)
            ra = f.get("ra_raw_px", 0.0)
            dec = f.get("dec_raw_px", 0.0)

            snr_delta = snr - prev_snr
            innov_ra = abs(ra - prev_ra)
            innov_dec = abs(dec - prev_dec)

            # Input features (normalized)
            inputs.append([
                snr / 100.0,
                snr_delta / 10.0,
                innov_ra,
                innov_dec,
                dt / 2.0
            ])

            # Target: innovation² gives the optimal Q
            # log_Q = log(innovation² + eps) to keep it in log-scale
            target_q_ra = np.log(max(innov_ra ** 2, 1e-4))
            target_q_dec = np.log(max(innov_dec ** 2, 1e-4))
            targets.append([target_q_ra, target_q_dec])

            prev_snr = snr
            prev_ra = ra
            prev_dec = dec

    if len(inputs) < 50:
        if verbose:
            print(f"  Only {len(inputs)} training samples. Using zero Q-net weights.")
        return _zero_qnet_weights()

    # Train the Q-net MLP
    X = torch.tensor(inputs, dtype=torch.float32)
    Y = torch.tensor(targets, dtype=torch.float32)

    if gpu and torch.cuda.is_available():
        device = torch.device("cuda")
    else:
        device = torch.device("cpu")

    model = QNet().to(device)
    X, Y = X.to(device), Y.to(device)

    optimizer = optim.Adam(model.parameters(), lr=1e-3)
    n_epochs = epochs if epochs else 200
    dataset = TensorDataset(X, Y)
    loader = DataLoader(dataset, batch_size=64, shuffle=True)

    best_loss = float("inf")
    for epoch in range(n_epochs):
        epoch_loss = 0.0
        for xb, yb in loader:
            pred = model(xb)
            loss = nn.functional.mse_loss(pred, yb)
            optimizer.zero_grad()
            loss.backward()
            optimizer.step()
            epoch_loss += loss.item() * len(xb)
        epoch_loss /= len(X)

        if epoch_loss < best_loss:
            best_loss = epoch_loss

        if verbose and (epoch + 1) % 50 == 0:
            print(f"  [Q-net] Epoch {epoch+1}/{n_epochs}: loss={epoch_loss:.4f}")

    if verbose:
        print(f"  [Q-net] Training complete. Best loss: {best_loss:.4f}")

    # Extract weights
    model.cpu()
    state = model.state_dict()
    return {
        "w1": state["fc1.weight"].flatten().tolist(),
        "b1": state["fc1.bias"].tolist(),
        "w2": state["fc2.weight"].flatten().tolist(),
        "b2": state["fc2.bias"].tolist(),
    }


class QNet(nn.Module):
    """5 → 8 (ReLU) → 2 Q-net for adaptive process noise."""
    def __init__(self):
        super().__init__()
        self.fc1 = nn.Linear(5, 8)
        self.fc2 = nn.Linear(8, 2)

    def forward(self, x):
        h = torch.relu(self.fc1(x))
        return self.fc2(h)


def _zero_qnet_weights():
    """Return zero weights → Q-net outputs ~0 → exp(0)=1 → moderate process noise."""
    return {
        "w1": [0.0] * (5 * 8),
        "b1": [0.0] * 8,
        "w2": [0.0] * (8 * 2),
        "b2": [-2.0, -2.0],  # exp(-2) ≈ 0.135 — conservative process noise
    }
