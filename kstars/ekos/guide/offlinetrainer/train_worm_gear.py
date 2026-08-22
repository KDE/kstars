#!/usr/bin/env python3
"""
offline_trainer/train_worm_gear.py — PINN + residual MLP trainer for worm-gear mounts.

Phase 1: FFT-based period and amplitude estimation from free-drift data.
Phase 2: Physics parameter fitting (refraction, polar drift).
Phase 3: Residual MLP training (PyTorch) on closed-loop sessions.

SPDX-License-Identifier: GPL-2.0-or-later
"""

import json
import numpy as np
import scipy.signal
import scipy.stats
import scipy.optimize
from datetime import datetime
import copy
import sys

try:
    import torch
    import torch.nn as nn
    import torch.optim as optim
    from torch.utils.data import TensorDataset, DataLoader
    TORCH_AVAILABLE = True
except ImportError:
    TORCH_AVAILABLE = False

from pid_autotune import recommend_pid_gains, SIMC_LAMBDA_L_FACTOR



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

def train_worm_gear(sysid: dict,
                    gpu: bool = False,
                    epochs: int = None,
                    verbose: bool = False,
                    pid_lambda_factor: float = SIMC_LAMBDA_L_FACTOR,
                    dropout_p: float = None,
                    weight_decay: float = None) -> dict:
    """
    Train the PINN + residual MLP for a worm-gear mount.
    Returns a weights dict compatible with WormGearGuider::loadWeights().

    Also computes an advisory "pid_autotune" PID gain recommendation from any
    pulse_response sessions present (see pid_autotune.py / pid_autotune_plan.md
    §8). Worm-gear mounts are exactly the class where backlash on DEC direction
    reversal is a well-known effect -- it should show up directly as dead time
    (L) in the FOPDT model. Returns confidence "unavailable" per axis if this
    sysid run has no pulse_response data (Options::aIPIDAutoTune() was
    off, or an older protocol run predates this mount type collecting it).
    """
    if not TORCH_AVAILABLE:
        print("[ERROR] PyTorch is required to train the WormGearGuider MLP.")
        print("Please install it: pip install torch")
        sys.exit(1)

    eq = sysid["equipment"]
    pixel_scale = _effective_pixel_scale(sysid)
    guide_exp   = eq.get("guide_exposure_ms", 2000.0) / 1000.0

    if verbose:
        print(f"\n--- Phase 1: Physics Parameter Estimation ---")
    
    # ── Step 1: Estimate PE period from FFT of free-drift RA error ─────────
    pe_period, pe_amplitude = _estimate_pe_from_fft(sysid, guide_exp, verbose)

    # ── Step 2: Estimate refraction coefficient from altitude-variant drift ─
    k_ref, d_ra_extra = _estimate_refraction(sysid, guide_exp, verbose)

    # ── Step 3: Estimate polar drift rate from DEC free drift ───────────────
    d_polar, k_ref_dec = _estimate_dec_drift(sysid, guide_exp, k_ref, verbose)

    if verbose:
        print(f"\n[Physics Parameters]")
        print(f"  PE period:    {pe_period:.1f} s")
        print(f"  PE amplitude: {pe_amplitude:.3f} px = {pe_amplitude * pixel_scale:.3f} arcsec")
        print(f"  k_ref:        {k_ref:.6e} px/s")
        print(f"  d_polar:      {d_polar:.6e} px/s")
        print(f"  k_ref_dec:    {k_ref_dec:.6e} px/s")

    # ── Step 4: Train residual MLP ───────────────────────────────────────────
    if verbose:
        print(f"\n--- Phase 2: Residual MLP Training ---")
        
    mlp_weights = _train_residual_mlp(sysid, pe_period, pe_amplitude,
                                      k_ref, d_ra_extra, d_polar, k_ref_dec, gpu, epochs, verbose,
                                      dropout_p, weight_decay)

    # We need a build_fingerprint function similar to train_direct_drive
    def _build_fingerprint(sys):
        fp = sys.get("model_fingerprint", {})
        return {
            "guide_exposure_s": fp.get("guide_exposure_s", guide_exp),
            "guide_binning": fp.get("guide_binning", "1x1"),
            "ra_proportional_gain": fp.get("ra_proportional_gain", 1.0),
            "dec_proportional_gain": fp.get("dec_proportional_gain", 1.0),
            "ra_pulse_algorithm": fp.get("ra_pulse_algorithm", 0),
            "dec_pulse_algorithm": fp.get("dec_pulse_algorithm", 0),
        }

    # Advisory PID auto-tune recommendation from pulse_response sessions, if any
    # (see pid_autotune.py). Not applied automatically -- a human reviews and
    # manually updates Options::rA/dECProportionalGain() in KStars.
    pid_autotune = recommend_pid_gains(sysid, guide_exp, verbose, pid_lambda_factor)

    def _recommended(axis_result, field):
        return axis_result[field] if axis_result["confidence"] != "unavailable" else None

    return {
        "format_version":    "1.0",
        "mount_type":        "WORM_GEAR",
        "trained_at":        datetime.utcnow().isoformat() + "Z",
        "mount_name":        eq.get("mount_name", "unknown"),
        "pixel_scale":       pixel_scale,
        "model_fingerprint": _build_fingerprint(sysid),
        # Advisory PID auto-tune recommendation -- see train_worm_gear()'s docstring.
        # recommended_* fields are None when confidence is "unavailable" (don't apply).
        "recommended_ra_proportional_gain":  _recommended(pid_autotune["ra"], "proportional_gain"),
        "recommended_ra_integral_gain":      _recommended(pid_autotune["ra"], "integral_gain"),
        "recommended_dec_proportional_gain": _recommended(pid_autotune["dec"], "proportional_gain"),
        "recommended_dec_integral_gain":     _recommended(pid_autotune["dec"], "integral_gain"),
        "pid_autotune": pid_autotune,
        "physics": {
            "pe_amplitude": float(pe_amplitude),
            "pe_period":    float(pe_period),
            "k_ref":        float(k_ref),
            "d_ra_extra":   float(d_ra_extra),
            "d_polar":      float(d_polar),
            "k_ref_dec":    float(k_ref_dec),
            "fit_alt_min":  float(min((s.get("altitude_deg", 45.0) for s in sysid["sessions"]
                                       if s.get("type") == "free_drift"), default=35.0)),
            "fit_alt_max":  float(max((s.get("altitude_deg", 45.0) for s in sysid["sessions"]
                                       if s.get("type") == "free_drift"), default=65.0))
        },
        "normalization": {
            "alt_scale":       90.0,
            "snr_scale":       100.0,
            "pulse_scale_ms":  1000.0,
            "dt_scale":        2.0,
        },
        "mlp": mlp_weights,
    }


