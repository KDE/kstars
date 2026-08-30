#!/usr/bin/env python3
"""
noise_floor_by_altitude.py -- the follow-up check flagged in pool_ai_debug_ra.py's finding
(2026-08-29): DEC and RA both showed the same altitude-dependent causal-EMA R^2 shape (negative/
near-zero at low altitude, climbing toward zenith) despite sharing no mechanism in common, arguing
for a generic confound (most plausibly seeing/SNR) rather than a DEC-specific polar-alignment effect.
This script tests that directly with a metric that needs NO physics model at all, on either axis:
the local second-difference of each axis's raw per-frame rate,
  d2[i] = rate[i-1] - 2*rate[i] + rate[i+1],  rate = uncorrected_*_delta_px / dt
d2 is the deviation from a trivial local-linear (2-point) extrapolation -- a pure roughness/noise-
floor proxy that cannot be explained by any real slow-varying signal (polar drift, refraction, worm
PE -- all far too slow to show up in a 3-frame window), so if it still shrinks with altitude on BOTH
axes, that shrinkage can only be measurement noise (SNR/seeing), not axis physics. Unlike the R^2
checks in pool_ai_debug_dec.py/pool_ai_debug_ra.py, this isn't validating a fitted predictor, so
there's no LOOCV needed -- it's a direct, unfitted, physics-free measurement, pooled across nights.

Usage:
    python noise_floor_by_altitude.py --csv-dir ~/.local/share/kstars/ai_debug_logs --before 20260828

SPDX-License-Identifier: GPL-2.0-or-later
"""

import argparse
import glob
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
from pool_ai_debug_dec import load_ai_debug_csv, file_date_str


def segment_rates(rows, gap_threshold_s=30.0, min_segment_frames=5):
    """Split into continuous runs (same guard as the other pool_ai_debug_*.py scripts: a big
    t_session jump means guiding was aborted/recalibrated in between). Returns a list of
    (altitude_deg array, ra_rate array, dec_rate array), one entry per continuous segment."""
    segments = []
    cur = []
    prev_t = None
    for r in rows:
        try:
            t = float(r["t_session"])
            dt = float(r["dt"])
            alt = float(r["altitude_deg"])
            ra_delta = float(r["uncorrected_ra_delta_px"])
            dec_delta = float(r["uncorrected_dec_delta_px"])
        except (ValueError, KeyError, TypeError):
            continue
        if dt <= 0:
            continue
        if prev_t is not None and (t - prev_t) > gap_threshold_s:
            if len(cur) >= min_segment_frames:
                segments.append(cur)
            cur = []
        cur.append((alt, ra_delta / dt, dec_delta / dt))
        prev_t = t
    if len(cur) >= min_segment_frames:
        segments.append(cur)

    out = []
    for seg in segments:
        if len(seg) < 3:
            continue
        alt = np.array([s[0] for s in seg])
        ra_rate = np.array([s[1] for s in seg])
        dec_rate = np.array([s[2] for s in seg])
        out.append((alt, ra_rate, dec_rate))
    return out


def second_diff(x):
    """d2[i] = x[i-1] - 2*x[i] + x[i+1], for the interior points only. Altitude is attributed
    using the CENTER frame i (index 1..N-2 of the input), matching where the roughness is measured."""
    return x[:-2] - 2 * x[1:-1] + x[2:]


def in_bin(alt_arr, lo, hi, is_last_bin):
    if is_last_bin:
        return (alt_arr >= lo) & (alt_arr <= hi)
    return (alt_arr >= lo) & (alt_arr < hi)


def classify_night_direction(rows, corr_threshold=0.3):
    """
    corr(time-fraction-within-file, altitude) -- a single continuously-tracked target's altitude
    moves almost monotonically over a session (rising toward, or falling away from, the meridian),
    so this is close to +-1 on most individual nights. The SIGN is what matters here: 'RISING'
    (positive -- late-session frames are the high-altitude ones) vs 'SETTING' (negative -- late-
    session frames are the LOW-altitude ones). Used to separate a genuine altitude effect from a
    time-since-session-start effect that happens to correlate with altitude in whichever direction
    that particular night ran: a real time effect should flip the apparent altitude pattern between
    the two groups; a real altitude effect should not. Returns None if too weak to classify (|r| <
    threshold) or too little data.
    """
    t_list, alt_list = [], []
    for r in rows:
        try:
            t = float(r["t_session"])
            alt = float(r["altitude_deg"])
        except (ValueError, KeyError, TypeError):
            continue
        t_list.append(t)
        alt_list.append(alt)
    if len(t_list) < 50:
        return None
    t_arr, alt_arr = np.array(t_list), np.array(alt_list)
    if t_arr.std() == 0 or alt_arr.std() == 0:
        return None
    r = float(np.corrcoef(t_arr, alt_arr)[0, 1])
    if r >= corr_threshold:
        return "RISING"
    if r <= -corr_threshold:
        return "SETTING"
    return None


