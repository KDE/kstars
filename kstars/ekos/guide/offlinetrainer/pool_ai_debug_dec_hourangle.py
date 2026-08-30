#!/usr/bin/env python3
"""
pool_ai_debug_dec_hourangle.py -- follow-up to pool_ai_debug_dec.py's altitude finding (2026-08-29):
DEC's causal-EMA-explained residual variance rises with altitude across 29 real nights (p<0.05,
Mann-Whitney low-vs-high bin). But the true causal driver was never identified -- polar alignment,
SNR, and time-since-session-start were all ruled out. Altitude might just be a CORRELATE of the real
driver, not the cause: for a GEM tracking any target near the celestial equator, altitude is close to
its own maximum right at meridian transit (hour angle = 0) and falls off on either side -- so altitude
and |hour angle| are naturally collinear for continuously-tracked targets, and "altitude" and
"distance from meridian" are hard to tell apart without checking directly.

This script computes hour angle (and its sign, a proxy for pier side -- HA<0 is pre-meridian/rising,
HA>0 is post-meridian/setting, for an ideal GEM that flips exactly at the meridian; using the sign of
HA rather than trusting an INDI-reported pier-side label sidesteps East/West labeling-convention
ambiguity that varies by software) purely from altitude_deg/azimuth_deg and the site's latitude --
no RA/LST needed, same spherical-trig identity altaz_to_radec.py already uses, just stopping one step
earlier. Then reruns the exact same leave-one-night-out causal-EMA methodology, binned by |HA| instead
of altitude, and reports the altitude/|HA| collinearity directly, so the two explanations can actually
be compared instead of assumed.

Usage:
    python pool_ai_debug_dec_hourangle.py --csv-dir ~/.local/share/kstars/ai_debug_logs \\
        --fixed-physics-from ~/.local/share/kstars/ai_guider_weights.json --before 20260828

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

SITE_LAT_DEG = 29.1136559  # matches altaz_to_radec.py -- same site all this data was collected at


def hour_angle_deg(altitude_deg, azimuth_deg, lat_deg=SITE_LAT_DEG):
    """
    Hour angle (deg, signed: negative = pre-meridian/rising, positive = post-meridian/setting) from
    alt/az/lat alone -- the same spherical-trig identity altaz_to_radec.py uses to get RA/DEC, just
    without the final LST->RA step (which needs wall-clock time; HA itself doesn't).
    """
    alt_r = np.radians(altitude_deg)
    az_r = np.radians(azimuth_deg)
    lat_r = np.radians(lat_deg)

    sin_dec = np.sin(alt_r) * np.sin(lat_r) + np.cos(alt_r) * np.cos(lat_r) * np.cos(az_r)
    sin_dec = np.clip(sin_dec, -1.0, 1.0)
    dec_r = np.arcsin(sin_dec)
    cos_dec = np.cos(dec_r)
    cos_dec = np.where(np.abs(cos_dec) < 1e-6, 1e-6, cos_dec)

    sinH = -np.sin(az_r) * np.cos(alt_r) / cos_dec
    cosH = (np.sin(alt_r) - np.sin(lat_r) * sin_dec) / (np.cos(lat_r) * cos_dec)
    H = np.arctan2(sinH, cosH)
    return np.degrees(H)


def segment_and_residual_with_ha(rows, d_polar, k_ref_dec, gap_threshold_s=30.0, min_segment_frames=10):
    """Same segmentation/residual-rate computation as pool_ai_debug_dec.py's DEC path, but also
    carries altitude and hour angle per frame through to the caller."""
    from pool_ai_debug_dec import physics_dec_rate_px_s  # reuse the exact validated formula

    raw_segments = []
    cur = []
    prev_t = None
    for r in rows:
        try:
            t = float(r["t_session"])
            dt = float(r["dt"])
            alt = float(r["altitude_deg"])
            az = float(r["azimuth_deg"])
            q = float(r["parallactic_angle_deg"])
            delta = float(r["uncorrected_dec_delta_px"])
        except (ValueError, KeyError, TypeError):
            continue
        if dt <= 0:
            continue
        if prev_t is not None and (t - prev_t) > gap_threshold_s:
            if len(cur) >= min_segment_frames:
                raw_segments.append(cur)
            cur = []
        cur.append((alt, az, q, dt, delta))
        prev_t = t
    if len(cur) >= min_segment_frames:
        raw_segments.append(cur)

    series = []
    for seg in raw_segments:
        alt_arr = np.array([s[0] for s in seg])
        az_arr = np.array([s[1] for s in seg])
        q_arr = np.array([s[2] for s in seg])
        dt_arr = np.array([s[3] for s in seg])
        delta_arr = np.array([s[4] for s in seg])

        rate = delta_arr / dt_arr
        phys_rate = physics_dec_rate_px_s(alt_arr, q_arr, d_polar, k_ref_dec)
        residual = rate - phys_rate
        ha_arr = hour_angle_deg(alt_arr, az_arr)
        series.append((alt_arr, ha_arr, residual))
    return series


def loocv_binned(night_series, key_fn, bin_edges, alphas):
    """
    Generic leave-one-night-out causal-EMA R^2, binned by whatever scalar key_fn(alt_arr, ha_arr)
    returns per frame. Mirrors pool_ai_debug_dec.py's loocv_dec_altitude_bins() structure but
    parameterized over the binning variable so altitude and |HA| can be run through identically.
    """
    nights = sorted(night_series)
    cache = {}
    for night in nights:
        segs = night_series[night]
        key_concat = np.concatenate([key_fn(alt, ha) for alt, ha, _ in segs])
        for alpha in alphas:
            ems, ebs = [], []
            for _, _, residual in segs:
                em, eb = causal_ema_errors(residual, alpha)
                ems.append(em)
                ebs.append(eb)
            cache[(night, alpha)] = (key_concat, np.concatenate(ems), np.concatenate(ebs))

    n_bins = len(bin_edges) - 1
    results = {}
    for b in range(n_bins):
        lo, hi = float(bin_edges[b]), float(bin_edges[b + 1])
        is_last = (b == n_bins - 1)
        total_sm, total_sb, total_n = 0.0, 0.0, 0
        fold_r2 = []
        for held_out in nights:
            train_nights = [n for n in nights if n != held_out]
            best_alpha, best_r2 = None, -np.inf
            for alpha in alphas:
                sm, sb = 0.0, 0.0
                for n in train_nights:
                    kc, em, eb = cache[(n, alpha)]
                    mask = in_bin(kc, lo, hi, is_last)
                    sm += em[mask].sum()
                    sb += eb[mask].sum()
                r2 = 1.0 - sm / sb if sb > 0 else -np.inf
                if r2 > best_r2:
                    best_alpha, best_r2 = alpha, r2
            kc, em, eb = cache[(held_out, best_alpha)]
            mask = in_bin(kc, lo, hi, is_last)
            n_t = int(mask.sum())
            if n_t == 0:
                continue
            sm_t, sb_t = em[mask].sum(), eb[mask].sum()
            total_sm += sm_t
            total_sb += sb_t
            total_n += n_t
            fold_r2.append(1.0 - sm_t / sb_t if sb_t > 0 else float("nan"))
        r2 = 1.0 - total_sm / total_sb if total_sb > 0 else float("nan")
        results[b] = {"lo": lo, "hi": hi, "n_frames": total_n, "n_nights": len(fold_r2),
                      "r2": float(r2), "per_fold_r2": [float(x) for x in fold_r2]}
    return results


def loocv_predicate_binned(night_series, groups, alphas):
    """
    Like loocv_binned(), but groups are arbitrary (label, predicate) pairs instead of numeric edges
    on a single scalar key -- predicate(alt_arr, ha_arr) returns a boolean mask. Lets a "group" be a
    compound condition (e.g. "altitude in [53,62] AND HA<0"), which is what an altitude-controlled
    pier-side check needs: the EMA state still walks the FULL, real, continuous segment (exactly as
    the online filter would), only the post-hoc attribution of each frame's already-computed error
    into a results bucket is compound-conditioned.
    """
    nights = sorted(night_series)
    cache = {}
    for night in nights:
        segs = night_series[night]
        alt_concat = np.concatenate([alt for alt, _, _ in segs])
        ha_concat = np.concatenate([ha for _, ha, _ in segs])
        for alpha in alphas:
            ems, ebs = [], []
            for _, _, residual in segs:
                em, eb = causal_ema_errors(residual, alpha)
                ems.append(em)
                ebs.append(eb)
            cache[(night, alpha)] = (alt_concat, ha_concat, np.concatenate(ems), np.concatenate(ebs))

    results = {}
    for label, predicate in groups:
        total_sm, total_sb, total_n = 0.0, 0.0, 0
        fold_r2 = []
        for held_out in nights:
            train_nights = [n for n in nights if n != held_out]
            best_alpha, best_r2 = None, -np.inf
            for alpha in alphas:
                sm, sb = 0.0, 0.0
                for n in train_nights:
                    alt_c, ha_c, em, eb = cache[(n, alpha)]
                    mask = predicate(alt_c, ha_c)
                    sm += em[mask].sum()
                    sb += eb[mask].sum()
                r2 = 1.0 - sm / sb if sb > 0 else -np.inf
                if r2 > best_r2:
                    best_alpha, best_r2 = alpha, r2
            if best_alpha is None:
                continue  # no training-night frames matched this predicate at all -- skip this fold
            alt_c, ha_c, em, eb = cache[(held_out, best_alpha)]
            mask = predicate(alt_c, ha_c)
            n_t = int(mask.sum())
            if n_t == 0:
                continue
            sm_t, sb_t = em[mask].sum(), eb[mask].sum()
            total_sm += sm_t
            total_sb += sb_t
            total_n += n_t
            fold_r2.append(1.0 - sm_t / sb_t if sb_t > 0 else float("nan"))
        r2 = 1.0 - total_sm / total_sb if total_sb > 0 else float("nan")
        results[label] = {"n_frames": total_n, "n_nights": len(fold_r2), "r2": float(r2),
                           "per_fold_r2": [float(x) for x in fold_r2]}
    return results


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--csv-dir", type=Path, required=True)
    p.add_argument("--fixed-physics-from", type=Path, required=True)
    p.add_argument("--before", type=str, default=None)
    p.add_argument("--n-bins", type=int, default=5)
    p.add_argument("--min-frames-per-night", type=int, default=50)
    p.add_argument("--alpha-grid", type=str, default="0.005,0.01,0.02,0.05,0.08,0.1,0.15,0.2,0.3")
    args = p.parse_args()

    physics = json.loads(args.fixed_physics_from.read_text())["physics"]
    d_polar, k_ref_dec = physics["d_polar"], physics["k_ref_dec"]
    alphas = [float(x) for x in args.alpha_grid.split(",")]

    paths = sorted(glob.glob(str(args.csv_dir / "ai_guider_*.csv")))
    if args.before:
        paths = [pth for pth in paths if (file_date_str(pth) or "99999999") < args.before]
    print(f"=== Loading {len(paths)} CSV files (before={args.before}) ===")

    night_series = {}
    all_alt, all_ha = [], []
    for path in paths:
        rows = load_ai_debug_csv(path)
        series = segment_and_residual_with_ha(rows, d_polar, k_ref_dec)
        n_frames = sum(len(a) for a, _, _ in series)
        if n_frames < args.min_frames_per_night:
            continue
        night_series[Path(path).name] = series
        all_alt.append(np.concatenate([a for a, _, _ in series]))
        all_ha.append(np.concatenate([h for _, h, _ in series]))
        print(f"  {Path(path).name}: {len(series)} segments, {n_frames} frames")

    all_alt = np.concatenate(all_alt)
    all_ha = np.concatenate(all_ha)
    all_abs_ha = np.abs(all_ha)

    print(f"\n=== Altitude vs |hour angle| collinearity check ===")
    r = np.corrcoef(all_alt, all_abs_ha)[0, 1]
    print(f"  Pearson corr(altitude, |HA|) across {len(all_alt)} frames: {r:.3f}")
    print(f"  (0 = independent variables, +/-1 = one is redundant given the other;"
          f" expect strongly negative if they're substantially the same axis)")
    print(f"  Pier-side-sign split (HA<0 pre-meridian vs HA>=0 post-meridian): "
          f"{(all_ha < 0).sum()} vs {(all_ha >= 0).sum()} frames")

    print(f"\n=== Re-running the SAME leave-one-night-out analysis, binned by |hour angle| instead of altitude ===")
    ha_edges = np.linspace(0.0, float(np.percentile(all_abs_ha, 99)), args.n_bins + 1)
    print(f"  |HA| bins (deg): {[round(float(e), 1) for e in ha_edges]}")
    ha_results = loocv_binned(night_series, lambda alt, ha: np.abs(ha), ha_edges, alphas)
    for b in sorted(ha_results):
        r_ = ha_results[b]
        print(f"  |HA| [{r_['lo']:.1f}, {r_['hi']:.1f}]: R^2={r_['r2']:.3f}  "
              f"n_frames={r_['n_frames']}  n_nights={r_['n_nights']}")

    print(f"\n=== Same analysis, binned by altitude (reference -- should match pool_ai_debug_dec.py) ===")
    alt_edges = np.linspace(float(all_alt.min()), float(all_alt.max()), args.n_bins + 1)
    alt_results = loocv_binned(night_series, lambda alt, ha: alt, alt_edges, alphas)
    for b in sorted(alt_results):
        r_ = alt_results[b]
        print(f"  alt [{r_['lo']:.1f}, {r_['hi']:.1f}]: R^2={r_['r2']:.3f}  "
              f"n_frames={r_['n_frames']}  n_nights={r_['n_nights']}")

    print(f"\n=== Pier-side-sign only (2 bins, ignoring altitude/|HA| magnitude entirely) ===")
    sign_results = loocv_binned(night_series, lambda alt, ha: np.sign(ha), [-1.5, 0.0, 1.5], alphas)
    labels = {0: "pre-meridian (HA<0)", 1: "post-meridian (HA>=0)"}
    for b in sorted(sign_results):
        r_ = sign_results[b]
        print(f"  {labels[b]}: R^2={r_['r2']:.3f}  n_frames={r_['n_frames']}  n_nights={r_['n_nights']}")

    print(f"\n=== Altitude-controlled pier-side check: pre- vs post-meridian WITHIN each altitude bin ===")
    print(f"  (this is the actual test -- the uncontrolled 0.217 vs 0.091 split could just reflect")
    print(f"   the two groups having different altitude mixes; holding altitude fixed removes that)")
    groups = []
    for b in range(len(alt_edges) - 1):
        lo, hi = float(alt_edges[b]), float(alt_edges[b + 1])
        is_last = (b == len(alt_edges) - 2)
        for sign_label, sign_pred in [("pre-meridian", lambda h: h < 0), ("post-meridian", lambda h: h >= 0)]:
            def pred(alt_c, ha_c, lo=lo, hi=hi, is_last=is_last, sign_pred=sign_pred):
                alt_mask = in_bin(alt_c, lo, hi, is_last)
                return alt_mask & sign_pred(ha_c)
            groups.append((f"alt[{lo:.1f},{hi:.1f}] {sign_label}", pred))
    stratified = loocv_predicate_binned(night_series, groups, alphas)
    for label, r_ in stratified.items():
        print(f"  {label:<32s}: R^2={r_['r2']:.3f}  n_frames={r_['n_frames']:5d}  n_nights={r_['n_nights']}")

    print(f"\n  Pre- vs post-meridian delta, per altitude bin:")
    for b in range(len(alt_edges) - 1):
        lo, hi = float(alt_edges[b]), float(alt_edges[b + 1])
        pre = stratified.get(f"alt[{lo:.1f},{hi:.1f}] pre-meridian")
        post = stratified.get(f"alt[{lo:.1f},{hi:.1f}] post-meridian")
        if pre and post and pre["n_nights"] > 0 and post["n_nights"] > 0:
            print(f"    alt[{lo:.1f},{hi:.1f}]: pre={pre['r2']:.3f} (n_nights={pre['n_nights']})  "
                  f"post={post['r2']:.3f} (n_nights={post['n_nights']})  delta={pre['r2']-post['r2']:+.3f}")
        else:
            print(f"    alt[{lo:.1f},{hi:.1f}]: insufficient nights in one or both groups to compare")

    alt_r2 = [alt_results[b]["r2"] for b in sorted(alt_results) if not np.isnan(alt_results[b]["r2"])]
    ha_r2 = [ha_results[b]["r2"] for b in sorted(ha_results) if not np.isnan(ha_results[b]["r2"])]
    print(f"\n=== Comparison ===")
    print(f"  Altitude bins R^2 range: {min(alt_r2):.3f} to {max(alt_r2):.3f} (spread {max(alt_r2)-min(alt_r2):.3f})")
    print(f"  |HA| bins R^2 range:     {min(ha_r2):.3f} to {max(ha_r2):.3f} (spread {max(ha_r2)-min(ha_r2):.3f})")
    if abs(r) > 0.6:
        print(f"  Altitude and |HA| are substantially collinear (|r|={abs(r):.2f}) at this site/dataset --"
              f" the two binnings are largely re-describing the same underlying axis, so whichever spread"
              f" is larger doesn't cleanly prove causal priority, just which framing organizes the noise"
              f" slightly better in this particular sample.")
    else:
        print(f"  Altitude and |HA| are NOT strongly collinear (|r|={abs(r):.2f}) -- whichever shows the"
              f" cleaner/larger spread is a real, independent signal about which variable actually matters.")


if __name__ == "__main__":
    main()