def pulse_correction_px(pulse_ms, cal_rate_ms_per_arcsec, pixel_scale):
    """Accurately convert actual logged pulse back into pixel drift using calibration rates."""
    if pulse_ms == 0.0 or cal_rate_ms_per_arcsec <= 0.0:
        return 0.0
    # pulse_ms / (ms/arcsec) = arcsec. arcsec / (arcsec/px) = px
    return (pulse_ms / cal_rate_ms_per_arcsec) / pixel_scale


def _estimate_pe_from_fft(sysid: dict, guide_exp: float, verbose: bool):
    """
    Estimate worm PE period and amplitude from FFT of free-drift RA error.
    """
    known_period = sysid.get("equipment", {}).get("pe_period", None)
    
    free_drift_sessions = [s for s in sysid["sessions"] if s["type"] == "free_drift"]
    if not free_drift_sessions:
        if verbose: print("[WARNING] No free_drift sessions found. Using default PE.")
        return 480.0, 1.0

    all_ra = []
    # Use the longest free drift session for the best FFT resolution
    longest_session = max(free_drift_sessions, key=lambda s: len(s["frames"]))
    
    frames = longest_session["frames"]
    t_vals = []
    ra_vals = []
    
    # Extract data
    t = 0.0
    for f in frames:
        t += f.get("dt", guide_exp)
        t_vals.append(t)
        ra_vals.append(f["ra_raw_px"])
        
    t_vals = np.array(t_vals)
    ra_vals = np.array(ra_vals)
    
    if len(t_vals) < 20:
        if verbose: print("[WARNING] Free drift session too short. Using default PE.")
        return 480.0, 1.0

    # Detrend to remove linear drift (refraction/polar alignment)
    slope, intercept, _, _, _ = scipy.stats.linregress(t_vals, ra_vals)
    ra_detrended = ra_vals - (slope * t_vals + intercept)

    # Compute Lomb-Scargle Periodogram
    # We expect periods in 33s - 1000s range (0.001 - 0.03 Hz)
    f_valid = np.linspace(0.001, 0.03, 2000)
    omega = 2 * np.pi * f_valid
    # scipy lombscargle takes angular frequencies
    Pxx_valid = scipy.signal.lombscargle(t_vals, ra_detrended, omega, precenter=True)

    if len(f_valid) == 0:
        return 480.0, 1.0

    peak_idx = np.argmax(Pxx_valid)
    best_f = f_valid[peak_idx]
    best_period = 1.0 / best_f

    # A worm-gear PE waveform is rarely a pure sinusoid (eccentricity + gear-mesh
    # harmonics), so its periodogram routinely has real, comparably-strong power at
    # period/2, period/3, etc. A bare argmax has no way to prefer the true fundamental
    # over a harmonic that happens to carry more power in one particular session --
    # confirmed 2026-08-21 on a real EQ8 (known worm period 198s): argmax picked 96.1s
    # (power 3.92), while a peak at 201.0s -- 86% of the winning power, i.e. nowhere
    # near noise -- sat right next to it, itself accompanied by a weaker one at 48.6s
    # (~201/4). That's a textbook harmonic series with a ~198-201s fundamental, not
    # three independent phenomena.
    #
    # "Comparably strong" alone is NOT enough to trust walking to a longer period,
    # though -- caught this against a second real dataset from the SAME mount (2026-08-18,
    # a night whose 191.5s estimate was already close to the true period): an early
    # version of this walk, using only the power-ratio test below, took that good 191.5s
    # and walked it to a spurious 387.4s (~191.5*2, 85% as strong). The difference between
    # that failure and tonight's correct 96.1s->201.0s walk is cycle count: 201.0s across a
    # 602s span is 3.0 full cycles (a periodogram can actually resolve that), while 387.4s
    # across a 628s span is only 1.6 -- barely more than one cycle, which no periodogram
    # can distinguish from an arbitrary residual hump. Requiring >=2.5 resolved cycles
    # before accepting a longer candidate is what makes this walk trustworthy on a mount
    # whose true period is NOT already known (the general case for most rigs, not just
    # this one) rather than a heuristic tuned to get tonight's specific answer right.
    span = float(t_vals[-1] - t_vals[0])
    local_max = [i for i in range(1, len(Pxx_valid) - 1)
                 if Pxx_valid[i] > Pxx_valid[i - 1] and Pxx_valid[i] > Pxx_valid[i + 1]]
    candidates = [(1.0 / f_valid[i], Pxx_valid[i]) for i in local_max]

    cur_period, cur_power = best_period, Pxx_valid[peak_idx]
    for _ in range(3):  # bounded walk: fundamental, its parent, its parent's parent
        best_match = None
        for n in (2, 3, 4):
            target = cur_period * n
            match = min(candidates, key=lambda c: abs(c[0] - target), default=None)
            if (match is not None and abs(match[0] - target) / target < 0.07
                    and match[1] >= 0.4 * cur_power
                    and span / match[0] >= 2.5):
                # Prefer the largest valid n found this pass -- it's the most direct
                # step toward the fundamental available from the current candidate.
                if best_match is None or n > best_match[2]:
                    best_match = (match[0], match[1], n)
        if best_match is None:
            break
        if verbose:
            print(f"  [FFT] {cur_period:.1f}s (power {cur_power:.3f}) looks like the "
                  f"1/{best_match[2]} harmonic of {best_match[0]:.1f}s (power {best_match[1]:.3f}, "
                  f"{100 * best_match[1] / cur_power:.0f}% as strong, "
                  f"{span / best_match[0]:.1f} cycles resolved) -- preferring the longer period")
        cur_period, cur_power = best_match[0], best_match[1]
    best_period = cur_period
    best_f = 1.0 / best_period

    # A weak peak means no usable PE line; the argmax is noise. Judged against the
    # spectrum's overall noise floor (median power), not "everything outside a narrow
    # band around the peak" -- that comparison breaks specifically when the harmonic
    # walk above did its job, since a real fundamental's own harmonics are still
    # outside that narrow band and can legitimately out-power it (confirmed on the same
    # EQ8 data this walk was built from: 201.0s power 3.38 vs its own 96.1s 2nd harmonic
    # at 3.92 -- both real signal, not noise, so comparing against the harmonic instead
    # of the noise floor called a strong, correctly-identified fundamental "unreliable").
    noise_floor = np.median(Pxx_valid) + 1e-10
    dominance = cur_power / noise_floor
    if dominance < 10.0:
        print(f"  [FFT] WARNING: no dominant PE line (peak/noise-floor ratio {dominance:.2f}) -- "
              f"period {best_period:.1f}s is unreliable; free drift may not have truly drifted.")

    if known_period is not None and known_period > 0:
        if verbose:
            print(f"  [FFT] Found known PE period override in sysid: {known_period:.1f} s")
        best_period = known_period

    # Power spectrum amplitude approximation for LS, from the final selected period's
    # own power (not the original argmax's, which may be a different harmonic now).
    best_amp = np.sqrt(4 * cur_power / len(t_vals))
    
    if verbose:
        print(f"  [FFT] Analyzed {len(t_vals)} free drift frames (duration: {t_vals[-1]:.1f}s)")
        print(f"  [FFT] Found peak at {best_f:.5f} Hz -> Period: {best_period:.1f} s")
        print(f"  [FFT] Estimated amplitude: {best_amp:.3f} px")
        
    # Sanity bounds
    best_period = np.clip(best_period, 100.0, 1000.0)
    best_amp = np.clip(best_amp, 0.1, 20.0)
    
    return float(best_period), float(best_amp)


