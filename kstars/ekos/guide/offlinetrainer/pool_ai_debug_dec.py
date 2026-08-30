#!/usr/bin/env python3
"""
pool_ai_debug_dec.py -- sibling to pool_sysid_data.py's `--axis dec` mode, using a different data
source because that one turned out to be the wrong regime for this question.

Why not sysid free_drift (see pool_sysid_data.py's build_dec_session_series() docstring): free_drift
sessions are short (~7-10 min), passively tracking with no active correction, and per-session physics
fitting already absorbs most of the session's own linear trend by construction. Checked directly on
real data (2026-08-29): the frame-to-frame DEC RATE computed from free_drift position has strongly
NEGATIVE lag-1 autocorrelation (-0.3 to -0.44 across every session checked, classic differencing
noise), not the slow positive autocorrelation (0.15-0.4 out to 30+s) the live single-night
investigation found in closed-loop guiding data -- a causal EMA validated on free_drift scored ~0 or
slightly negative everywhere and flat across altitude, regardless of whether physics was refit
per-night or held fixed. That's a real property of free_drift, not a bug in that script: whatever
produces the live DEC signature (plausibly backlash/stiction only excited under active correction
pulsing, or a longer-timescale drift only visible over longer continuous sessions) isn't present in
short passive drift snapshots.

This script instead pools the same per-frame "uncorrected_dec_delta_px" / physics-residual-rate
quantity WormGearGuider::updateResidualKFDec() tracks online, straight from real multi-night
closed-loop ai_debug_logs CSVs, binned by altitude continuously (not 3 fixed sysid altitudes -- the
mount naturally sweeps a continuous altitude range over a night's guiding) and validated
leave-one-night-out (alpha chosen only from other nights, never the one being scored).

Deliberately excludes any file from the night the DEC online filter was actually built and live
tested (2026-08-28 onward, via --before) -- that data is what generated the altitude-dependence
hypothesis in the first place, so re-using it here would not be an independent check. Every earlier
file predates the filter's existence in the deployed binary, so their raw "uncorrected_dec_delta_px"
is a clean physics-only regime with no risk of the online filter's own estimate leaking into what's
supposed to be an untouched residual.

MOUNT-AGNOSTICISM (added 2026-08-30): this altitude effect was found on one EQ8-class mount and must
never be assumed to hold for any other mount -- see validate_altitude_effect()'s docstring for the
full reasoning (including a concrete mistake made investigating THIS mount's own data: further
slicing the same finite dataset by other variables kept turning up new-looking "effects" that were
really just small-sample noise). Every run of this script now ends with a pre-specified, automatic
significance gate; `dec_alt_trust_table` is only ever written to the output JSON if the gate passes
for that mount's own pooled data. If it doesn't pass -- expected for most mounts, not a failure --
no table is written, and WormGearGuider::decAltTrustMultiplier() already returns 1.0 (no adjustment)
when no table is loaded, so "this mount doesn't show the effect" requires no special handling
anywhere, online or offline.

Usage:
    python pool_ai_debug_dec.py --csv-dir ~/.local/share/kstars/ai_debug_logs \\
        --fixed-physics-from ~/.local/share/kstars/ai_guider_weights.json \\
        --before 20260828 --output dec_trust_csv.json

SPDX-License-Identifier: GPL-2.0-or-later
"""

import argparse
import csv
import glob
import json
import sys
from pathlib import Path

import numpy as np
from scipy import stats as scipy_stats


def load_ai_debug_csv(path):
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def file_date_str(path):
    """ai_guider_YYYYMMDD_HHMMSS.csv -> 'YYYYMMDD'."""
    parts = Path(path).stem.split("_")
    return parts[-2] if len(parts) >= 2 else None


def physics_dec_rate_px_s(altitude_deg, parallactic_angle_deg, d_polar, k_ref_dec):
    """Must match WormGearGuider::physicsDECBase() exactly: d_polar + k_ref_dec*sin(q)/cos^2(alt)."""
    alt_rad = np.radians(altitude_deg)
    q_rad = np.radians(parallactic_angle_deg)
    cos_alt = np.maximum(np.abs(np.cos(alt_rad)), 1e-4)
    return d_polar + k_ref_dec * np.sin(q_rad) / (cos_alt ** 2)


