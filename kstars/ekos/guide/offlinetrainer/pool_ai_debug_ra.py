#!/usr/bin/env python3
"""
pool_ai_debug_ra.py -- RA counterpart to pool_ai_debug_dec.py, built to answer one specific
question: does RA show the same altitude-dependent causal-EMA R^2 pattern DEC does in the pooled
ai_debug_logs data (2026-08-29 finding: R^2 climbs from ~-0.01 at 26-35 deg to ~0.30 at 62-71 deg,
the OPPOSITE shape from the single live-tested night)? If RA -- whose dominant physics is periodic
worm PE + refraction, with no polar-alignment component at all -- shows the SAME altitude pattern,
that argues for a generic confound (better seeing/SNR nearer zenith, less atmosphere) rather than
anything DEC-specific. If RA is flat or shows a different pattern, that argues the DEC effect is real
and axis-specific.

Originally tried to reconstruct WormGearGuider::physicsRABase()'s Shape-Net path exactly from the
CSV's logged online RLS state (rls_sin_coeff_px/rls_cos_coeff_px/rls_drift_rate_px_s/rls_offset_px).
That turned out to be impossible: checked directly against gmath.cpp/worm_gear_guider.cpp
(2026-08-29), those last two columns are actually m_rls_theta(2)/m_rls_theta(3) -- stale names left
over from before the PE model was extended from 1 harmonic to N_HARMONICS=4 (back when theta(2) WAS
the drift term). They're really harmonic-2's sin/cos coefficients now, and neither the true online
drift term (theta(8)) nor harmonics 3-4 are logged anywhere in this CSV schema. A reconstruction
built on that assumption was silently wrong by ~40-100x when checked against the CSV's own logged
physics_ra_arcsec column.

Reconstructs a fundamental-only physics RA rate instead, fit PER SEGMENT the same way
train_worm_gear.py's _build_training_dataset() fits each session's per-frame harmonic phase: lock
the fundamental's amplitude to the deployed weights.json's global prior
(physics.pe_harmonic_amplitudes[0]), free-fit only its phase via linear least-squares against this
segment's own delta series (after subtracting the known, per-frame-altitude-accurate refraction +
d_ra_extra terms), then reconstruct the predicted RATE exactly as _physics_drift() does. This uses
only uncorrected_ra_delta_px/dt/altitude_deg/t_session -- the same CSV columns DEC's script already
trusts -- and drops harmonics 2-4 (a reasonable simplification: their amplitudes are 0.08/0.05/0.01px
vs the fundamental's 0.43px in the deployed weights, so the fundamental dominates PE structure).

Same exclusion as pool_ai_debug_dec.py: --before 20260828 by default, keeping this an independent
check against the night the DEC filter (and any RA filter retuning) was live-tested.

Usage:
    python pool_ai_debug_ra.py --csv-dir ~/.local/share/kstars/ai_debug_logs \\
        --weights ~/.local/share/kstars/ai_guider_weights.json --before 20260828

SPDX-License-Identifier: GPL-2.0-or-later
"""

import argparse
import glob
import json
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
from pool_ai_debug_dec import load_ai_debug_csv, file_date_str, causal_ema_errors, in_bin


def fit_segment_phase_and_residual(t_arr, dt_arr, alt_arr, delta_arr,
                                    pe_period, amp_global, k_ref, d_ra_extra,
                                    fit_alt_min, fit_alt_max):
    """
    Fundamental-only version of train_worm_gear.py's _build_training_dataset() per-session harmonic
    fit: amplitude locked to the deployed global prior, phase free-fit per segment via linear
    least-squares (the worm's absolute position at segment start is unrecoverable from this CSV, so
    phase can't be pinned to anything else). Refraction/d_ra_extra are the FIXED deployed values,
    applied per-frame with the segment's own real altitude (not a single per-segment constant) --
    more accurate than a scalar detrend when altitude drifts within a long closed-loop segment.
    Returns the physics-residual RATE array (same units/meaning as pool_ai_debug_dec.py's DEC one).
    """
    omega = 2.0 * np.pi / pe_period
    alt_clamped = np.clip(alt_arr, fit_alt_min, fit_alt_max)
    cos_alt = np.cos(np.radians(alt_clamped))
    refraction_rate = np.where(np.abs(cos_alt) < 1e-4, 0.0, k_ref / (cos_alt ** 2))
    known_offset_rate = refraction_rate + d_ra_extra

    target = delta_arr - known_offset_rate * dt_arr
    X = np.column_stack([
        omega * np.cos(omega * t_arr) * dt_arr,
        -omega * np.sin(omega * t_arr) * dt_arr,
    ])
    coef, _, _, _ = np.linalg.lstsq(X, target, rcond=None)
    sin_free, cos_free = coef
    phase = np.arctan2(cos_free, sin_free)  # direction only, matches train_worm_gear.py's convention
    sin_k, cos_k = amp_global * np.cos(phase), amp_global * np.sin(phase)

    k_omega = omega
    pe_rate = sin_k * k_omega * np.cos(k_omega * t_arr) - cos_k * k_omega * np.sin(k_omega * t_arr)
    physics_rate = pe_rate + known_offset_rate

    rate = delta_arr / dt_arr
    return rate - physics_rate