def _estimate_refraction(sysid: dict, guide_exp: float, verbose: bool):
    """
    Estimate k_ref and d_ra_extra from altitude-variant RA drift.
    """
    free_drift_sessions = [s for s in sysid["sessions"] if s["type"] == "free_drift"]
    
    rates = []
    cos_alts = []
    
    for s in free_drift_sessions:
        frames = s["frames"]
        if len(frames) < 10: continue
        
        alt = s.get("altitude_deg", 45.0)
        t = 0.0
        t_vals, ra_vals = [], []
        for f in frames:
            t += f.get("dt", guide_exp)
            t_vals.append(t)
            ra_vals.append(f["ra_raw_px"])
            
        slope, _, _, _, _ = scipy.stats.linregress(t_vals, ra_vals)
        rates.append(slope)
        cos_alt = max(abs(np.cos(np.radians(alt))), 1e-4)
        cos_alts.append(1.0 / (cos_alt**2))
        
    if len(rates) < 2:
        if rates:
            return 0.0, rates[0]
        return 0.0, 0.0
        
    # Fit: rate = k_ref * sec^2(alt) + d_ra_extra
    slope, intercept, r_value, _, _ = scipy.stats.linregress(cos_alts, rates)
    
    if verbose:
        print(f"  [Refraction] Fit over {len(rates)} sessions. R^2: {r_value**2:.3f}")

    if r_value**2 < 0.2:
        if verbose:
            print(f"  [Refraction] WARNING: R^2 is too low. Data is too noisy to determine polar drift. Defaulting to 0.")
        return 0.0, float(np.mean(rates))
        
    return float(slope), float(intercept)


