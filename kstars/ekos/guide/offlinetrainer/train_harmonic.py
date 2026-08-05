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

# Elastic-windup (kappa/tau) spring fitting is disabled for now -- see the note
# in train_harmonic() below. Re-import this if that fit is ever revisited.
# from pulse_response_fit import fit_pulse_response as _fit_spring_params
from pid_autotune import recommend_pid_gains as _recommend_pid_gains, SIMC_LAMBDA_L_FACTOR


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
        print(f"\n--- Phase 1: Spring Parameter Fitting (disabled -- see note below) ---")

    # ── Step 1: Elastic/spring (kappa/tau) fitting -- commented out for now ────
    # _fit_spring_params() (pulse_response_fit.fit_pulse_response()) has never
    # resolved a spring response above the noise floor on any rig tested so far
    # (pid_autotune_plan.md §9.1): kappa has come back 0.0 on every real run,
    # bounded by the achieved guide-frame cadence being too coarse relative to
    # any plausible spring time constant. Rather than spend the pulse_response
    # phase's frame budget on a fit that has never once produced a usable
    # result, this is disabled for now and left as a future-exploration item
    # (e.g. revisit if a rig ever shows a resolvable spring, or add a separate
    # opt-in toggle specifically for elastic-windup modeling rather than always
    # attempting it as a side effect of PID auto-tune's data collection).
    # The same pulse_response sessions still fully serve PID auto-tune (Step 1b
    # below), which never depended on this fit succeeding.
    kappa_ra, tau_ra   = 0.0, 1.5
    kappa_dec, tau_dec = 0.0, 1.5
    # kappa_ra, tau_ra   = _fit_spring_params(sysid, "RA",  guide_exp, verbose)
    # kappa_dec, tau_dec = _fit_spring_params(sysid, "DEC", guide_exp, verbose)

    if verbose:
        print(f"  κ_ra={kappa_ra:.3f}  τ_ra={tau_ra:.2f}s (fit disabled, using default)")
        print(f"  κ_dec={kappa_dec:.3f}  τ_dec={tau_dec:.2f}s (fit disabled, using default)")
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
    (drift_ra, drift_dec, d_polar, k_ref, k_ref_dec,
     fit_alts, fit_qs) = _fit_drift_params(sysid, guide_exp, verbose)

    # Geometry the drift/refraction fit is valid for (runtime clamps to it) — from the
    # sessions the fit actually used, not everything recorded.
    fit_alt_min = min(fit_alts) if fit_alts else 35.0
    fit_alt_max = max(fit_alts) if fit_alts else 65.0
    fit_par_min = min(fit_qs) if fit_qs else -90.0
    fit_par_max = max(fit_qs) if fit_qs else 90.0

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
            "fit_par_min":   float(fit_par_min),
            "fit_par_max":   float(fit_par_max),
        },
        "qnet": qnet_weights,
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

    # A rate from a very short window is noise, not a measurement: at a ~0.5" noise
    # floor a 25s sample has ~±0.07 "/s slope uncertainty — one such point silently
    # set k_ref_dec on real data (see DEC_OFFSET_ROOT_CAUSE.md).
    MIN_FIT_SPAN_S = 120.0

    ra_rates = []
    dec_rates = []
    cos2_alts = []  # 1/cos²(alt) for each session
    q_factors = []  # sin(q)/cos²(alt) for each session
    q_angles = []
    fit_alts = []   # altitudes of the sessions actually used (runtime clamps to these)

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
        if span < MIN_FIT_SPAN_S:
            if verbose:
                print(f"  [drift] skipping {s.get('session_id', '?')}: {span:.0f}s span — "
                      f"too short to constrain a rate (need >= {MIN_FIT_SPAN_S:.0f}s)")
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
        fit_alts.append(alt)

    if not ra_rates:
        return 0.0, 0.0, 0.0, 0.0, 0.0, [], []

    # RA: rate = k_ref / cos²(alt) + drift_ra_extra. Without real altitude spread the
    # regression divides by ~zero variance and returns garbage — fall back to the mean.
    alt_range = max(fit_alts) - min(fit_alts) if len(fit_alts) >= 2 else 0.0
    if len(ra_rates) >= 2 and alt_range >= 10.0:
        k_ref, drift_ra = scipy.stats.linregress(cos2_alts, ra_rates)[:2]
    else:
        if verbose and len(ra_rates) >= 2:
            print(f"  [RA] altitude range: {alt_range:.1f}° < 10° — k_ref = 0, using mean rate")
        k_ref = 0.0
        drift_ra = float(np.mean(ra_rates))

    # DEC: rate = d_polar + k_ref_dec * sin(q)/cos²(alt)
    q_range = max(q_angles) - min(q_angles) if len(q_angles) >= 2 else 0.0

    if len(dec_rates) >= 2 and q_range >= 20.0:
        k_ref_dec, d_polar = scipy.stats.linregress(q_factors, dec_rates)[:2]
        if verbose:
            print(f"  [DEC] Parallactic angle range: {q_range:.1f}° (sufficient)")
    elif len(dec_rates) >= 2:
        # No geometric spread means sin(q)/cos²(alt) is unconstrained; borrowing k_ref
        # would apply an RA coefficient to DEC geometry it was never fitted at.
        if verbose:
            print(f"  [DEC] Parallactic angle range: {q_range:.1f}° < 20° — k_ref_dec = 0")
        k_ref_dec = 0.0
        d_polar = float(np.mean(dec_rates))
    else:
        d_polar = dec_rates[0] if dec_rates else 0.0
        k_ref_dec = 0.0

    drift_dec = 0.0  # Absorbed into d_polar

    return (float(drift_ra), float(drift_dec), float(d_polar), float(k_ref), float(k_ref_dec),
            fit_alts, q_angles)


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


if TORCH_AVAILABLE:
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
