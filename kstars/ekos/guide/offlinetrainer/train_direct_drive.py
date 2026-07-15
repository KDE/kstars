"""
offline_trainer/train_direct_drive.py — Parametric refraction model fitting.

This is the only trainer that does NOT use PyTorch. It fits 4 parameters
analytically using least-squares regression on free-drift sysid sessions.
No GPU needed. Runtime: < 5 seconds on any hardware.

Fits:
    drift_ra(alt)  = k_ref / cos²(alt)  +  d_ra_extra
    drift_dec      = d_dec  (constant)

SPDX-License-Identifier: GPL-2.0-or-later
"""

import json
import numpy as np
import scipy.stats
from typing import Optional
from datetime import datetime


def train_direct_drive(sysid: dict,
                       verbose: bool = False) -> dict:
    """
    Fit the 4-parameter refraction model from free-drift sysid sessions.

    Returns a weights dict compatible with DirectDriveGuider::loadWeights().
    """
    eq = sysid["equipment"]
    pixel_scale = eq["pixel_scale_arcsec_per_px"]   # arcsec/px
    guide_exp   = eq["guide_exposure_ms"] / 1000.0   # seconds

    # Collect all free-drift frames across all positions
    ra_drifts   = []   # observed RA drift (pixels/second)
    dec_drifts  = []   # observed DEC drift (pixels/second)
    altitudes   = []   # altitude at each measurement (degrees)
    q_angles    = []   # parallactic angle at each measurement (degrees)

    for session in sysid["sessions"]:
        if session["type"] != "free_drift":
            continue

        frames = session["frames"]
        alt    = session["altitude_deg"]
        n      = len(frames)

        if n < 3:
            if verbose:
                print(f"  Skipping short free-drift session (n={n}): {session['session_id']}")
            continue

        # Compute per-frame drift velocities from consecutive frame pairs
        for i in range(1, n):
            dt = frames[i]["dt"] if "dt" in frames[i] else guide_exp
            if dt < 0.1:
                continue
            dra  = (frames[i]["ra_raw_px"]  - frames[i-1]["ra_raw_px"])  / dt
            ddec = (frames[i]["dec_raw_px"] - frames[i-1]["dec_raw_px"]) / dt
            q    = frames[i].get("parallactic_angle_deg", 0.0)

            ra_drifts.append(dra)
            dec_drifts.append(ddec)
            altitudes.append(alt)
            q_angles.append(q)

    if len(ra_drifts) < 10:
        raise ValueError(
            f"Insufficient free-drift data: only {len(ra_drifts)} measurements. "
            f"Need at least 10. Run the Guide AI Assistant at more sky positions."
        )

    ra_drifts  = np.array(ra_drifts)
    dec_drifts = np.array(dec_drifts)
    altitudes  = np.array(altitudes)
    q_angles   = np.array(q_angles)

    if verbose:
        print(f"  Fitting on {len(ra_drifts)} drift measurements across "
              f"{len(set(altitudes))} altitudes")

    # ---------------------------------------------------------------------------
    # Fit RA drift: drift_ra = k_ref / cos²(alt) + d_ra_extra
    # Using ordinary least squares:
    #   [1/cos²(alt₁)  1]  [k_ref      ]   [drift_ra₁]
    #   [1/cos²(alt₂)  1] ×[d_ra_extra ] = [drift_ra₂]
    #   [    ...           ]             = [    ...   ]
    # ---------------------------------------------------------------------------
    alt_rad    = np.radians(altitudes)
    cos_sq_inv = 1.0 / np.cos(alt_rad) ** 2

    A_ra = np.column_stack([cos_sq_inv, np.ones_like(cos_sq_inv)])
    result_ra, _, _, _ = np.linalg.lstsq(A_ra, ra_drifts, rcond=None)
    k_ref, d_ra_extra = result_ra

    # ---------------------------------------------------------------------------
    # Fit DEC drift: drift_dec = k_ref_dec * (sin(q)/cos^2(alt)) + d_polar
    # ---------------------------------------------------------------------------
    q_rad = np.radians(q_angles)
    refraction_factors = np.sin(q_rad) * cos_sq_inv
    
    q_range = np.max(q_angles) - np.min(q_angles) if len(q_angles) > 0 else 0.0
    if q_range < 20.0:
        if verbose:
            print(f"  [Refraction DEC] WARNING: Parallactic angle coverage insufficient ({q_range:.1f}° < 20°). Falling back to k_ref.")
        k_ref_dec = k_ref
        d_polar = float(np.mean(dec_drifts - k_ref_dec * refraction_factors))
    else:
        slope_dec, intercept_dec, r_value_dec, _, _ = scipy.stats.linregress(refraction_factors, dec_drifts)
        k_ref_dec = float(slope_dec)
        d_polar = float(intercept_dec)

    # ---------------------------------------------------------------------------
    # phi_drift: angle of overall polar drift vector
    # ---------------------------------------------------------------------------
    phi_drift = float(np.arctan2(d_polar, d_ra_extra))

    if verbose:
        print(f"  k_ref      = {k_ref:.6f}  (refraction coefficient)")
        print(f"  d_ra_extra = {d_ra_extra:.6f}  px/s residual RA drift")
        print(f"  d_polar    = {d_polar:.6f}  px/s polar drift")
        print(f"  k_ref_dec  = {k_ref_dec:.6f}  px/s DEC refraction coefficient")
        print(f"  phi_drift  = {np.degrees(phi_drift):.2f}°")

        # Report residuals
        predicted_ra = k_ref * cos_sq_inv + d_ra_extra
        residuals    = ra_drifts - predicted_ra
        print(f"  RA fit RMS residual: {np.std(residuals) * pixel_scale * 3600:.3f} arcsec/s")

    # Build model fingerprint from equipment block
    fingerprint = _build_fingerprint(sysid)

    return {
        "format_version":   "1.0",
        "mount_type":       "DIRECT_DRIVE",
        "trained_at":       datetime.utcnow().isoformat() + "Z",
        "mount_name":       eq.get("mount_name", "unknown"),
        "pixel_scale":      pixel_scale,
        "model_fingerprint": fingerprint,
        "parameters": {
            "k_ref":      float(k_ref),
            "d_polar":    float(d_polar),
            "k_ref_dec":  float(k_ref_dec),
            "d_ra_extra": float(d_ra_extra),
            "phi_drift":  float(phi_drift),
        },
        "training_stats": {
            "n_measurements": int(len(ra_drifts)),
            "n_altitudes":    int(len(set(altitudes.tolist()))),
            "ra_residual_rms_arcsec_per_s": float(np.std(ra_drifts - (k_ref * cos_sq_inv + d_ra_extra)) * pixel_scale * 3600),
        }
    }