def _estimate_dec_drift(sysid: dict, guide_exp: float, k_ref_fallback: float, verbose: bool):
    """
    Estimate polar alignment DEC drift rate and DEC refraction coefficient.
    drift_dec = d_polar + k_ref_dec * sin(q) / cos²(alt)
    """
    free_drift_sessions = [s for s in sysid["sessions"] if s["type"] == "free_drift"]
    rates = []
    refraction_factors = []
    q_angles = []
    
    for s in free_drift_sessions:
        frames = s["frames"]
        if len(frames) < 10: continue
        
        alt = s.get("altitude_deg", 45.0)
        
        t = 0.0
        t_vals, dec_vals = [], []
        q_sum = 0.0
        for f in frames:
            t += f.get("dt", guide_exp)
            t_vals.append(t)
            dec_vals.append(f["dec_raw_px"])
            q_sum += f.get("parallactic_angle_deg", 0.0)
            
        slope, _, _, _, _ = scipy.stats.linregress(t_vals, dec_vals)
        rates.append(slope)
        
        avg_q_deg = q_sum / len(frames)
        q_angles.append(avg_q_deg)
        
        q_rad = np.radians(avg_q_deg)
        alt_rad = np.radians(alt)
        cos_alt = max(abs(np.cos(alt_rad)), 1e-4)
        ref_factor = np.sin(q_rad) / (cos_alt**2)
        refraction_factors.append(ref_factor)
        
    if not rates:
        return 0.0, 0.0
        
    # Check if parallactic angle variation is sufficient
    q_range = max(q_angles) - min(q_angles) if q_angles else 0.0
    if q_range < 20.0:
        if verbose:
            print(f"  [Refraction DEC] WARNING: Parallactic angle coverage insufficient ({q_range:.1f}° < 20°).")
            print(f"  [Refraction DEC] Falling back to k_ref_dec = k_ref.")
        
        # d_polar = avg(rate - k_ref_fallback * ref_factor)
        d_polar = float(np.mean([r - k_ref_fallback * ref for r, ref in zip(rates, refraction_factors)]))
        return d_polar, k_ref_fallback
        
    # Fit: rate = k_ref_dec * (sin(q)/cos^2(alt)) + d_polar
    slope, intercept, r_value, _, _ = scipy.stats.linregress(refraction_factors, rates)
    
    if verbose:
        print(f"  [Refraction DEC] Fit over {len(rates)} sessions. R^2: {r_value**2:.3f}")
        
    return float(intercept), float(slope)

