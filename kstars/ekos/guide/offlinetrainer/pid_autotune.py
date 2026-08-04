"""
offline_trainer/pid_autotune.py — recommend base RA/DEC PID guide gains from
pulse_response step-response data, for any mount type.

Offline, calibration-time-only calculation (pid_autotune_plan.md). Reuses the
per-fit P_fit/tau_fit/residual_std/t_arr values pulse_response_fit.fit_pulse_
response() already computes from pulse_response sessions -- no new data
collection, just a second consumer of the same step-response fits. Extracts a
per-axis FOPDT-like model (process gain K, time constant tau, dead time L),
applies a conservative SIMC/IMC-style PI tuning rule, and back-converts the
result into KStars' dimensionless proportional_gain convention via the
calibrated ms_per_arcsec rate recorded in standard_guiding sessions.

This is advisory only. Nothing in KStars or any trainer applies the result
automatically -- callers surface it as recommended_ra/dec_proportional_gain
(+ integral_gain) fields plus a confidence flag, for a human to review before
manually updating Options::rA/dECProportionalGain().

Originally implemented inside train_harmonic.py (the only mount type with
pulse_response data at the time); moved here, unchanged, once WORM_GEAR and
DIRECT_DRIVE gained their own pulse-response phases -- the calculation itself
was never Harmonic-Drive-specific, only the data collection was.

SPDX-License-Identifier: GPL-2.0-or-later
"""

import numpy as np


# Default SIMC design parameter: lambda = max(tau, SIMC_LAMBDA_L_FACTOR * dead_time).
# Larger -> slower/more robust closed loop; smaller -> faster/less margin. Kept as
# an explicit, overridable constant per the plan's "expose lambda as a documented
# knob, not a hidden constant" (pid_autotune_plan.md §3.2).
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
            ps *= bf
    return ps


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



def _step_samples_by_magnitude(sysid: dict, axis: str) -> dict:
    """
    Step amplitude of each pulse response, in pixels, keyed by pulse magnitude.

    Each response is fitted on its own as a step plus a local linear trend and the
    step is read off at the pulse instant. Detrending per curve removes anything
    moving at a steady rate during the window -- drift, periodic error, and the
    residual tracking-rate offset an RA pulse leaves behind -- so the estimate does
    not depend on the opposite-direction pulse having been fired close enough in
    time for those to cancel by subtraction. This mirrors
    AIGuideProtocol::computeAndApplyAxisGain() so the advisory and the gain the
    protocol applies live are computed the same way.
    """
    axis_key = "ra_raw_px" if axis.upper() == "RA" else "dec_raw_px"
    pos_dir, neg_dir = ("EAST", "WEST") if axis.upper() == "RA" else ("NORTH", "SOUTH")

    by_mag, dead_times, first_times, signs = {}, {}, {}, set()
    for s in sysid["sessions"]:
        if s.get("type") != "pulse_response" or s.get("pulse_axis", "").upper() != axis.upper():
            continue
        direction = s.get("pulse_direction", "")
        if direction not in (pos_dir, neg_dir):
            continue
        mag = float(s.get("pulse_magnitude_ms", 0.0))
        frames = s.get("response_frames", [])
        # The pulse lands mid-exposure, so the first frame integrates only part of
        # the motion and does not sit on the post-step trend line.
        if mag <= 0.0 or len(frames) < 6:
            continue
        base_frames = s.get("baseline_frames", [])
        baseline = float(np.mean([f.get(axis_key, 0.0) for f in base_frames])) if base_frames else 0.0
        t = np.array([f.get("t", 0.0) for f in frames], dtype=float)
        pos = np.array([f.get(axis_key, 0.0) for f in frames], dtype=float) - baseline

        # Frames whose exposure overlapped the pulse only integrate part of the motion.
        # At least one is always dropped; at a fast cadence more than one can fall inside.
        first = 1
        while first < len(t) - 4 and t[first] < mag / 1000.0:
            first += 1
        slope, intercept = np.polyfit(t[first:], pos[first:], 1)
        resid = pos[first:] - (intercept + slope * t[first:])
        # Two parameters were fitted, so n-2 is the unbiased residual estimate.
        residual_std = float(np.sqrt(np.sum(resid ** 2) / max(len(resid) - 2, 1)))
        if abs(intercept) < 2.0 * max(residual_std, 1e-6):
            continue  # noise-dominated

        by_mag.setdefault(mag, []).append(abs(intercept))
        dead_times.setdefault(mag, []).append(
            _estimate_dead_time_s(t, pos, max(residual_std, 1e-6)))
        first_times.setdefault(mag, []).append(float(t[0]))
        signs.add((1 if direction == pos_dir else -1) * (1 if intercept > 0 else -1))

    return {"by_mag": by_mag, "dead_times": dead_times,
            "first_times": first_times, "signs": signs}

# Mirrors K_MAGNITUDE_CONSISTENCY_TOLERANCE in aiguideprotocol.cpp.
K_MAGNITUDE_CONSISTENCY_TOLERANCE = 0.25


