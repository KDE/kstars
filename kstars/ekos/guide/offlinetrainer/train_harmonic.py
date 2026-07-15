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
        },
        "qnet": qnet_weights,
    }


# ═══════════════════════════════════════════════════════════════════════════════
# Phase 1: Spring parameter fitting from pulse_response sessions
# ═══════════════════════════════════════════════════════════════════════════════

def _spring_response_model(t, pulse_magnitude, kappa, tau):
    """
    Model: the mount position after a pulse = pulse_mag * (1 - κ * exp(-t/τ))
    At t=0 the immediate response is pulse_mag * (1 - κ).
    As t→∞ the spring releases and total response → pulse_mag.
    """
    return pulse_magnitude * (1.0 - kappa * np.exp(-t / tau))


def _fit_spring_params(sysid: dict, axis: str, guide_exp: float, verbose: bool):
    """
    Fit spring constant κ and time constant τ from pulse_response sessions.

    For each pulse_response session matching the given axis:
    1. Extract the response curve (position change after the pulse)
    2. Fit: response(t) = pulse_mag * (1 - κ * exp(-t/τ))
    3. Average κ and τ across all pulse magnitudes for robustness

    Returns: (kappa, tau_seconds)
    """
    pulse_sessions = [
        s for s in sysid["sessions"]
        if s.get("type") == "pulse_response" and s.get("pulse_axis", "").upper() == axis.upper()
    ]

    if not pulse_sessions:
        if verbose:
            print(f"  [{axis}] No pulse_response sessions found. Using defaults (κ=0.2, τ=1.5s)")
        return 0.2, 1.5

    kappas = []
    taus = []

    for s in pulse_sessions:
        pulse_mag = s.get("pulse_magnitude_ms", 100.0)
        frames = s.get("response_frames", [])
        if len(frames) < 3:
            continue

        # Build time and position arrays
        # Position is the cumulative displacement from the first frame
        axis_key = "ra_raw_px" if axis.upper() == "RA" else "dec_raw_px"
        baseline = frames[0].get(axis_key, 0.0)

        t_vals = []
        pos_vals = []
        for i, f in enumerate(frames):
            if i == 0:
                continue  # Skip the frame where pulse was sent
            t = f.get("t", 0.0) - frames[0].get("t", 0.0)
            if t <= 0:
                t = i * guide_exp
            t_vals.append(t)
            pos_vals.append(abs(f.get(axis_key, 0.0) - baseline))

        t_arr = np.array(t_vals)
        pos_arr = np.array(pos_vals)

        if len(t_arr) < 3 or np.max(pos_arr) < 0.01:
            continue

        # Normalize position by the expected full response
        # (we don't know the exact calibration, so use max observed position)
        max_response = np.max(pos_arr)

        try:
            # Fit the spring model
            # response(t) = max_response * (1 - κ * exp(-t/τ))
            def model(t, kappa, tau):
                return max_response * (1.0 - kappa * np.exp(-t / tau))

            popt, pcov = scipy.optimize.curve_fit(
                model, t_arr, pos_arr,
                p0=[0.3, 1.5],
                bounds=([0.0, 0.1], [0.9, 10.0]),
                maxfev=5000
            )
            kappas.append(popt[0])
            taus.append(popt[1])

            if verbose:
                print(f"  [{axis}] Pulse {pulse_mag}ms "
                      f"{s.get('pulse_direction', '?')}: "
                      f"κ={popt[0]:.3f}, τ={popt[1]:.2f}s "
                      f"(max_response={max_response:.3f}px)")
        except (RuntimeError, ValueError) as e:
            if verbose:
                print(f"  [{axis}] Pulse {pulse_mag}ms: curve_fit failed ({e})")
            continue

    if not kappas:
        if verbose:
            print(f"  [{axis}] All curve fits failed. Using defaults.")
        return 0.2, 1.5

    # Median is more robust than mean against outliers
    kappa_result = float(np.median(kappas))
    tau_result = float(np.median(taus))

    if verbose:
        print(f"  [{axis}] Final: κ={kappa_result:.3f} (from {len(kappas)} fits), "
              f"τ={tau_result:.2f}s")

    return kappa_result, tau_result