# --- PyTorch MLP ---

# Regularization defaults. Both are training-time-only knobs: they change how the fixed
# 15->32->16->2 architecture is fit, not its shape, so the exported w1/b1/w2/b2/w_out/b_out
# arrays stay the same size and WormGearGuider::loadWeights() (which hardcodes those
# dimensions) needs no changes. Prefer raising these over shrinking hidden-layer width when
# samples-per-parameter is low -- shrinking the architecture *does* require updating the C++
# loader's hardcoded matrix dimensions and shipping a new KStars build.
DEFAULT_DROPOUT_P = 0.2
DEFAULT_WEIGHT_DECAY = 1e-3


class ResidualMLP(nn.Module):
    def __init__(self, dropout_p: float = DEFAULT_DROPOUT_P):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(15, 32),
            nn.ReLU(),
            nn.Dropout(dropout_p),
            nn.Linear(32, 16),
            nn.ReLU(),
            nn.Dropout(dropout_p),
            nn.Linear(16, 2)
        )

    def forward(self, x):
        return self.net(x)

def _physics_drift(t, alt_deg, q_deg, A_pos, T, phi, k_ref, d_ra_extra, d_polar, k_ref_dec, dt):
    """Compute physical drift over dt."""
    alt_rad = np.radians(alt_deg)
    q_rad = np.radians(q_deg)
    cos_alt = max(abs(np.cos(alt_rad)), 1e-4)
    # RA rate derivative of A_pos * sin(2pi*t/T + phi)
    ra_rate = A_pos * (2 * np.pi / T) * np.cos(2 * np.pi * t / T + phi) + k_ref / (cos_alt**2) + d_ra_extra
    dec_rate = d_polar + k_ref_dec * np.sin(q_rad) / (cos_alt**2)
    return ra_rate * dt, dec_rate * dt