def _build_fingerprint(sysid: dict) -> dict:
    """Build the model validity fingerprint from sysid equipment block."""
    eq = sysid["equipment"]
    # The sysid JSON records the settings that were active during collection
    fp = {
        "guide_exposure_s":    eq.get("guide_exposure_ms", 2000) / 1000.0,
        "guide_binning":       eq.get("guide_binning", "1x1"),
        "ra_proportional_gain":  eq.get("ra_proportional_gain",  133.33),
        "dec_proportional_gain": eq.get("dec_proportional_gain", 133.33),
        "ra_integral_gain":      eq.get("ra_integral_gain",  0.0),
        "dec_integral_gain":     eq.get("dec_integral_gain", 0.0),
        "ra_min_pulse_arcsec":   eq.get("ra_min_pulse_arcsec",  0.2),
        "dec_min_pulse_arcsec":  eq.get("dec_min_pulse_arcsec", 0.2),
        "ra_max_pulse_arcsec":   eq.get("ra_max_pulse_arcsec",  25.0),
        "dec_max_pulse_arcsec":  eq.get("dec_max_pulse_arcsec", 25.0),
        "ra_hysteresis":         eq.get("ra_hysteresis",  0.0),
        "dec_hysteresis":        eq.get("dec_hysteresis", 0.0),
        "ra_pulse_algorithm":    eq.get("ra_pulse_algorithm",  0),
        "dec_pulse_algorithm":   eq.get("dec_pulse_algorithm", 0),
        "all_directions_enabled": True,
    }
    # SHA256 of sorted key=value pairs
    import hashlib
    fp_str = "&".join(f"{k}={v}" for k, v in sorted(fp.items()))
    fp["fingerprint_sha256"] = hashlib.sha256(fp_str.encode()).hexdigest()[:16]
    return fp