# ═══════════════════════════════════════════════════════════════════════════════
# Phase 2: PE period detection from free-drift data
# ═══════════════════════════════════════════════════════════════════════════════

def _estimate_pe(sysid: dict, guide_exp: float, verbose: bool):
    """
    Detect PE period and amplitude from free-drift data using Lomb-Scargle.
    Searches 0.1-0.5 Hz (periods 2-10s) for harmonic drive PE.
    Returns (period_seconds, amplitude_pixels) or (0.0, 0.0) if no PE detected.
    """
    free_drift_sessions = [s for s in sysid["sessions"] if s["type"] == "free_drift"]
    if not free_drift_sessions:
        if verbose:
            print("  No free_drift sessions found. PE detection skipped.")
        return 0.0, 0.0

    # Use the longest free drift session
    longest_session = max(free_drift_sessions, key=lambda s: len(s["frames"]))
    frames = longest_session["frames"]

    t_vals = []
    ra_vals = []
    t = 0.0
    for f in frames:
        t += f.get("dt", guide_exp)
        t_vals.append(t)
        ra_vals.append(f["ra_raw_px"])

    t_arr = np.array(t_vals)
    ra_arr = np.array(ra_vals)

    if len(t_arr) < 20:
        if verbose:
            print("  Free drift session too short for PE detection.")
        return 0.0, 0.0

    # Detrend to remove linear drift
    slope, intercept, _, _, _ = scipy.stats.linregress(t_arr, ra_arr)
    ra_detrended = ra_arr - (slope * t_arr + intercept)

    # Lomb-Scargle in the harmonic drive PE frequency range
    # PE periods 2-10s → frequencies 0.1-0.5 Hz
    nyquist = 0.5 / guide_exp  # Maximum observable frequency
    f_max = min(0.5, nyquist * 0.9)  # Stay below Nyquist
    f_min = 0.1

    if f_min >= f_max:
        if verbose:
            print(f"  Guide exposure ({guide_exp}s) too long for harmonic PE detection. "
                  f"Nyquist={nyquist:.2f} Hz")
        return 0.0, 0.0

    f_search = np.linspace(f_min, f_max, 2000)
    omega = 2 * np.pi * f_search
    Pxx = scipy.signal.lombscargle(t_arr, ra_detrended, omega, precenter=True)

    peak_idx = np.argmax(Pxx)
    peak_power = Pxx[peak_idx]
    peak_freq = f_search[peak_idx]
    peak_period = 1.0 / peak_freq

    # Significance test: is the peak significantly above the noise floor?
    # Use the median power as the noise level estimate
    noise_floor = np.median(Pxx)
    snr = peak_power / (noise_floor + 1e-10)

    # Amplitude estimate from Lomb-Scargle power
    amplitude = np.sqrt(4 * peak_power / len(t_arr))

    if verbose:
        print(f"  [LS] Searched {f_min:.2f}-{f_max:.2f} Hz ({1/f_max:.1f}-{1/f_min:.1f}s)")
        print(f"  [LS] Peak at {peak_freq:.4f} Hz → Period: {peak_period:.2f}s")
        print(f"  [LS] Peak SNR: {snr:.1f} (threshold: 10.0)")
        print(f"  [LS] Amplitude: {amplitude:.4f} px")

    # Require SNR > 10 for a significant PE detection
    if snr < 10.0:
        if verbose:
            print(f"  [LS] PE not significant (SNR {snr:.1f} < 10.0). Disabling PE states.")
        return 0.0, 0.0

    # Sanity bounds
    peak_period = np.clip(peak_period, 1.5, 15.0)
    amplitude = np.clip(amplitude, 0.01, 5.0)

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