def _build_training_dataset(sysid, pe_period, pe_amplitude, k_ref, d_ra_extra, d_polar, k_ref_dec, verbose):
    """Build X, Y tensors for the MLP from standard_guiding sessions."""
    guided_sessions = [s for s in sysid["sessions"] if s["type"] == "standard_guiding"]
    
    X_all = []
    Y_all = []
    W_all = []  # per-sample [ra, dec] loss weights; dec=0 drops contaminated supervision
    
    pixel_scale = _effective_pixel_scale(sysid)
    
    # Track saturation stats
    total_frames = 0
    saturated_frames = 0
    
    # Typical values for normalization
    median_dt = 2.0
    
    for s_idx, s in enumerate(guided_sessions):
        frames = s["frames"]
        if len(frames) < 2: continue
        
        alt_deg = s.get("altitude_deg", 45.0)
        alt_norm = alt_deg / 90.0
        
        # Load calibration rates (fallback to large number to prevent division by zero)
        ra_cal = s.get("ra_ms_per_arcsec", 1000.0)
        dec_cal = s.get("dec_ms_per_arcsec", 1000.0)

        # DEC rate far below RA means the calibration ate backlash; drop its DEC targets.
        dec_suspect = ("dec_ms_per_arcsec" in s and "ra_ms_per_arcsec" in s
                       and dec_cal > 2.0 * ra_cal)
        if dec_suspect:
            print(f"  [Session {s_idx}] WARNING: dec_ms_per_arcsec={dec_cal:.0f} vs ra={ra_cal:.0f} "
                  f"-- backlash-poisoned DEC calibration, dropping this session's DEC targets.")
        
        max_ra_pulse_ms = s.get("max_pulse_ra_arcsec", 2.5) * ra_cal
        max_dec_pulse_ms = s.get("max_pulse_dec_arcsec", 2.5) * dec_cal
        
        # Build time array and extract drift for phase estimation
        t_arr = np.zeros(len(frames) - 1)
        dt_arr = np.zeros(len(frames) - 1)
        drift_ra_arr = np.zeros(len(frames) - 1)
        t_accum = 0.0
        for i in range(len(frames) - 1):
            f1, f2 = frames[i], frames[i+1]
            dt = f2.get("dt", 2.0)
            t_accum += dt
            t_arr[i] = t_accum
            dt_arr[i] = dt
            drift_ra_arr[i] = f2["ra_raw_px"] - f1["ra_raw_px"] + pulse_correction_px(
                f1.get("ra_pulse_ms", 0.0), ra_cal, pixel_scale)
        
        # Fit phi using drift-based method (matches what simulator and C++ RLS will do)
        alt_rad = np.radians(alt_deg)
        # cos_alt is a scalar constant per session (altitude doesn't change within a session).
        # Computed once outside the optimizer closure for efficiency and zenith safety.
        cos_alt = max(abs(np.cos(alt_rad)), 1e-4)
        def loss_fn(phi_val):
            ra_rate = pe_amplitude * (2 * np.pi / pe_period) * np.cos(2 * np.pi * t_arr / pe_period + phi_val) + k_ref / (cos_alt**2) + d_ra_extra
            pred_drift = ra_rate * dt_arr
            return np.sum((pred_drift - drift_ra_arr)**2)
            
        res = scipy.optimize.minimize(loss_fn, x0=[0.0], bounds=[(-np.pi, np.pi)])
        phi_session = res.x[0]
        
        if verbose:
            print(f"  [Session {s_idx}] Fitted PE phase: {phi_session:.3f} rad")
            
        # Now construct the frame-by-frame data
        ra_err = np.array([f["ra_raw_px"] for f in frames])
        session_rms = np.std(ra_err)
        
        t_accum = 0.0
        for i in range(len(frames) - 1):
            f1, f2 = frames[i], frames[i+1]
            dt = f2.get("dt", median_dt)
            t_accum += dt
            
            # Quality filters
            total_frames += 1
            if f1.get("error_code", 0) != 0 or f1.get("snr", 100) < 10:
                continue
            if dt > 3 * median_dt:
                continue
            if abs(f1["ra_raw_px"]) > 5 * session_rms:
                continue
                
            # Pulse Saturation Filter
            # If the pulse is within 2ms of the calculated maximum, it was clamped.
            # We drop these frames because they represent abnormal struggle (wind/snag/spike),
            # and we only want to train the network on the mount's repeatable mechanical behavior.
            pulse_ra = abs(f1.get("ra_pulse_ms", 0.0))
            pulse_dec = abs(f1.get("dec_pulse_ms", 0.0))
            if (max_ra_pulse_ms > 0 and pulse_ra >= max_ra_pulse_ms - 2.0) or \
               (max_dec_pulse_ms > 0 and pulse_dec >= max_dec_pulse_ms - 2.0):
                saturated_frames += 1
                continue
                
            snr_norm = f1.get("snr", 20.0) / 100.0
            last_ra_pulse_norm = f1.get("ra_pulse_ms", 0.0) / 1000.0
            last_dec_pulse_norm = f1.get("dec_pulse_ms", 0.0) / 1000.0
            dt_norm = dt / median_dt
            
            # Use parallactic angle from frame if available, else 0
            q_deg = f1.get("parallactic_angle_deg", 0.0)
            parallactic_angle_norm = q_deg / 180.0
            
            # Physics prediction of the drift
            phys_ra_drift, phys_dec_drift = _physics_drift(
                t_accum, alt_deg, q_deg, pe_amplitude, pe_period, phi_session, k_ref, d_ra_extra, d_polar, k_ref_dec, dt
            )
            
            # Hardcoded Pier Side based on UI phase sequence: first 2 are West, 3rd is East
            pier_side_norm = -1.0 if s_idx < 2 else 1.0
            
            # Input features: [alt_norm, sin_phase, cos_phase, sin2, cos2, sin3, cos3, sin4, cos4, snr_norm, last_ra_pulse_norm, last_dec_pulse_norm, dt_norm, q_norm, pier_side_norm]
            phase = 2 * np.pi * t_accum / pe_period + phi_session
            x = [
                alt_norm,
                np.sin(phase),
                np.cos(phase),
                np.sin(2.0 * phase),
                np.cos(2.0 * phase),
                np.sin(3.0 * phase),
                np.cos(3.0 * phase),
                np.sin(4.0 * phase),
                np.cos(4.0 * phase),
                snr_norm,
                last_ra_pulse_norm,
                last_dec_pulse_norm,
                dt_norm,
                parallactic_angle_norm,
                pier_side_norm
            ]
            
            # Isolate mechanical drift by removing the pulse effect
            if f1.get("ra_pulse_ms", 0.0) == 0.0:
                true_drift_ra = f2["ra_raw_px"] - f1["ra_raw_px"]
            else:
                # KStars emits pulseRA with opposite sign convention to pulseDEC.
                # A positive RA pulse moves the mount negative. Thus pulse_effect = -pulse_correction_px.
                # true_drift = observed - pulse_effect = observed + pulse_correction_px
                true_drift_ra = f2["ra_raw_px"] - f1["ra_raw_px"] + pulse_correction_px(
                    f1.get("ra_pulse_ms", 0.0), ra_cal, pixel_scale)

            if f1.get("dec_pulse_ms", 0.0) == 0.0:
                true_drift_dec = f2["dec_raw_px"] - f1["dec_raw_px"]
            else:
                true_drift_dec = f2["dec_raw_px"] - f1["dec_raw_px"] - pulse_correction_px(
                    f1.get("dec_pulse_ms", 0.0), dec_cal, pixel_scale)
            
            target_ra = true_drift_ra - phys_ra_drift
            target_dec = true_drift_dec - phys_dec_drift
            
            X_all.append(x)
            Y_all.append([target_ra, target_dec])
            W_all.append([1.0, 0.0 if dec_suspect else 1.0])
            
    if total_frames > 0:
        sat_pct = (saturated_frames / total_frames) * 100.0
        if verbose:
            print(f"  [Dataset] Filtered {saturated_frames}/{total_frames} ({sat_pct:.1f}%) frames due to pulse saturation.")
        if sat_pct > 10.0:
            print(f"\n[WARNING] {sat_pct:.1f}% of your guiding frames hit the Max Pulse limit!")
            print("Your mount is constantly struggling. Please increase your Max Pulse setting in")
            print("the Guide Options, or check your polar alignment and balance.\n")

    return np.array(X_all, dtype=np.float32), np.array(Y_all, dtype=np.float32), np.array(W_all, dtype=np.float32)

