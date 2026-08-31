#!/usr/bin/env python3
"""
dec_reversal_dwell_analysis.py -- second attempt at testing the debounce idea offline, after
dec_reversal_guard_replay.py falsified the physics-sign discriminator (see
DEC_ALTITUDE_THEORY_LOG.md). A full closed-loop counterfactual ("what would guide RMS have been under
a debounced controller") isn't reconstructible from these logs alone -- a different controller sends
different pulses, which changes the star's own subsequent motion, and we don't have a validated
mechanistic backlash model to simulate that response. But a narrower, directly answerable, and still
genuinely useful question IS answerable from existing data: how much of the observed reversal traffic
is "churn" -- a reversal followed almost immediately by another reversal back -- versus a committed,
sustained direction change? Churn is exactly the class of event a duration-based debounce COULD
suppress without cost (if it reverses back on its own within the hold window anyway, delaying it costs
nothing and avoids re-engaging backlash twice for no reason). A dominance of long-dwell, committed
reversals would mean debounce has little to work with, regardless of algorithm quality.

This uses hindsight (the time to the NEXT reversal, unknowable to a real-time controller at decision
time) deliberately -- it's not proposing a real-time algorithm, it's upper-bounding how much
exploitable churn exists at all, which is the prerequisite question before investing in a real-time
design.

Definitions:
  - "reversal" -- same qualifying sign-change definition as pool_backlash_by_altitude.py /
    dec_reversal_guard_replay.py: a dec_error_arcsec sign change with |error| >= --min-error-arcsec on
    both sides.
  - "dwell time" of a reversal -- seconds from this reversal to the NEXT qualifying reversal in the
    same continuous segment (NaN if it's the last reversal in its segment -- excluded from the churn
    fraction, since we don't know what came after truncation).
  - "churn" -- dwell time <= --window-s (default 8s, matching the empirically measured near-reversal
    disturbance window from pool_backlash_by_altitude.py).

SPDX-License-Identifier: GPL-2.0-or-later
"""

import argparse
import csv
import glob
import sys
from pathlib import Path

import numpy as np


def load_ai_debug_csv(path):
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def file_date_str(path):
    parts = Path(path).stem.split("_")
    return parts[-2] if len(parts) >= 2 else None


def segment_by_time_gap(rows, gap_threshold_s=30.0, min_segment_frames=10):
    segments = []
    cur = []
    prev_t = None
    for r in rows:
        try:
            t = float(r["t_session"])
            dt = float(r["dt"])
            alt = float(r["altitude_deg"])
            err = float(r["dec_error_arcsec"])
        except (ValueError, KeyError, TypeError):
            continue
        if dt <= 0:
            continue
        if prev_t is not None and (t - prev_t) > gap_threshold_s:
            if len(cur) >= min_segment_frames:
                segments.append(cur)
            cur = []
        cur.append((t, alt, err))
        prev_t = t
    if len(cur) >= min_segment_frames:
        segments.append(cur)
    return segments