def segment_by_time_gap(rows, gap_threshold_s=30.0, min_segment_frames=10):
    """
    Split a file's rows into continuous runs. A big t_session jump means guiding was aborted and
    recalibrated in between (no frames logged during calibration -- see the capture-stall bug noted
    in reference_hardware_safety_protocol.md), so nothing before/after the gap may be treated as
    temporally adjacent by a causal filter. Also drops any row with non-positive dt or missing
    fields (older/newer CSV schema mismatches, blank trailing rows, etc).
    """
    segments = []
    cur = []
    prev_t = None
    for r in rows:
        try:
            t = float(r["t_session"])
            dt = float(r["dt"])
            alt = float(r["altitude_deg"])
            q = float(r["parallactic_angle_deg"])
            delta = float(r["uncorrected_dec_delta_px"])
        except (ValueError, KeyError, TypeError):
            continue
        if dt <= 0:
            continue
        if prev_t is not None and (t - prev_t) > gap_threshold_s:
            if len(cur) >= min_segment_frames:
                segments.append(cur)
            cur = []
        cur.append((alt, q, dt, delta))
        prev_t = t
    if len(cur) >= min_segment_frames:
        segments.append(cur)
    return segments


def residual_series_for_segment(segment, d_polar, k_ref_dec):
    """Returns (altitude_deg array, physics-residual DEC rate array), one entry per frame."""
    alt = np.array([s[0] for s in segment])
    q = np.array([s[1] for s in segment])
    dt = np.array([s[2] for s in segment])
    delta = np.array([s[3] for s in segment])
    rate = delta / dt
    phys_rate = physics_dec_rate_px_s(alt, q, d_polar, k_ref_dec)
    return alt, rate - phys_rate


def causal_ema_errors(residual, alpha):
    """
    Per-frame squared errors of a causal (past-only) EMA one-step-ahead prediction vs. the
    zero-prediction baseline (trusting the fixed physics term alone). Returns parallel arrays so the
    caller can bucket contributions by each frame's own altitude afterward. EMA state starts at 0 at
    the beginning of the array -- callers must never call this across a segment boundary.
    """
    ema = 0.0
    err_model = np.empty(len(residual))
    err_baseline = np.empty(len(residual))
    for i, r in enumerate(residual):
        err_model[i] = (r - ema) ** 2
        err_baseline[i] = r * r
        ema = alpha * r + (1.0 - alpha) * ema
    return err_model, err_baseline