def _train_residual_mlp(sysid, pe_period, pe_amplitude, k_ref, d_ra_extra, d_polar, k_ref_dec, gpu, epochs, verbose,
                         dropout_p=None, weight_decay=None) -> dict:
    epochs = epochs or 300
    dropout_p = DEFAULT_DROPOUT_P if dropout_p is None else dropout_p
    weight_decay = DEFAULT_WEIGHT_DECAY if weight_decay is None else weight_decay

    X_np, Y_np, W_np = _build_training_dataset(sysid, pe_period, pe_amplitude, k_ref, d_ra_extra, d_polar, k_ref_dec, verbose)
    if len(X_np) < 50:
        if verbose: print("[WARNING] Insufficient data for MLP training. Returning zero weights.")
        return _zero_weights()

    n_params = 15 * 32 + 32 + 32 * 16 + 16 + 16 * 2 + 2  # 1074, must match the fixed architecture
    samples_per_param = len(X_np) / n_params
    if verbose:
        print(f"  [MLP] {len(X_np)} samples / {n_params} params = {samples_per_param:.2f} samples/param "
              f"(dropout={dropout_p}, weight_decay={weight_decay})")
    if samples_per_param < 1.0:
        print(f"  [WARNING] {samples_per_param:.2f} samples/param is below the ~1:1 rule of thumb -- "
              "relying on dropout/weight_decay to control overfitting since architecture size is fixed "
              "by the C++ inference engine. Collect more standard_guiding sessions if you can.")

    # Simple split (last 20% is validation)
    split_idx = int(0.8 * len(X_np))
    X_train, Y_train, W_train = torch.tensor(X_np[:split_idx]), torch.tensor(Y_np[:split_idx]), torch.tensor(W_np[:split_idx])
    X_val, Y_val, W_val = torch.tensor(X_np[split_idx:]), torch.tensor(Y_np[split_idx:]), torch.tensor(W_np[split_idx:])

    device = torch.device("cuda" if gpu and torch.cuda.is_available() else "cpu")
    model = ResidualMLP(dropout_p=dropout_p).to(device)
    X_train, Y_train, W_train = X_train.to(device), Y_train.to(device), W_train.to(device)
    X_val, Y_val, W_val = X_val.to(device), Y_val.to(device), W_val.to(device)

    optimizer = optim.Adam(model.parameters(), lr=1e-3, weight_decay=weight_decay)
    # MSE with per-sample [ra, dec] weights so contaminated DEC targets carry no gradient.
    criterion = lambda pred, target, w: ((pred - target) ** 2 * w).sum() / w.sum().clamp_min(1.0)
    scheduler = optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=epochs)
    
    best_val_loss = float('inf')
    best_state = None
    
    if verbose:
        print(f"  [MLP] Training on {len(X_train)} samples, validating on {len(X_val)}")
        print(f"  [MLP] Architecture: 15 -> 32 -> 16 -> 2 (Pier Side included)")
        print(f"  [MLP] Epochs: {epochs}, Device: {device}")
        
    for epoch in range(epochs):
        model.train()
        optimizer.zero_grad()
        pred = model(X_train)
        loss = criterion(pred, Y_train, W_train)
        loss.backward()
        
        # Gradient clipping
        torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
        optimizer.step()
        scheduler.step()
        
        model.eval()
        with torch.no_grad():
            val_pred = model(X_val)
            val_loss = criterion(val_pred, Y_val, W_val)
            
        if val_loss.item() < best_val_loss:
            best_val_loss = val_loss.item()
            best_state = copy.deepcopy(model.state_dict())
            
        if verbose and (epoch % 50 == 0 or epoch == epochs - 1):
            grad_norm = sum(p.grad.norm().item()**2 for p in model.parameters() if p.grad is not None)**0.5
            print(f"  [Epoch {epoch:3d}] Loss: {loss.item():.6f} | Val Loss: {val_loss.item():.6f} | Grad: {grad_norm:.4f}")
            
    if verbose:
        print(f"  [MLP] Best Val Loss: {best_val_loss:.6f}")
        
    # Extract weights to lists for JSON export.
    # Module indices: 0=Linear(15,32), 1=ReLU, 2=Dropout, 3=Linear(32,16), 4=ReLU, 5=Dropout,
    # 6=Linear(16,2) -- shifted from 0/2/4 because of the two inserted Dropout layers. Dropout
    # has no learnable parameters, so this only changes these lookup keys, not the exported
    # array shapes/values: WormGearGuider::loadWeights() and its hardcoded 32x15/16x32/2x16
    # dimensions are unaffected.
    w1 = best_state['net.0.weight'].cpu().numpy().flatten().tolist()
    b1 = best_state['net.0.bias'].cpu().numpy().flatten().tolist()
    w2 = best_state['net.3.weight'].cpu().numpy().flatten().tolist()
    b2 = best_state['net.3.bias'].cpu().numpy().flatten().tolist()
    w_out = best_state['net.6.weight'].cpu().numpy().flatten().tolist()
    b_out = best_state['net.6.bias'].cpu().numpy().flatten().tolist()
    
    return {
        "w1": w1, "b1": b1,
        "w2": w2, "b2": b2,
        "w_out": w_out, "b_out": b_out,
    }

def _zero_weights():
    return {
        "w1": [0.0] * (15 * 32), "b1": [0.0] * 32,
        "w2": [0.0] * (32 * 16), "b2": [0.0] * 16,
        "w_out": [0.0] * (16 * 2), "b_out": [0.0] * 2,
    }