def _recommend_axis_pid_gain(sysid: dict, axis: str, guide_exp: float,
                             lambda_l_factor: float, verbose: bool) -> dict:
    """
    Derive a recommended base proportional/integral gain for one axis from its
    pulse_response step-response fits, via a conservative SIMC/IMC-style PI rule
    (pid_autotune_plan.md §3.2). Mount-agnostic: works for any mount type whose
    sysid data includes pulse_response sessions for this axis. Returns a dict;
    confidence "unavailable" means the numbers (if present at all) should not
    be used -- most commonly because this mount type/run has no pulse_response
    data yet (Options::aIPIDAutoTune() was off, or this mount class's
    protocol doesn't collect it).
    """
    cal_ms_per_arcsec = _calibration_ms_per_arcsec(sysid, axis)
    if cal_ms_per_arcsec <= 0.0:
        return {"confidence": "unavailable",
                "reason": "no calibrated ms_per_arcsec recorded for this axis (need a standard_guiding session)"}

    pixel_scale = _effective_pixel_scale(sysid)

    steps = _step_samples_by_magnitude(sysid, axis)
    by_mag = steps["by_mag"]
    if not by_mag:
        return {"confidence": "unavailable",
                "reason": "no usable pulse-response step-response fits"}
    if len(steps["signs"]) > 1:
        return {"confidence": "unavailable",
                "reason": "pulse-response signs inconsistent across directions -- fits are noise, not mechanics"}

    n_fits = sum(len(v) for v in by_mag.values())
    # K is arcsec per ms of pulse, so it must not depend on the magnitude used to
    # measure it. Disagreement means the responses are contaminated and no median
    # over the pooled samples would be meaningful.
    k_by_mag = {mag: float(np.median(v)) * pixel_scale / mag for mag, v in by_mag.items()}
    k_lo, k_hi = min(k_by_mag.values()), max(k_by_mag.values())
    detail = ", ".join(f"{m:.0f}ms: {k:.5f}" for m, k in sorted(k_by_mag.items()))
    if len(k_by_mag) < 2:
        return {"confidence": "unavailable",
                "reason": f"only one usable pulse magnitude ({detail}) -- the cross-magnitude check cannot run"}
    if k_lo <= 0.0 or (k_hi - k_lo) / k_lo > K_MAGNITUDE_CONSISTENCY_TOLERANCE:
        return {"confidence": "unavailable",
                "reason": f"process gain disagrees across pulse magnitudes ({detail}) -- responses are contaminated"}

    # The largest pulse has the best signal-to-contamination ratio.
    best_mag = max(by_mag)
    if len(by_mag[best_mag]) < MIN_FITS_FOR_MEDIUM_CONFIDENCE:
        return {"confidence": "unavailable",
                "reason": f"only {len(by_mag[best_mag])} usable fit(s) at {best_mag:.0f}ms"}
    K = k_by_mag[best_mag]                      # arcsec of response per ms of pulse
    L = float(np.median(steps["dead_times"][best_mag]))
    t_first = float(np.median(steps["first_times"][best_mag]))
    tau = max(t_first, L)
    resolution_limited = bool(np.isclose(L, t_first, rtol=0.05))

    lam = max(tau, lambda_l_factor * L)
    Kc = (1.0 / K) * tau / (lam + L)               # ms of pulse per arcsec of error
    tau_i = min(tau, 4.0 * (lam + L))              # SIMC reset time, reported only -- see constant doc above

    proportional_gain = Kc / cal_ms_per_arcsec
    integral_gain = INTEGRAL_GAIN_CONSERVATIVE_FRACTION * proportional_gain
    confidence = "low" if (resolution_limited or n_fits < MIN_FITS_FOR_MEDIUM_CONFIDENCE) else "medium"

    if verbose:
        print(f"  [{axis} PID] K={K:.5f} arcsec/ms  tau={tau:.2f}s  L={L:.2f}s  lambda={lam:.2f}s "
              f"(n={n_fits} fits @ {best_mag:.0f}ms, cal={cal_ms_per_arcsec:.1f}ms/arcsec)")
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
        "n_fits":                     n_fits,
        "resolution_limited":         resolution_limited,
    }


def recommend_pid_gains(sysid: dict, guide_exp: float, verbose: bool,
                        lambda_l_factor: float = SIMC_LAMBDA_L_FACTOR) -> dict:
    """
    Recommend base RA/DEC proportional (+ conservative integral) gains from the
    pulse_response step-response data, via a conservative SIMC/IMC-style PI rule
    (pid_autotune_plan.md §3). Works for any mount type (WORM_GEAR, HARMONIC_DRIVE,
    DIRECT_DRIVE) whose sysid data has pulse_response sessions; returns
    confidence "unavailable" per axis when it doesn't. Calibration-time only,
    never auto-applied -- see this module's docstring for how callers should
    surface the result.
    """
    return {
        "ra":  _recommend_axis_pid_gain(sysid, "RA",  guide_exp, lambda_l_factor, verbose),
        "dec": _recommend_axis_pid_gain(sysid, "DEC", guide_exp, lambda_l_factor, verbose),
    }