def validate_altitude_effect(bin_results, min_nights_per_bin=6, alpha_sig=0.05, min_effect_r2=0.05):
    """
    Statistical gate: does THIS mount's own pooled data actually show a real, altitude-linked DEC
    residual predictability effect, or is it noise? This exists because of a concrete mistake made
    investigating the EQ8 this table was originally built for (2026-08-29): after the raw altitude
    effect held up, further slicing the SAME dataset (by hour angle, then by pier side controlled for
    altitude) kept turning up new "findings" -- including one, a pier-side x near-zenith interaction,
    that looked dramatic (a 0.30 R^2 gap) but came from a 6-vs-4-night sub-slice with no correction
    for having checked several such slices. Smaller partitions of the same finite dataset will
    eventually hand you something that looks like a pattern. The fix isn't "be more careful eyeballing
    it" -- it's a pre-specified, automatic, mount-agnostic test that runs the same way every time and
    can say "no effect detected" as easily as "effect detected." No mount's DEC behavior should be
    assumed to look like any other mount's (see this file's other constants' comments for the same
    principle applied elsewhere) -- this is that principle applied to the trust table itself: if a
    mount doesn't clear this bar, `dec_alt_trust_table` is omitted entirely from its weights.json, and
    decAltTrustMultiplier() already returns 1.0 (no adjustment) when the table is absent -- abstaining
    is a free, safe, already-built code path, not something that needs separate handling.

    Two pre-specified (not cherry-picked) tests, both required to pass:
      1. Trend test: OLS regression of per-night-held-out fold R^2 against altitude, pooling every
         bin's folds together (uses ALL the data, most statistical power). Requires slope p <
         alpha_sig.
      2. Extreme-bin test: Mann-Whitney U (non-parametric -- robust to the single-outlier-fold
         problem a t-test is vulnerable to, e.g. the 0.91 fold found in one EQ8 bin) comparing the
         LOWEST-altitude bin's folds against the HIGHEST-altitude bin's folds specifically -- not
         whichever two bins happen to show the biggest gap post-hoc, which would just reintroduce the
         same fishing problem this function exists to prevent.
    Both bins used in test 2 must have >= min_nights_per_bin independent nights of data; below that,
    abstain outright regardless of how the numbers look -- the whole point is not trusting a pattern
    built on too few independent trials, a lesson from directly observing how much a single pair's
    delta swings with n=2-3 live A/B trials this same night.
    """
    valid_bins = [b for b in bin_results.values() if not np.isnan(b["r2"])]
    if len(valid_bins) < 2:
        return {"validated": False, "reason": "fewer than 2 populated altitude bins"}

    all_alts, all_r2s = [], []
    for b in valid_bins:
        mid = (b["alt_lo"] + b["alt_hi"]) / 2.0
        for fold in b["per_fold_r2"]:
            all_alts.append(mid)
            all_r2s.append(fold)
    if len(all_alts) < 10:
        return {"validated": False, "reason": f"only {len(all_alts)} total per-night folds across all bins"}

    slope, intercept, r, p_trend, se = scipy_stats.linregress(all_alts, all_r2s)

    ordered = sorted(valid_bins, key=lambda b: b["alt_lo"])
    lo_bin, hi_bin = ordered[0], ordered[-1]
    if lo_bin["n_nights_with_data"] < min_nights_per_bin or hi_bin["n_nights_with_data"] < min_nights_per_bin:
        return {
            "validated": False,
            "reason": f"lowest/highest altitude bin has fewer than {min_nights_per_bin} nights "
                      f"({lo_bin['n_nights_with_data']}, {hi_bin['n_nights_with_data']})",
            "p_trend": float(p_trend),
        }

    lo_folds = np.array(lo_bin["per_fold_r2"])
    hi_folds = np.array(hi_bin["per_fold_r2"])
    try:
        _, p_extreme = scipy_stats.mannwhitneyu(lo_folds, hi_folds, alternative="two-sided")
    except ValueError:
        return {"validated": False, "reason": "Mann-Whitney U test failed (degenerate input)"}

    effect_r2 = hi_bin["r2"] - lo_bin["r2"]
    validated = (p_trend < alpha_sig) and (p_extreme < alpha_sig) and (abs(effect_r2) >= min_effect_r2)

    reason = None
    if not validated:
        if p_trend >= alpha_sig:
            reason = f"trend test p={p_trend:.3f} >= {alpha_sig}"
        elif p_extreme >= alpha_sig:
            reason = f"extreme-bin test p={p_extreme:.3f} >= {alpha_sig}"
        else:
            reason = f"effect size {abs(effect_r2):.3f} < {min_effect_r2} floor"

    return {
        "validated": bool(validated),
        "p_trend": float(p_trend),
        "p_extreme_bins": float(p_extreme),
        "effect_r2": float(effect_r2),
        "low_bin_alt": [lo_bin["alt_lo"], lo_bin["alt_hi"]],
        "high_bin_alt": [hi_bin["alt_lo"], hi_bin["alt_hi"]],
        "low_bin_n_nights": lo_bin["n_nights_with_data"],
        "high_bin_n_nights": hi_bin["n_nights_with_data"],
        "reason": reason,
    }