def run_one_group(paths, group_name, args):
    print(f"\n{'=' * 70}\n=== {group_name} nights ({len(paths)}) ===\n{'=' * 70}")
    all_alt, all_ra_d2, all_dec_d2 = [], [], []
    per_night = {}
    for path in paths:
        rows = load_ai_debug_csv(path)
        segs = segment_rates(rows, args.gap_threshold_s)
        if not segs:
            continue
        n_frames = sum(len(a) for a, _, _ in segs)
        if n_frames < args.min_frames_per_night:
            continue

        alt_center_list, ra_d2_list, dec_d2_list = [], [], []
        for alt, ra_rate, dec_rate in segs:
            alt_center_list.append(alt[1:-1])
            ra_d2_list.append(second_diff(ra_rate))
            dec_d2_list.append(second_diff(dec_rate))
        alt_c = np.concatenate(alt_center_list)
        ra_d2 = np.concatenate(ra_d2_list)
        dec_d2 = np.concatenate(dec_d2_list)
        per_night[Path(path).name] = (alt_c, ra_d2, dec_d2)
        all_alt.append(alt_c)
        all_ra_d2.append(ra_d2)
        all_dec_d2.append(dec_d2)

    if not per_night:
        print("  No usable data in this group.")
        return None

    all_alt = np.concatenate(all_alt)
    all_ra_d2 = np.concatenate(all_ra_d2)
    all_dec_d2 = np.concatenate(all_dec_d2)
    print(f"  {len(per_night)} nights, {len(all_alt)} interior frames, "
          f"altitude range {all_alt.min():.1f}-{all_alt.max():.1f}")

    bin_edges = args.shared_bin_edges
    print(f"  {'alt bin':<16}{'RA RMS':<12}{'DEC RMS':<12}{'n_frames':<10}{'n_nights'}")
    ra_rms_list, dec_rms_list = [], []
    for b in range(len(bin_edges) - 1):
        lo, hi = float(bin_edges[b]), float(bin_edges[b + 1])
        is_last = (b == len(bin_edges) - 2)
        mask = in_bin(all_alt, lo, hi, is_last)
        n = int(mask.sum())
        ra_rms = float(np.sqrt(np.mean(all_ra_d2[mask] ** 2))) if n > 1 else float("nan")
        dec_rms = float(np.sqrt(np.mean(all_dec_d2[mask] ** 2))) if n > 1 else float("nan")
        n_nights = sum(1 for a, _, _ in per_night.values() if in_bin(a, lo, hi, is_last).any())
        ra_rms_list.append(ra_rms)
        dec_rms_list.append(dec_rms)
        label = f"[{lo:.1f},{hi:.1f}{']' if is_last else ')'}"
        print(f"  {label:<16}{ra_rms:<12.4f}{dec_rms:<12.4f}{n:<10}{n_nights}")
    return ra_rms_list, dec_rms_list


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--csv-dir", type=Path, required=True)
    p.add_argument("--before", type=str, default=None,
                   help="YYYYMMDD -- restrict to files before this date, e.g. to match the exact "
                        "night-set pool_ai_debug_dec.py/pool_ai_debug_ra.py used (--before 20260828).")
    p.add_argument("--gap-threshold-s", type=float, default=30.0)
    p.add_argument("--n-bins", type=int, default=5)
    p.add_argument("--min-frames-per-night", type=int, default=50)
    p.add_argument("--split-by-direction", action="store_true",
                   help="Split nights into RISING (late-session = high altitude) vs SETTING "
                        "(late-session = low altitude) groups and run the altitude-binned noise "
                        "floor on each separately -- see classify_night_direction()'s docstring for "
                        "why this isolates a genuine altitude effect from a time-since-session-start "
                        "confound.")
    p.add_argument("--corr-threshold", type=float, default=0.3)
    p.add_argument("--verbose", action="store_true")
    args = p.parse_args()

    paths = sorted(glob.glob(str(args.csv_dir / "ai_guider_*.csv")))
    if args.before:
        paths = [pth for pth in paths if (file_date_str(pth) or "99999999") < args.before]
    print(f"=== Loading {len(paths)} CSV files (before={args.before}) ===")

    if args.split_by_direction:
        rising, setting, ambiguous = [], [], []
        for path in paths:
            rows = load_ai_debug_csv(path)
            direction = classify_night_direction(rows, args.corr_threshold)
            if direction == "RISING":
                rising.append(path)
            elif direction == "SETTING":
                setting.append(path)
            else:
                ambiguous.append(path)
        print(f"  RISING (late=high alt): {len(rising)}   SETTING (late=low alt): {len(setting)}   "
              f"ambiguous/excluded: {len(ambiguous)}")

        # Shared bin edges across both groups (and matching the earlier un-split run's range) so
        # the two tables are directly comparable bin-for-bin, not each auto-ranged separately.
        all_alt_probe = []
        for path in paths:
            rows = load_ai_debug_csv(path)
            for a, _, _ in segment_rates(rows, args.gap_threshold_s):
                all_alt_probe.append(a)
        all_alt_probe = np.concatenate(all_alt_probe)
        args.shared_bin_edges = np.linspace(float(all_alt_probe.min()), float(all_alt_probe.max()),
                                             args.n_bins + 1)
        print(f"  Shared altitude bins: {[round(float(e), 1) for e in args.shared_bin_edges]}")

        rising_result = run_one_group(rising, "RISING", args)
        setting_result = run_one_group(setting, "SETTING", args)

        if rising_result and setting_result:
            print(f"\n{'=' * 70}\n=== Comparison ===\n{'=' * 70}")
            r_ra, r_dec = rising_result
            s_ra, s_dec = setting_result
            print(f"  RA  RISING : {[round(v, 3) for v in r_ra]}")
            print(f"  RA  SETTING: {[round(v, 3) for v in s_ra]}")
            print(f"  DEC RISING : {[round(v, 3) for v in r_dec]}")
            print(f"  DEC SETTING: {[round(v, 3) for v in s_dec]}")

            def shape_agrees(a, b):
                a_valid = [v for v in a if not np.isnan(v)]
                b_valid = [v for v in b if not np.isnan(v)]
                if len(a_valid) < 2 or len(b_valid) < 2:
                    return None
                # Same sign of (last - first) = same qualitative direction across the altitude range.
                return (a_valid[-1] - a_valid[0]) * (b_valid[-1] - b_valid[0]) > 0

            ra_agree = shape_agrees(r_ra, s_ra)
            dec_agree = shape_agrees(r_dec, s_dec)
            print(f"\n  RA  RISING vs SETTING same low->high direction: {ra_agree}")
            print(f"  DEC RISING vs SETTING same low->high direction: {dec_agree}")
            if ra_agree and dec_agree:
                print(f"\n  RESULT: the altitude pattern points the SAME way in both RISING and "
                      f"SETTING nights, i.e. it tracks ALTITUDE, not time-since-session-start (a pure "
                      f"time effect would have flipped sign between the two groups, since 'late in "
                      f"session' means opposite altitudes in each group). The altitude effect survives "
                      f"controlling for this confound.")
            elif ra_agree is False or dec_agree is False:
                print(f"\n  RESULT: the pattern reverses between RISING and SETTING nights on at least "
                      f"one axis -- consistent with a time-since-session-start effect that only LOOKED "
                      f"like an altitude effect in the pooled (un-split) data because most nights ran "
                      f"in the same direction. Do not trust the pooled altitude curve as an altitude "
                      f"effect without more investigation.")
            else:
                print(f"\n  RESULT: inconclusive (insufficient data in one or both groups).")
        return

    all_alt, all_ra_d2, all_dec_d2 = [], [], []
    per_night = {}
    for path in paths:
        rows = load_ai_debug_csv(path)
        segs = segment_rates(rows, args.gap_threshold_s)
        if not segs:
            continue
        n_frames = sum(len(a) for a, _, _ in segs)
        if n_frames < args.min_frames_per_night:
            if args.verbose:
                print(f"  [SKIP] {Path(path).name}: only {n_frames} usable frames")
            continue

        alt_center_list, ra_d2_list, dec_d2_list = [], [], []
        for alt, ra_rate, dec_rate in segs:
            alt_center_list.append(alt[1:-1])
            ra_d2_list.append(second_diff(ra_rate))
            dec_d2_list.append(second_diff(dec_rate))
        alt_c = np.concatenate(alt_center_list)
        ra_d2 = np.concatenate(ra_d2_list)
        dec_d2 = np.concatenate(dec_d2_list)

        per_night[Path(path).name] = (alt_c, ra_d2, dec_d2)
        all_alt.append(alt_c)
        all_ra_d2.append(ra_d2)
        all_dec_d2.append(dec_d2)
        print(f"  {Path(path).name}: {len(segs)} segments, {len(alt_c)} usable interior frames")

    if not per_night:
        print("No usable data.")
        sys.exit(1)

    all_alt = np.concatenate(all_alt)
    all_ra_d2 = np.concatenate(all_ra_d2)
    all_dec_d2 = np.concatenate(all_dec_d2)

    alt_min, alt_max = float(all_alt.min()), float(all_alt.max())
    bin_edges = np.linspace(alt_min, alt_max, args.n_bins + 1)
    print(f"\n=== Altitude range {alt_min:.1f}-{alt_max:.1f} deg, {args.n_bins} uniform bins: "
          f"{[round(float(e), 1) for e in bin_edges]} ===")
    print(f"    (pooled across {len(per_night)} nights, {len(all_alt)} interior frames)")

    print(f"\n=== Noise-floor RMS(second-difference of rate) per altitude bin, physics-free ===")
    print(f"    {'alt bin':<16}{'RA RMS (px/s)':<18}{'DEC RMS (px/s)':<18}{'n_frames':<10}{'n_nights'}")
    ra_rms_list, dec_rms_list = [], []
    for b in range(args.n_bins):
        lo, hi = float(bin_edges[b]), float(bin_edges[b + 1])
        is_last = (b == args.n_bins - 1)
        mask = in_bin(all_alt, lo, hi, is_last)
        n = int(mask.sum())
        ra_rms = float(np.sqrt(np.mean(all_ra_d2[mask] ** 2))) if n > 1 else float("nan")
        dec_rms = float(np.sqrt(np.mean(all_dec_d2[mask] ** 2))) if n > 1 else float("nan")
        n_nights = sum(1 for a, _, _ in per_night.values() if in_bin(a, lo, hi, is_last).any())
        ra_rms_list.append(ra_rms)
        dec_rms_list.append(dec_rms)
        label = f"[{lo:.1f},{hi:.1f}{']' if is_last else ')'}"
        print(f"    {label:<16}{ra_rms:<18.5f}{dec_rms:<18.5f}{n:<10}{n_nights}")

    def trend_str(vals):
        valid = [v for v in vals if not np.isnan(v)]
        if len(valid) < 2:
            return "insufficient data"
        return f"{valid[0]:.5f} -> {valid[-1]:.5f}  ({100*(valid[-1]/valid[0]-1):+.0f}%)"

    print(f"\n  RA  noise floor, low-alt -> high-alt: {trend_str(ra_rms_list)}")
    print(f"  DEC noise floor, low-alt -> high-alt: {trend_str(dec_rms_list)}")

    ra_range = (max(ra_rms_list) - min(v for v in ra_rms_list if not np.isnan(v)))
    dec_range = (max(dec_rms_list) - min(v for v in dec_rms_list if not np.isnan(v)))
    if ra_rms_list[0] > ra_rms_list[-1] and dec_rms_list[0] > dec_rms_list[-1]:
        print(f"\n  RESULT: noise floor DECREASES with altitude on BOTH axes -- consistent with a "
              f"shared seeing/SNR confound explaining most of the earlier R^2-vs-altitude pattern on "
              f"both RA and DEC (less atmosphere near zenith -> cleaner centroid measurements -> "
              f"less irreducible frame-to-frame noise for a causal filter to fight, independent of "
              f"axis physics).")
    else:
        print(f"\n  RESULT: noise floor does NOT monotonically decrease with altitude on both axes -- "
              f"the earlier R^2-vs-altitude pattern is NOT fully explained by a shared measurement-"
              f"noise-floor effect; something else (real axis physics, or a different confound) is "
              f"likely still contributing.")


if __name__ == "__main__":
    main()
