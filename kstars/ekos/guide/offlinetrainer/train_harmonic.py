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


def train_harmonic(sysid: dict,
                   gpu: bool = False,
                   epochs: int = None,
                   verbose: bool = False) -> dict:
    """
    Fit κ/τ spring parameters, detect PE, fit drift, and train Q-net
    for a harmonic drive mount.
    Returns a weights dict compatible with HarmonicGuider::loadWeights().
    """
    eq = sysid["equipment"]
    pixel_scale = eq["pixel_scale_arcsec_per_px"]
    guide_exp   = eq.get("guide_exposure_ms", 1000.0) / 1000.0

    if verbose:
        print(f"\n--- Phase 1: Spring Parameter Fitting ---")

    # ── Step 1: Fit spring parameters from pulse_response sessions ─────────
    kappa_ra, tau_ra   = _fit_spring_params(sysid, "RA",  guide_exp, verbose)
    kappa_dec, tau_dec = _fit_spring_params(sysid, "DEC", guide_exp, verbose)

    if verbose:
        print(f"  κ_ra={kappa_ra:.3f}  τ_ra={tau_ra:.2f}s")
        print(f"  κ_dec={kappa_dec:.3f}  τ_dec={tau_dec:.2f}s")
        print(f"\n--- Phase 2: PE Period Detection ---")

    # ── Step 2: Detect PE period from free-drift data ──────────────────────
    pe_period, pe_amplitude = _estimate_pe(sysid, guide_exp, verbose)

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

    from train_direct_drive import _build_fingerprint
    return {
        "format_version":    "1.0",
        "mount_type":        "HARMONIC_DRIVE",
        "trained_at":        datetime.utcnow().isoformat() + "Z",
        "mount_name":        eq.get("mount_name", "unknown"),
        "pixel_scale":       pixel_scale,
        "model_fingerprint": _build_fingerprint(sysid),
        "physical": {
            "kappa_ra":      float(kappa_ra),
            "tau_ra":        float(tau_ra),
            "kappa_dec":     float(kappa_dec),
            "tau_dec":       float(tau_dec),
            "pe_period":     float(pe_period),
            "pe_amplitude":  float(pe_amplitude),
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

def _fit_spring_params(sysid: dict, axis: str, guide_exp: float, verbose: bool):
    """
    Fit spring constant κ and time constant τ from pulse_response sessions.

    Model: d(t) = P * (1 - κ * exp(-t/τ)) + v*t + c. Fits whose |P| is not
    significantly above the residual noise are skipped.

    Returns: (kappa, tau_seconds)
    """
    # Unmeasured means unmodeled: the default kappa stays 0
    DEFAULTS = (0.0, 1.5)
    pulse_sessions = [
        s for s in sysid["sessions"]
        if s.get("type") == "pulse_response" and s.get("pulse_axis", "").upper() == axis.upper()
    ]

    if not pulse_sessions:
        if verbose:
            print(f"  [{axis}] No pulse_response sessions found. Using defaults (κ=0.2, τ=1.5s)")
        return DEFAULTS

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
            if verbose:
                print(f"  [{axis}] Pulse {pulse_mag}ms {s.get('pulse_direction', '?')}: "
                      f"κ={kappa_fit:.3f}, τ={tau_fit:.2f}s (P={P_fit:.2f}px, noise={residual_std:.2f}px)")

    if not kappas:
        if verbose:
            print(f"  [{axis}] No pulse response measurable above noise "
                  f"({skipped_noise} skipped). Using defaults (κ=0.2, τ=1.5s). "
                  f"Consider larger protocol pulses.")
        return DEFAULTS

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
        return DEFAULTS

    kappa_result = float(np.median(kappas))
    tau_result = float(np.median(taus)) if taus else DEFAULTS[1]

    # A median within ~2% of the fit bounds means the model chased noise/drift, not physics.
    if kappa_result > 0.88 or tau_result > 9.8:
        if verbose:
            print(f"  [{axis}] WARNING: fit pinned at bounds (κ={kappa_result:.3f}, "
                  f"τ={tau_result:.2f}s) — unphysical. Using defaults (κ=0.2, τ=1.5s).")
        return DEFAULTS

    if verbose:
        print(f"  [{axis}] Final: κ={kappa_result:.3f} (from {len(kappas)} fits), "
              f"τ={tau_result:.2f}s")

    return kappa_result, tau_result


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
    pixel_scale = sysid["equipment"].get("pixel_scale_arcsec_per_px", 1.0)
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
    Detect PE period and amplitude via Lomb-Scargle over every usable session.
    Band per series: from 2 cycles per observation span up to Nyquist, so long
    standard-guiding sessions expose the strain-wave fundamental (~300-900s)
    and short free drifts still cover fast components.
    Returns (period_seconds, amplitude_pixels) or (0.0, 0.0) if none significant.
    """
    candidates = _pe_candidate_series(sysid, guide_exp)
    if not candidates:
        if verbose:
            print("  No usable sessions found. PE detection skipped.")
        return 0.0, 0.0

    best = None  # (snr, period, amplitude, label)
    for t_arr, ra_arr, label in candidates:
        span = t_arr[-1] - t_arr[0]
        nyquist = 0.5 / guide_exp
        f_min = max(2.0 / span, 0.002)  # need >= 2 observed cycles
        f_max = min(0.5, nyquist * 0.9)
        if f_min >= f_max or span <= 0:
            continue

        slope, intercept, _, _, _ = scipy.stats.linregress(t_arr, ra_arr)
        ra_detrended = ra_arr - (slope * t_arr + intercept)

        f_search = np.geomspace(f_min, f_max, 4000)
        omega = 2 * np.pi * f_search
        Pxx = scipy.signal.lombscargle(t_arr, ra_detrended, omega, precenter=True)

        peak_idx = np.argmax(Pxx)
        peak_freq = f_search[peak_idx]
        noise_floor = np.median(Pxx)
        snr = Pxx[peak_idx] / (noise_floor + 1e-10)
        amplitude = np.sqrt(4 * Pxx[peak_idx] / len(t_arr))

        at_edge = peak_freq <= f_min * 1.05
        if verbose:
            print(f"  [LS] {label}: span={span:.0f}s, band {1/f_max:.1f}-{1/f_min:.0f}s, "
                  f"peak {1/peak_freq:.1f}s, SNR {snr:.1f}, amp {amplitude:.3f}px"
                  f"{'  [AT BAND EDGE]' if at_edge else ''}")

        if best is None or snr > best[0]:
            best = (snr, 1.0 / peak_freq, amplitude, label, at_edge)

    if best is None or best[0] < 10.0:
        if verbose:
            snr = 0.0 if best is None else best[0]
            print(f"  [LS] PE not significant (best SNR {snr:.1f} < 10.0). Disabling PE states.")
        return 0.0, 0.0

    snr, peak_period, amplitude, label, at_edge = best
    if at_edge:
        # True period is longer than the session can resolve: disable PE, tell the user
        if verbose:
            print(f"  [LS] WARNING: dominant PE ({amplitude:.2f}px, SNR {snr:.0f}) sits at the "
                  f"band edge ({peak_period:.0f}s) — true period is longer than the session "
                  f"can resolve. Collect a session of at least {2.5 * peak_period:.0f}s "
                  f"(free drift or standard guiding) and retrain. Disabling PE states.")
        return 0.0, 0.0

    peak_period = np.clip(peak_period, 1.5, 1500.0)
    amplitude = np.clip(amplitude, 0.01, 50.0)
    if verbose:
        print(f"  [LS] Selected: {peak_period:.1f}s, amp {amplitude:.3f}px (from {label})")

    return float(peak_period), float(amplitude)


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
