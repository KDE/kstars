#!/usr/bin/env python3
"""
pool_backlash_by_altitude.py -- offline test of the mechanical-loading/backlash hypothesis for the
DEC altitude effect (see pool_ai_debug_dec.py's module docstring for the full history: SNR/noise-
floor, time-in-session, pier-side, and atmospheric refraction have all been checked and ruled out or
downgraded as the cause of the validated altitude-dependent DEC residual predictability effect).

HYPOTHESIS: DEC worm-gear backlash/stiction severity depends on mechanical loading, which depends on
the gravity-vector torque on the DEC axis, which changes with pointing altitude. If true, this should
show up as a *specific, localized* signature: excess unexplained residual concentrated in the frames
immediately following a guide-correction DIRECTION REVERSAL (where any backlash gap has to be taken
up again), not spread uniformly across all frames. This is a different, more specific claim than "the
residual is altitude-dependent" (already established) -- it asks WHERE in time that dependence lives.

Two independent questions, deliberately kept separate so one doesn't get read as evidence for the
other:
  (a) Does a reversal-localized excess exist at all, pooled over all data? (a backlash/stiction
      sanity check, not itself the altitude question)
  (b) Does the SIZE of that excess (near-reversal vs baseline ratio), specifically, vary with
      altitude -- using the same pre-specified OLS-trend + extreme-bin-Mann-Whitney gate as
      validate_altitude_effect() in pool_ai_debug_dec.py, with the same min-nights-per-bin floor.
If (a) is true but (b) fails the gate, backlash/stiction may be real but is NOT the altitude-varying
piece -- something else (still unidentified) would have to be. If (a) is false, backlash isn't a
significant contributor to this residual at all, at any altitude.

A "reversal" is defined on dec_error_arcsec (the guide star's raw deviation from lock position) sign
changes, not on any commanded pulse field -- this CSV schema logs no pulse polarity/duration directly,
so dec_error's sign is the best available proxy for when the guide loop's corrective effort must
change direction. A minimum magnitude threshold on both sides of the crossing (--min-error-arcsec)
avoids counting trivial noise dither around zero as a "reversal" -- only crossings large enough to
plausibly re-engage a mechanical backlash gap count.

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
    parts = Path(path).stem.split("_")
    return parts[-2] if len(parts) >= 2 else None


def physics_dec_rate_px_s(altitude_deg, parallactic_angle_deg, d_polar, k_ref_dec):
    alt_rad = np.radians(altitude_deg)
    q_rad = np.radians(parallactic_angle_deg)
    cos_alt = np.maximum(np.abs(np.cos(alt_rad)), 1e-4)
    return d_polar + k_ref_dec * np.sin(q_rad) / (cos_alt ** 2)


def segment_by_time_gap(rows, gap_threshold_s=30.0, min_segment_frames=10):
    """Same discipline as pool_ai_debug_dec.py: never treat frames across a calibration gap as
    temporally adjacent -- both for physics-residual rate AND for reversal/time-since-reversal
    bookkeeping, which resets at zero at the start of every segment."""
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
            err = float(r["dec_error_arcsec"])
        except (ValueError, KeyError, TypeError):
            continue
        if dt <= 0:
            continue
        if prev_t is not None and (t - prev_t) > gap_threshold_s:
            if len(cur) >= min_segment_frames:
                segments.append(cur)
            cur = []
        cur.append((alt, q, dt, delta, err))
        prev_t = t
    if len(cur) >= min_segment_frames:
        segments.append(cur)
    return segments


def segment_arrays(segment, d_polar, k_ref_dec, min_error_arcsec):
    """Returns per-frame altitude, squared physics-residual rate, and a near_reversal boolean mask
    for one continuous segment."""
    alt = np.array([s[0] for s in segment])
    q = np.array([s[1] for s in segment])
    dt = np.array([s[2] for s in segment])
    delta = np.array([s[3] for s in segment])
    err = np.array([s[4] for s in segment])

    rate = delta / dt
    phys_rate = physics_dec_rate_px_s(alt, q, d_polar, k_ref_dec)
    sq_resid = (rate - phys_rate) ** 2

    t = np.concatenate(([0.0], np.cumsum(dt)[:-1]))  # time of frame i since segment start

    sign = np.sign(err)
    big_enough = np.abs(err) >= min_error_arcsec
    is_reversal = np.zeros(len(err), dtype=bool)
    is_reversal[1:] = (sign[1:] != sign[:-1]) & (sign[1:] != 0) & (sign[:-1] != 0) & \
        big_enough[1:] & big_enough[:-1]

    reversal_times = t[is_reversal]
    reversal_dirs = sign[is_reversal]  # +1 = became a "push north" error, -1 = "push south", after the flip
    if len(reversal_times) == 0:
        time_since_reversal = np.full(len(err), np.inf)
        direction_of_last_reversal = np.zeros(len(err))
    else:
        # for each frame time, time since the most recent reversal at or before it (inf if none yet)
        idx = np.searchsorted(reversal_times, t, side="right") - 1
        time_since_reversal = np.where(idx >= 0, t - reversal_times[np.clip(idx, 0, None)], np.inf)
        direction_of_last_reversal = np.where(idx >= 0, reversal_dirs[np.clip(idx, 0, None)], 0)

    return alt, sq_resid, time_since_reversal, direction_of_last_reversal, int(is_reversal.sum())


def validate_metric_altitude_effect(bin_results, min_nights_per_bin=6, alpha_sig=0.05):
    """Structurally identical gate to pool_ai_debug_dec.py's validate_altitude_effect(), adapted to a
    log-ratio metric instead of R^2 -- same reasoning applies (see that function's docstring): a
    pre-specified trend test plus an extreme-bin test, both required, with a minimum-nights floor,
    so that "no effect" is as available an outcome as "effect found."
    """
    valid_bins = [b for b in bin_results.values() if b["n_nights_with_data"] > 0]
    if len(valid_bins) < 2:
        return {"validated": False, "reason": "fewer than 2 populated altitude bins"}

    all_alts, all_vals = [], []
    for b in valid_bins:
        mid = (b["alt_lo"] + b["alt_hi"]) / 2.0
        for v in b["per_night_log_ratio"]:
            all_alts.append(mid)
            all_vals.append(v)
    if len(all_alts) < 10:
        return {"validated": False, "reason": f"only {len(all_alts)} total night-bin samples across all bins"}

    slope, intercept, r, p_trend, se = scipy_stats.linregress(all_alts, all_vals)

    ordered = sorted(valid_bins, key=lambda b: b["alt_lo"])
    lo_bin, hi_bin = ordered[0], ordered[-1]
    if lo_bin["n_nights_with_data"] < min_nights_per_bin or hi_bin["n_nights_with_data"] < min_nights_per_bin:
        return {
            "validated": False,
            "reason": f"lowest/highest altitude bin has fewer than {min_nights_per_bin} nights "
                      f"({lo_bin['n_nights_with_data']}, {hi_bin['n_nights_with_data']})",
            "p_trend": float(p_trend),
        }

    lo_vals = np.array(lo_bin["per_night_log_ratio"])
    hi_vals = np.array(hi_bin["per_night_log_ratio"])
    try:
        _, p_extreme = scipy_stats.mannwhitneyu(lo_vals, hi_vals, alternative="two-sided")
    except ValueError:
        return {"validated": False, "reason": "Mann-Whitney U test failed (degenerate input)"}

    effect = float(np.median(hi_vals) - np.median(lo_vals))
    validated = (p_trend < alpha_sig) and (p_extreme < alpha_sig)

    reason = None
    if not validated:
        reason = f"trend p={p_trend:.3f}" if p_trend >= alpha_sig else f"extreme-bin p={p_extreme:.3f}"
        reason += f" >= {alpha_sig}"

    return {
        "validated": bool(validated),
        "p_trend": float(p_trend),
        "p_extreme_bins": float(p_extreme),
        "effect_log_ratio": effect,
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
    p.add_argument("--fixed-physics-from", type=Path, required=True)
    p.add_argument("--before", type=str, default=None)
    p.add_argument("--extra-dates", type=str, default=None,
                   help="comma-separated YYYYMMDD dates to include IN ADDITION to --before, e.g. to "
                        "add specific later nights while still excluding others in between (such as "
                        "nights known to have an online filter actively toggling mid-session).")
    p.add_argument("--gap-threshold-s", type=float, default=30.0)
    p.add_argument("--n-bins", type=int, default=5)
    p.add_argument("--window-s", type=float, default=8.0,
                   help="seconds after a reversal counted as 'near_reversal' (default 8s -- a few "
                        "guide cycles at typical cadence, long enough for a backlash catch-up to "
                        "finish, short enough not to blur into unrelated later disturbance).")
    p.add_argument("--min-error-arcsec", type=float, default=0.5,
                   help="minimum |dec_error_arcsec| required on BOTH sides of a sign change to count "
                        "as a real reversal, not noise dither around zero.")
    p.add_argument("--min-frames-per-night", type=int, default=50)
    p.add_argument("--min-near-per-night-bin", type=int, default=5)
    p.add_argument("--min-base-per-night-bin", type=int, default=20)
    p.add_argument("--output", type=Path, default=None)
    p.add_argument("--verbose", action="store_true")
    args = p.parse_args()

    physics = json.loads(args.fixed_physics_from.read_text())["physics"]
    d_polar, k_ref_dec = physics["d_polar"], physics["k_ref_dec"]
    print(f"=== Fixed physics: d_polar={d_polar:.4e} k_ref_dec={k_ref_dec:.4e} "
          f"(from {args.fixed_physics_from.name}) ===")

    all_paths = sorted(glob.glob(str(args.csv_dir / "ai_guider_*.csv")))
    extra_dates = set(args.extra_dates.split(",")) if args.extra_dates else set()
    paths = [pth for pth in all_paths
             if (args.before and (file_date_str(pth) or "99999999") < args.before)
             or (file_date_str(pth) in extra_dates)]
    print(f"\n=== Loading {len(paths)} CSV files (before={args.before}, extra_dates={sorted(extra_dates)}) ===")

    night_data = {}
    all_alts = []
    total_reversals = 0
    for path in paths:
        rows = load_ai_debug_csv(path)
        segments = segment_by_time_gap(rows, args.gap_threshold_s)
        per_seg = [segment_arrays(seg, d_polar, k_ref_dec, args.min_error_arcsec) for seg in segments]
        n_frames = sum(len(a) for a, _, _, _, _ in per_seg)
        n_rev = sum(nr for _, _, _, _, nr in per_seg)
        if n_frames < args.min_frames_per_night:
            if args.verbose:
                print(f"  [SKIP] {Path(path).name}: only {n_frames} usable frames")
            continue
        night_data[Path(path).name] = per_seg
        all_alts.append(np.concatenate([a for a, _, _, _, _ in per_seg]))
        total_reversals += n_rev
        print(f"  {Path(path).name}: {len(segments)} segments, {n_frames} frames, {n_rev} reversals")

    if not night_data:
        print("No usable data.")
        sys.exit(1)

    all_alts_flat = np.concatenate(all_alts)
    alt_min, alt_max = float(all_alts_flat.min()), float(all_alts_flat.max())
    bin_edges = np.linspace(alt_min, alt_max, args.n_bins + 1)
    print(f"\n=== Altitude range {alt_min:.1f}-{alt_max:.1f} deg, {args.n_bins} uniform bins: "
          f"{[round(float(e), 1) for e in bin_edges]}, {total_reversals} qualifying reversals total ===")

    # --- (a) pooled sanity check: does a reversal-localized excess exist at all? ---
    pooled_near, pooled_base = [], []
    pooled_near_pos, pooled_near_neg = [], []  # split by the direction landed on AFTER the reversal
    for per_seg in night_data.values():
        for alt, sq_resid, tsr, dirn, _ in per_seg:
            near = tsr <= args.window_s
            pooled_near.append(sq_resid[near])
            pooled_base.append(sq_resid[~near])
            pooled_near_pos.append(sq_resid[near & (dirn > 0)])
            pooled_near_neg.append(sq_resid[near & (dirn < 0)])
    pooled_near = np.concatenate(pooled_near)
    pooled_base = np.concatenate(pooled_base)
    pooled_near_pos = np.concatenate(pooled_near_pos)
    pooled_near_neg = np.concatenate(pooled_near_neg)
    near_rms = np.sqrt(pooled_near.mean())
    base_rms = np.sqrt(pooled_base.mean())
    _, p_pooled = scipy_stats.mannwhitneyu(pooled_near, pooled_base, alternative="two-sided")
    print(f"\n=== (a) Pooled reversal-localized excess (all nights, all altitudes) ===")
    print(f"  near-reversal RMS residual: {near_rms:.4f} px  (n={len(pooled_near)})")
    print(f"  baseline RMS residual:      {base_rms:.4f} px  (n={len(pooled_base)})")
    print(f"  ratio: {near_rms/base_rms:.3f}x   Mann-Whitney p={p_pooled:.4g}")

    print(f"\n=== (a2) Is the reversal excess symmetric between the two directions? ===")
    pos_rms = np.sqrt(pooled_near_pos.mean())
    neg_rms = np.sqrt(pooled_near_neg.mean())
    _, p_dir = scipy_stats.mannwhitneyu(pooled_near_pos, pooled_near_neg, alternative="two-sided")
    print(f"  near-reversal-to-POSITIVE (dec_error crossed to +) RMS: {pos_rms:.4f} px  (n={len(pooled_near_pos)})")
    print(f"  near-reversal-to-NEGATIVE (dec_error crossed to -) RMS: {neg_rms:.4f} px  (n={len(pooled_near_neg)})")
    print(f"  ratio pos/neg: {pos_rms/neg_rms:.3f}x   Mann-Whitney p={p_dir:.4g}  "
          f"(both vs baseline {base_rms:.4f}px: {pos_rms/base_rms:.3f}x / {neg_rms/base_rms:.3f}x)")

    # --- (b) does the near/baseline ratio itself vary with altitude? ---
    print(f"\n=== (b) Per-night, per-altitude-bin near-reversal / baseline log-ratio ===")
    nights = sorted(night_data)
    bin_results = {}
    for b in range(args.n_bins):
        lo, hi = float(bin_edges[b]), float(bin_edges[b + 1])
        is_last = (b == args.n_bins - 1)
        per_night_log_ratio = []
        for night in nights:
            near_vals, base_vals = [], []
            for alt, sq_resid, tsr, dirn, _ in night_data[night]:
                mask = in_bin(alt, lo, hi, is_last)
                near = mask & (tsr <= args.window_s)
                base = mask & (tsr > args.window_s)
                near_vals.append(sq_resid[near])
                base_vals.append(sq_resid[base])
            near_vals = np.concatenate(near_vals) if near_vals else np.array([])
            base_vals = np.concatenate(base_vals) if base_vals else np.array([])
            if len(near_vals) < args.min_near_per_night_bin or len(base_vals) < args.min_base_per_night_bin:
                continue
            near_mean = near_vals.mean()
            base_mean = base_vals.mean()
            if near_mean <= 0 or base_mean <= 0:
                continue
            per_night_log_ratio.append(float(np.log(near_mean / base_mean)))

        bin_results[b] = {
            "alt_lo": lo, "alt_hi": hi,
            "n_nights_with_data": len(per_night_log_ratio),
            "per_night_log_ratio": per_night_log_ratio,
            "median_log_ratio": float(np.median(per_night_log_ratio)) if per_night_log_ratio else None,
        }
        med = bin_results[b]["median_log_ratio"]
        med_str = f"{med:+.3f}" if med is not None else "n/a"
        print(f"  alt [{lo:.1f}, {hi:.1f}{']' if is_last else ')'}: median log-ratio={med_str}  "
              f"n_nights={len(per_night_log_ratio)}/{len(nights)}")

    print(f"\n=== Mount-agnostic significance gate: does reversal-excess severity vary with altitude? ===")
    gate = validate_metric_altitude_effect(bin_results)
    if gate["validated"]:
        print(f"  VALIDATED: trend p={gate['p_trend']:.4f}, extreme-bin p={gate['p_extreme_bins']:.4f}, "
              f"effect {gate['effect_log_ratio']:+.3f} log-ratio "
              f"(alt [{gate['low_bin_alt'][0]:.1f},{gate['low_bin_alt'][1]:.1f}] "
              f"n_nights={gate['low_bin_n_nights']} vs "
              f"alt [{gate['high_bin_alt'][0]:.1f},{gate['high_bin_alt'][1]:.1f}] "
              f"n_nights={gate['high_bin_n_nights']}). Backlash/stiction severity DOES appear to vary "
              f"with altitude on this mount's data.")
    else:
        print(f"  NOT VALIDATED: {gate['reason']}. No altitude-dependence of reversal-excess severity "
              f"detected at this sample size -- if part (a) above showed a real pooled excess, backlash "
              f"may still be present, but it is not the altitude-varying component of the residual.")

    if args.output:
        out = {
            "fixed_physics_from": str(args.fixed_physics_from),
            "before": args.before,
            "window_s": args.window_s,
            "min_error_arcsec": args.min_error_arcsec,
            "nights_used": nights,
            "n_bins": args.n_bins,
            "bin_edges": [float(e) for e in bin_edges],
            "pooled_near_rms_px": float(near_rms),
            "pooled_base_rms_px": float(base_rms),
            "pooled_mannwhitney_p": float(p_pooled),
            "bin_results": {str(k): v for k, v in bin_results.items()},
            "altitude_gate": gate,
        }
        args.output.write_text(json.dumps(out, indent=2))
        print(f"\n  Full results written to {args.output}")


if __name__ == "__main__":
    main()