def in_bin(alt_arr, lo, hi, is_last_bin):
    if is_last_bin:
        return (alt_arr >= lo) & (alt_arr <= hi)
    return (alt_arr >= lo) & (alt_arr < hi)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--csv-dir", type=Path, required=True)
    p.add_argument("--fixed-physics-from", type=Path, required=True,
                   help="ai_guider_weights.json-format file to take d_polar/k_ref_dec from.")
    p.add_argument("--before", type=str, default=None,
                   help="YYYYMMDD -- only include files with this date prefix strictly before it. "
                        "Use to exclude nights collected after the online DEC filter went live, so "
                        "its own estimate can't leak into what should be a clean physics-only "
                        "residual (see module docstring).")
    p.add_argument("--gap-threshold-s", type=float, default=30.0)
    p.add_argument("--n-bins", type=int, default=5)
    p.add_argument("--alpha-grid", type=str, default="0.005,0.01,0.02,0.05,0.08,0.1,0.15,0.2,0.3")
    p.add_argument("--min-frames-per-night", type=int, default=50)
    p.add_argument("--output", type=Path, default=None)
    p.add_argument("--verbose", action="store_true")
    args = p.parse_args()

    physics = json.loads(args.fixed_physics_from.read_text())["physics"]
    d_polar, k_ref_dec = physics["d_polar"], physics["k_ref_dec"]
    print(f"=== Fixed physics: d_polar={d_polar:.4e} k_ref_dec={k_ref_dec:.4e} "
          f"(from {args.fixed_physics_from.name}) ===")

    paths = sorted(glob.glob(str(args.csv_dir / "ai_guider_*.csv")))
    if args.before:
        paths = [pth for pth in paths if (file_date_str(pth) or "99999999") < args.before]
    print(f"\n=== Loading {len(paths)} CSV files (before={args.before}) ===")

    night_segments = {}
    all_alts = []
    for path in paths:
        rows = load_ai_debug_csv(path)
        segments = segment_by_time_gap(rows, args.gap_threshold_s)
        series = [residual_series_for_segment(seg, d_polar, k_ref_dec) for seg in segments]
        n_frames = sum(len(a) for a, _ in series)
        if n_frames < args.min_frames_per_night:
            if args.verbose:
                print(f"  [SKIP] {Path(path).name}: only {n_frames} usable frames")
            continue
        night_segments[Path(path).name] = series
        all_alts.append(np.concatenate([a for a, _ in series]))
        print(f"  {Path(path).name}: {len(segments)} segments, {n_frames} frames")

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

    # Precompute each (night, alpha) causal-EMA walk once -- LOOCV folds below only ever slice and
    # sum these cached per-frame errors by altitude mask, never re-walk the EMA recursion.
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

    print(f"\n=== Leave-one-night-out validation across {len(nights)} nights ===")
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

    print(f"\n=== Mount-agnostic significance gate: is this effect real for THIS mount, or noise? ===")
    gate = validate_altitude_effect(bin_results)
    if gate["validated"]:
        print(f"  VALIDATED: trend p={gate['p_trend']:.4f}, extreme-bin p={gate['p_extreme_bins']:.4f}, "
              f"effect size {gate['effect_r2']:+.3f} R^2 "
              f"(alt [{gate['low_bin_alt'][0]:.1f},{gate['low_bin_alt'][1]:.1f}] "
              f"n_nights={gate['low_bin_n_nights']} vs "
              f"alt [{gate['high_bin_alt'][0]:.1f},{gate['high_bin_alt'][1]:.1f}] "
              f"n_nights={gate['high_bin_n_nights']}). "
              f"A dec_alt_trust_table is warranted for this mount's data.")
    else:
        print(f"  NOT VALIDATED: {gate['reason']}. No altitude effect detected for this mount at this "
              f"sample size -- do NOT emit a dec_alt_trust_table. Loading no table is the correct, "
              f"already-supported outcome (decAltTrustMultiplier() returns 1.0, no adjustment) --"
              f" this is the expected, common result for most mounts, not a failure of this script.")

    if args.output:
        out = {
            "fixed_physics_from": str(args.fixed_physics_from),
            "before": args.before,
            "nights_used": nights,
            "n_bins": args.n_bins,
            "bin_edges": [float(e) for e in bin_edges],
            "alpha_grid": alphas,
            "dec_altitude_gate": gate,
        }
        if gate["validated"]:
            out["dec_residual_trust"] = [{"bin": b, **r} for b, r in sorted(bin_results.items())]
            print(f"\n  Altitude-binned DEC trust table written to {args.output}")
        else:
            print(f"\n  Gate result (no table) written to {args.output} -- "
                  f"do not copy another mount's dec_alt_trust_table into this one's weights.json.")
        args.output.write_text(json.dumps(out, indent=2))


if __name__ == "__main__":
    main()
