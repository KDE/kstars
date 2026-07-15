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


def train_worm_gear(sysid: dict,
                    gpu: bool = False,
                    epochs: int = None,
                    verbose: bool = False) -> dict:
    """
    Train the PINN + residual MLP for a worm-gear mount.
    Returns a weights dict compatible with WormGearGuider::loadWeights().
    """
    if not TORCH_AVAILABLE:
        print("[ERROR] PyTorch is required to train the WormGearGuider MLP.")
        print("Please install it: pip install torch")
        sys.exit(1)

    eq = sysid["equipment"]
    pixel_scale = eq["pixel_scale_arcsec_per_px"]
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
                                      k_ref, d_ra_extra, d_polar, k_ref_dec, gpu, epochs, verbose)

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

    return {
        "format_version":    "1.0",
        "mount_type":        "WORM_GEAR",
        "trained_at":        datetime.utcnow().isoformat() + "Z",
        "mount_name":        eq.get("mount_name", "unknown"),
        "pixel_scale":       pixel_scale,
        "model_fingerprint": _build_fingerprint(sysid),
        "physics": {
            "pe_amplitude": float(pe_amplitude),
            "pe_period":    float(pe_period),
            "k_ref":        float(k_ref),
            "d_ra_extra":   float(d_ra_extra),
            "d_polar":      float(d_polar),
            "k_ref_dec":    float(k_ref_dec)
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
    
    if known_period is not None and known_period > 0:
        if verbose:
            print(f"  [FFT] Found known PE period override in sysid: {known_period:.1f} s")
        best_period = known_period
    
    # Power spectrum amplitude approximation for LS
    best_amp = np.sqrt(4 * Pxx_valid[peak_idx] / len(t_vals))
    
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

class ResidualMLP(nn.Module):
    def __init__(self):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(15, 32),
            nn.ReLU(),
            nn.Linear(32, 16),
            nn.ReLU(),
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
    
    pixel_scale = sysid["equipment"]["pixel_scale_arcsec_per_px"]
    
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
            
    if total_frames > 0:
        sat_pct = (saturated_frames / total_frames) * 100.0
        if verbose:
            print(f"  [Dataset] Filtered {saturated_frames}/{total_frames} ({sat_pct:.1f}%) frames due to pulse saturation.")
        if sat_pct > 10.0:
            print(f"\n[WARNING] {sat_pct:.1f}% of your guiding frames hit the Max Pulse limit!")
            print("Your mount is constantly struggling. Please increase your Max Pulse setting in")
            print("the Guide Options, or check your polar alignment and balance.\n")

    return np.array(X_all, dtype=np.float32), np.array(Y_all, dtype=np.float32)

def _train_residual_mlp(sysid, pe_period, pe_amplitude, k_ref, d_ra_extra, d_polar, k_ref_dec, gpu, epochs, verbose) -> dict:
    epochs = epochs or 300
    
    X_np, Y_np = _build_training_dataset(sysid, pe_period, pe_amplitude, k_ref, d_ra_extra, d_polar, k_ref_dec, verbose)
    if len(X_np) < 50:
        if verbose: print("[WARNING] Insufficient data for MLP training. Returning zero weights.")
        return _zero_weights()
        
    # Simple split (last 20% is validation)
    split_idx = int(0.8 * len(X_np))
    X_train, Y_train = torch.tensor(X_np[:split_idx]), torch.tensor(Y_np[:split_idx])
    X_val, Y_val = torch.tensor(X_np[split_idx:]), torch.tensor(Y_np[split_idx:])
    
    device = torch.device("cuda" if gpu and torch.cuda.is_available() else "cpu")
    model = ResidualMLP().to(device)
    X_train, Y_train = X_train.to(device), Y_train.to(device)
    X_val, Y_val = X_val.to(device), Y_val.to(device)
    
    optimizer = optim.Adam(model.parameters(), lr=1e-3, weight_decay=1e-4)
    criterion = nn.MSELoss()
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
        loss = criterion(pred, Y_train)
        loss.backward()
        
        # Gradient clipping
        torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
        optimizer.step()
        scheduler.step()
        
        model.eval()
        with torch.no_grad():
            val_pred = model(X_val)
            val_loss = criterion(val_pred, Y_val)
            
        if val_loss.item() < best_val_loss:
            best_val_loss = val_loss.item()
            best_state = copy.deepcopy(model.state_dict())
            
        if verbose and (epoch % 50 == 0 or epoch == epochs - 1):
            grad_norm = sum(p.grad.norm().item()**2 for p in model.parameters() if p.grad is not None)**0.5
            print(f"  [Epoch {epoch:3d}] Loss: {loss.item():.6f} | Val Loss: {val_loss.item():.6f} | Grad: {grad_norm:.4f}")
            
    if verbose:
        print(f"  [MLP] Best Val Loss: {best_val_loss:.6f}")
        
    # Extract weights to lists for JSON export
    w1 = best_state['net.0.weight'].cpu().numpy().flatten().tolist()
    b1 = best_state['net.0.bias'].cpu().numpy().flatten().tolist()
    w2 = best_state['net.2.weight'].cpu().numpy().flatten().tolist()
    b2 = best_state['net.2.bias'].cpu().numpy().flatten().tolist()
    w_out = best_state['net.4.weight'].cpu().numpy().flatten().tolist()
    b_out = best_state['net.4.bias'].cpu().numpy().flatten().tolist()
    
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