def segment_and_residual(rows, weights, gap_threshold_s=30.0, min_segment_frames=30):
    """Split into continuous runs (same guard as pool_ai_debug_dec.py: a big t_session jump means
    guiding was aborted/recalibrated in between) and compute each frame's physics-residual RA rate.
    Returns a list of (altitude_deg array, residual array), one entry per continuous segment.
    min_segment_frames is higher than DEC's (30 vs 10) because phase-fitting a ~205s period needs
    enough span to be identifiable at all -- a handful of frames can't constrain a sinusoid's phase.
    """
    physics = weights["physics"]
    pe_period = physics["pe_period"]
    amp_global = physics["pe_harmonic_amplitudes"][0]
    k_ref, d_ra_extra = physics["k_ref"], physics["d_ra_extra"]
    fit_alt_min, fit_alt_max = physics["fit_alt_min"], physics["fit_alt_max"]

    raw_segments = []
    cur = []
    prev_t = None
    for r in rows:
        try:
            t = float(r["t_session"])
            dt = float(r["dt"])
            alt = float(r["altitude_deg"])
            delta = float(r["uncorrected_ra_delta_px"])
        except (ValueError, KeyError, TypeError):
            continue
        if dt <= 0:
            continue
        if prev_t is not None and (t - prev_t) > gap_threshold_s:
            if len(cur) >= min_segment_frames:
                raw_segments.append(cur)
            cur = []
        cur.append((t, dt, alt, delta))
        prev_t = t
    if len(cur) >= min_segment_frames:
        raw_segments.append(cur)

    series = []
    for seg in raw_segments:
        t_arr = np.array([s[0] for s in seg])
        dt_arr = np.array([s[1] for s in seg])
        alt_arr = np.array([s[2] for s in seg])
        delta_arr = np.array([s[3] for s in seg])
        residual = fit_segment_phase_and_residual(t_arr, dt_arr, alt_arr, delta_arr,
                                                    pe_period, amp_global, k_ref, d_ra_extra,
                                                    fit_alt_min, fit_alt_max)
        series.append((alt_arr, residual))
    return series


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--csv-dir", type=Path, required=True)
    p.add_argument("--weights", type=Path, required=True,
                   help="ai_guider_weights.json -- must contain a 'shape_net' section.")
    p.add_argument("--before", type=str, default=None)
    p.add_argument("--gap-threshold-s", type=float, default=30.0)
    p.add_argument("--n-bins", type=int, default=5)
    p.add_argument("--alpha-grid", type=str, default="0.005,0.01,0.02,0.05,0.08,0.1,0.15,0.2,0.3")
    p.add_argument("--min-frames-per-night", type=int, default=50)
    p.add_argument("--output", type=Path, default=None)
    p.add_argument("--verbose", action="store_true")
    args = p.parse_args()

    weights = json.loads(args.weights.read_text())
    phys = weights["physics"]
    print(f"=== Fixed RA physics (fundamental-only) from {args.weights.name}: "
          f"pe_period={phys['pe_period']:.2f}s  amp={phys['pe_harmonic_amplitudes'][0]:.4f}px  "
          f"k_ref={phys['k_ref']:.4e}  d_ra_extra={phys['d_ra_extra']:.4e} "
          f"(phase free-fit per continuous segment) ===")

    paths = sorted(glob.glob(str(args.csv_dir / "ai_guider_*.csv")))
    if args.before:
        paths = [pth for pth in paths if (file_date_str(pth) or "99999999") < args.before]
    print(f"\n=== Loading {len(paths)} CSV files (before={args.before}) ===")

    night_segments = {}
    all_alts = []
    for path in paths:
        rows = load_ai_debug_csv(path)
        series = segment_and_residual(rows, weights, args.gap_threshold_s)
        n_frames = sum(len(a) for a, _ in series)
        if n_frames < args.min_frames_per_night:
            if args.verbose:
                print(f"  [SKIP] {Path(path).name}: only {n_frames} usable frames")
            continue
        night_segments[Path(path).name] = series
        all_alts.append(np.concatenate([a for a, _ in series]))
        print(f"  {Path(path).name}: {len(series)} segments, {n_frames} frames")

    if not night_segments:
        print("No usable data.")
        sys.exit(1)

    all_alts_flat = np.concatenate(all_alts)
    alt_min, alt_max = float(all_alts_flat.min()), float(all_alts_flat.max())
    bin_edges = np.linspace(alt_min, alt_max, args.n_bins + 1)
    print(f"\n=== Altitude range {alt_min:.1f}-{alt_max:.1f} deg, {args.n_bins} uniform bins: "
          f"{[round(float(e), 1) for e in bin_edges]} ===")

    alphas = [float(x) for x in args.alpha_grid.split(",")]
    nights = sorted(night_segments)

    print(f"\n=== Precomputing causal-EMA walks for {len(nights)} nights x {len(alphas)} alphas ===")
    cache = {}
    for night in nights:
        segs = night_segments[night]
        alt_concat = np.concatenate([a for a, _ in segs])
        for alpha in alphas:
            ems, ebs = [], []
            for alt_arr, resid_arr in segs:
                em, eb = causal_ema_errors(resid_arr, alpha)
                ems.append(em)
                ebs.append(eb)
            cache[(night, alpha)] = (alt_concat, np.concatenate(ems), np.concatenate(ebs))

    print(f"\n=== Leave-one-night-out validation across {len(nights)} nights (RA) ===")
    bin_results = {}
    for b in range(args.n_bins):
        lo, hi = float(bin_edges[b]), float(bin_edges[b + 1])
        is_last = (b == args.n_bins - 1)
        total_sse_model, total_sse_baseline, total_n = 0.0, 0.0, 0
        fold_r2, chosen_alphas = [], []

        for held_out in nights:
            train_nights = [n for n in nights if n != held_out]
            best_alpha, best_r2 = None, -np.inf
            for alpha in alphas:
                sm, sb = 0.0, 0.0
                for n in train_nights:
                    alt_c, em_c, eb_c = cache[(n, alpha)]
                    mask = in_bin(alt_c, lo, hi, is_last)
                    sm += em_c[mask].sum()
                    sb += eb_c[mask].sum()
                r2 = 1.0 - sm / sb if sb > 0 else -np.inf
                if r2 > best_r2:
                    best_alpha, best_r2 = alpha, r2
            chosen_alphas.append(best_alpha)

            alt_c, em_c, eb_c = cache[(held_out, best_alpha)]
            mask = in_bin(alt_c, lo, hi, is_last)
            n_t = int(mask.sum())
            if n_t == 0:
                continue
            sm_t, sb_t = em_c[mask].sum(), eb_c[mask].sum()
            total_sse_model += sm_t
            total_sse_baseline += sb_t
            total_n += n_t
            fold_r2.append(1.0 - sm_t / sb_t if sb_t > 0 else float("nan"))

        r2 = 1.0 - total_sse_model / total_sse_baseline if total_sse_baseline > 0 else float("nan")
        bin_results[b] = {
            "alt_lo": lo, "alt_hi": hi, "n_frames": total_n,
            "n_nights_with_data": len(fold_r2), "r2": float(r2),
            "median_alpha": float(np.median(chosen_alphas)) if chosen_alphas else None,
            "per_fold_r2": [float(x) for x in fold_r2],
        }
        print(f"  alt [{lo:.1f}, {hi:.1f}{']' if is_last else ')'}: R^2={r2:.3f}  "
              f"alpha~{bin_results[b]['median_alpha']:.3g}  n_frames={total_n}  "
              f"n_nights={len(fold_r2)}/{len(nights)}")

    r2_values = [r["r2"] for r in bin_results.values() if not np.isnan(r["r2"])]
    if len(r2_values) >= 2 and (max(r2_values) - min(r2_values)) > 0.05:
        print(f"\n  RESULT: RA R^2 ALSO varies meaningfully across altitude "
              f"({min(r2_values):.3f} to {max(r2_values):.3f}) -- consistent with a generic "
              f"altitude/SNR confound rather than a DEC-specific mechanism. Compare the shape "
              f"(which altitude wins) against the DEC run, not just whether it varies.")
    elif r2_values:
        print(f"\n  RESULT: RA R^2 is roughly flat across altitude ({min(r2_values):.3f} to "
              f"{max(r2_values):.3f}) despite the same data/nights/methodology where DEC showed a "
              f"real altitude effect -- argues the DEC pattern is axis-specific, not a generic "
              f"seeing/SNR confound.")

    if args.output:
        out = {
            "weights_file": str(args.weights), "before": args.before,
            "nights_used": nights, "n_bins": args.n_bins,
            "bin_edges": [float(e) for e in bin_edges], "alpha_grid": alphas,
            "ra_residual_trust": [{"bin": b, **r} for b, r in sorted(bin_results.items())],
        }
        args.output.write_text(json.dumps(out, indent=2))
        print(f"\n  Altitude-binned RA trust table written to {args.output}")


if __name__ == "__main__":
    main()