def reversal_events(segment, min_error_arcsec):
    """Returns list of (time, altitude, peak_before_flip_arcsec, duration_before_flip_s) for each
    qualifying reversal -- duration_before_flip is exactly what a real-time hold-timer would be
    watching (how long the opposing sign has already persisted), unlike dwell-to-next-reversal which
    needs hindsight."""
    t = np.array([s[0] for s in segment])
    alt = np.array([s[1] for s in segment])
    err = np.array([s[2] for s in segment])
    sign = np.sign(err)
    big_enough = np.abs(err) >= min_error_arcsec
    events = []
    run_start_idx = 0
    for i in range(1, len(err)):
        if sign[i] != sign[i - 1] and sign[i] != 0 and sign[i - 1] != 0 and big_enough[i] and big_enough[i - 1]:
            peak = float(np.max(np.abs(err[run_start_idx:i])))
            duration = float(t[i - 1] - t[run_start_idx]) if i - 1 > run_start_idx else 0.0
            events.append((float(t[i]), float(alt[i]), peak, duration))
            run_start_idx = i
    return events


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--csv-dir", type=Path, required=True)
    p.add_argument("--before", type=str, default=None)
    p.add_argument("--gap-threshold-s", type=float, default=30.0)
    p.add_argument("--min-error-arcsec", type=float, default=0.5)
    p.add_argument("--window-s", type=float, default=8.0)
    p.add_argument("--n-alt-bins", type=int, default=5)
    args = p.parse_args()

    paths = sorted(glob.glob(str(args.csv_dir / "ai_guider_*.csv")))
    if args.before:
        paths = [pth for pth in paths if (file_date_str(pth) or "99999999") < args.before]
    print(f"=== Loading {len(paths)} CSV files (before={args.before}) ===")

    dwells, dwell_alts, dwell_peaks, dwell_durs = [], [], [], []
    for path in paths:
        rows = load_ai_debug_csv(path)
        for seg in segment_by_time_gap(rows, args.gap_threshold_s):
            events = reversal_events(seg, args.min_error_arcsec)
            for i in range(len(events) - 1):
                t_i, alt_i, peak_i, dur_i = events[i]
                t_next = events[i + 1][0]
                dwells.append(t_next - t_i)
                dwell_alts.append(alt_i)
                dwell_peaks.append(peak_i)
                dwell_durs.append(dur_i)

    if not dwells:
        print("No reversal pairs found.")
        sys.exit(1)

    dwells = np.array(dwells)
    dwell_alts = np.array(dwell_alts)
    dwell_peaks = np.array(dwell_peaks)
    dwell_durs = np.array(dwell_durs)
    n = len(dwells)
    churn = dwells <= args.window_s

    print(f"\n=== {n} reversal-to-next-reversal intervals (from reversals that have a following "
          f"reversal in the same segment) ===")
    print(f"  dwell time (s): median={np.median(dwells):.1f}  p25={np.percentile(dwells,25):.1f}  "
          f"p75={np.percentile(dwells,75):.1f}")
    print(f"  CHURN (dwell <= {args.window_s:.0f}s, i.e. reverses back before the backlash-disturbance "
          f"window from the FIRST reversal even resolves): {churn.sum()} / {n}  ({100*churn.mean():.1f}%)")

    from scipy import stats as scipy_stats
    print(f"\n=== The decisive check: does pre-flip duration (the one thing a real-time hold-timer "
          f"can actually observe) predict churn? ===")
    print(f"  pre-flip duration, CHURN reversals    (n={churn.sum()}): median={np.median(dwell_durs[churn]):.1f}s  "
          f"p75={np.percentile(dwell_durs[churn],75):.1f}s")
    print(f"  pre-flip duration, NON-CHURN reversals (n={(~churn).sum()}): median={np.median(dwell_durs[~churn]):.1f}s  "
          f"p75={np.percentile(dwell_durs[~churn],75):.1f}s")
    _, p_dur = scipy_stats.mannwhitneyu(dwell_durs[churn], dwell_durs[~churn], alternative="two-sided")
    print(f"  Mann-Whitney p={p_dur:.4g}")
    print(f"  pre-flip peak magnitude, CHURN    (n={churn.sum()}): median={np.median(dwell_peaks[churn]):.2f}\"")
    print(f"  pre-flip peak magnitude, NON-CHURN (n={(~churn).sum()}): median={np.median(dwell_peaks[~churn]):.2f}\"")
    _, p_peak = scipy_stats.mannwhitneyu(dwell_peaks[churn], dwell_peaks[~churn], alternative="two-sided")
    print(f"  Mann-Whitney p={p_peak:.4g}")

    print(f"\n=== ROC-style operating points: hold-time T_hold gated on pre-flip duration ===")
    print(f"  (a reversal is DELAYED by the debounce if its pre-flip duration <= T_hold)")
    for t_hold in [3, 5, 8, 10, 15, 20]:
        delayed = dwell_durs <= t_hold
        churn_caught = (delayed & churn).sum() / churn.sum()
        genuine_delayed = (delayed & ~churn).sum() / (~churn).sum()
        print(f"  T_hold={t_hold:4.0f}s: catches {100*churn_caught:5.1f}% of churn, "
              f"but also delays {100*genuine_delayed:5.1f}% of genuine (non-churn) reversals")

    print(f"\n=== Churn fraction and peak-before-flip by altitude ===")
    bin_edges = np.linspace(dwell_alts.min(), dwell_alts.max(), args.n_alt_bins + 1)
    for b in range(args.n_alt_bins):
        lo, hi = bin_edges[b], bin_edges[b + 1]
        is_last = (b == args.n_alt_bins - 1)
        mask = (dwell_alts >= lo) & (dwell_alts <= hi if is_last else dwell_alts < hi)
        if mask.sum() == 0:
            continue
        print(f"  alt [{lo:.1f}, {hi:.1f}{']' if is_last else ')'}: "
              f"churn={100*churn[mask].mean():.1f}%  n={int(mask.sum())}  "
              f"churn-peak median={np.median(dwell_peaks[mask & churn]) if churn[mask].any() else float('nan'):.2f}\"  "
              f"non-churn-peak median={np.median(dwell_peaks[mask & ~churn]) if (~churn[mask]).any() else float('nan'):.2f}\"")

    print(f"\n=== What this does and doesn't tell us ===")
    print(f"  {100*churn.mean():.1f}% of reversals are followed by another reversal within {args.window_s:.0f}s --")
    print(f"  an upper bound on how much 'wasted round-trip' traffic a hindsight-perfect debounce could")
    print(f"  have collapsed into single, less-frequent direction commitments. It does NOT tell us a")
    print(f"  real-time algorithm (deciding with no knowledge of the future) can actually identify these")
    print(f"  same events in advance, nor what the resulting guide RMS would be -- only whether there is")
    print(f"  enough exploitable churn in this data to make building that real-time algorithm worthwhile.")


if __name__ == "__main__":
    main()
