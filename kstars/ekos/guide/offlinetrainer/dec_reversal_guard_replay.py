#!/usr/bin/env python3
"""
dec_reversal_guard_replay.py -- offline replay to validate the core assumption behind a proposed
"reversal guard" controller (see DEC_ALTITUDE_THEORY_LOG.md, theory #6: backlash/stiction) before
writing any live pulse-decision code.

THE ASSUMPTION: pool_backlash_by_altitude.py showed DEC guide-correction direction reversals (sign
changes in dec_error_arcsec) trigger a real, symmetric (both directions equally) 1.44x residual excess
-- i.e. classical mechanical backlash, re-engaged by ANY direction change. A reversal-guard controller
would try to suppress/delay reversals that look like noise dither while still letting real ones
through immediately -- using physicsDECBase()'s predicted natural drift SIGN as the discriminator: if
the star is genuinely drifting the way physics says it should, a reversal is real; if the observed
error momentarily flips the OTHER way, physics disagrees and it's a noise-suspect candidate for
suppression.

This script does NOT simulate the closed loop (that would require knowing how the star would actually
have moved under a different controller, which these logs can't tell us) -- it only replays the
already-recorded reversal events and classifies each one as physics-agreeing or physics-disagreeing,
plus how big/how long the disagreeing ones persisted. That's enough to tell us whether the assumption
holds at all, and to size the guard's thresholds, before ever writing pulse-decision code.

Physics correction sign: physicsDECBase() gives the predicted DEC DRIFT rate if uncorrected. A
correction pushes AGAINST that drift, so the "physics-predicted correction direction" is
-sign(physics_dec_rate). The observed guide error dec_error_arcsec also grows in the direction the
star is drifting, so "physics agrees with this reversal" reduces to: sign(dec_error_arcsec at the
reversal) == sign(physics_dec_rate at the reversal) -- the star drifted the way physics said it would.

SPDX-License-Identifier: GPL-2.0-or-later
"""

import argparse
import csv
import glob
import json
import sys
from pathlib import Path

import numpy as np


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
    segments = []
    cur = []
    prev_t = None
    for r in rows:
        try:
            t = float(r["t_session"])
            dt = float(r["dt"])
            alt = float(r["altitude_deg"])
            q = float(r["parallactic_angle_deg"])
            err = float(r["dec_error_arcsec"])
        except (ValueError, KeyError, TypeError):
            continue
        if dt <= 0:
            continue
        if prev_t is not None and (t - prev_t) > gap_threshold_s:
            if len(cur) >= min_segment_frames:
                segments.append(cur)
            cur = []
        cur.append((t, dt, alt, q, err))
        prev_t = t
    if len(cur) >= min_segment_frames:
        segments.append(cur)
    return segments


def classify_reversals(segment, d_polar, k_ref_dec, min_error_arcsec):
    """Returns a list of dicts, one per qualifying reversal in this segment: altitude, whether physics
    agrees with the new direction, and the peak magnitude / duration of the opposing run that preceded
    it (i.e. how long/how far the error had been building against the OLD locked direction before the
    flip -- exactly what a guard's threshold+hold-time would have to clear)."""
    t = np.array([s[0] for s in segment])
    alt = np.array([s[2] for s in segment])
    q = np.array([s[3] for s in segment])
    err = np.array([s[4] for s in segment])
    phys_rate = physics_dec_rate_px_s(alt, q, d_polar, k_ref_dec)

    sign = np.sign(err)
    big_enough = np.abs(err) >= min_error_arcsec
    events = []
    run_start_idx = 0  # start of the current same-signed run (only advances on a qualifying reversal)
    for i in range(1, len(err)):
        if sign[i] != sign[i - 1] and sign[i] != 0 and sign[i - 1] != 0 and big_enough[i] and big_enough[i - 1]:
            # a qualifying reversal at frame i: the opposing run was [run_start_idx, i-1]
            opposing_run = err[run_start_idx:i]
            peak_arcsec = float(np.max(np.abs(opposing_run))) if len(opposing_run) else float(np.abs(err[i - 1]))
            duration_s = float(t[i - 1] - t[run_start_idx]) if i - 1 > run_start_idx else 0.0
            physics_agrees = bool(np.sign(err[i]) == np.sign(phys_rate[i]))
            events.append({
                "altitude": float(alt[i]),
                "physics_agrees": physics_agrees,
                "peak_arcsec_before_flip": peak_arcsec,
                "duration_s_before_flip": duration_s,
            })
            run_start_idx = i
    return events


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--csv-dir", type=Path, required=True)
    p.add_argument("--fixed-physics-from", type=Path, required=True)
    p.add_argument("--before", type=str, default=None)
    p.add_argument("--gap-threshold-s", type=float, default=30.0)
    p.add_argument("--min-error-arcsec", type=float, default=0.5)
    p.add_argument("--n-alt-bins", type=int, default=5)
    args = p.parse_args()

    physics = json.loads(args.fixed_physics_from.read_text())["physics"]
    d_polar, k_ref_dec = physics["d_polar"], physics["k_ref_dec"]

    paths = sorted(glob.glob(str(args.csv_dir / "ai_guider_*.csv")))
    if args.before:
        paths = [pth for pth in paths if (file_date_str(pth) or "99999999") < args.before]
    print(f"=== Loading {len(paths)} CSV files (before={args.before}) ===")

    all_events = []
    for path in paths:
        rows = load_ai_debug_csv(path)
        for seg in segment_by_time_gap(rows, args.gap_threshold_s):
            all_events.extend(classify_reversals(seg, d_polar, k_ref_dec, args.min_error_arcsec))

    if not all_events:
        print("No qualifying reversals found.")
        sys.exit(1)

    n = len(all_events)
    agrees = [e for e in all_events if e["physics_agrees"]]
    disagrees = [e for e in all_events if not e["physics_agrees"]]
    print(f"\n=== {n} total qualifying reversals ===")
    print(f"  physics-AGREES (likely genuine drift, should pass through immediately): "
          f"{len(agrees)} ({100*len(agrees)/n:.1f}%)")
    print(f"  physics-DISAGREES (noise-suspect, candidate for suppression):          "
          f"{len(disagrees)} ({100*len(disagrees)/n:.1f}%)")

    def describe(label, events):
        if not events:
            print(f"  {label}: none")
            return
        peaks = np.array([e["peak_arcsec_before_flip"] for e in events])
        durs = np.array([e["duration_s_before_flip"] for e in events])
        print(f"  {label} (n={len(events)}): peak-before-flip arcsec median={np.median(peaks):.3f} "
              f"p75={np.percentile(peaks, 75):.3f} p90={np.percentile(peaks, 90):.3f}  |  "
              f"duration-before-flip s median={np.median(durs):.1f} p75={np.percentile(durs, 75):.1f} "
              f"p90={np.percentile(durs, 90):.1f}")

    print()
    describe("physics-AGREES ", agrees)
    describe("physics-DISAGREES", disagrees)

    # how does the disagree fraction vary with altitude? (do we get more "suppressible" noise
    # reversals specifically where the backlash-excess effect was strongest, i.e. low altitude?)
    alts = np.array([e["altitude"] for e in all_events])
    disagree_mask = np.array([not e["physics_agrees"] for e in all_events])
    bin_edges = np.linspace(alts.min(), alts.max(), args.n_alt_bins + 1)
    print(f"\n=== Physics-disagreeing fraction by altitude ===")
    for b in range(args.n_alt_bins):
        lo, hi = bin_edges[b], bin_edges[b + 1]
        is_last = (b == args.n_alt_bins - 1)
        mask = (alts >= lo) & (alts <= hi if is_last else alts < hi)
        if mask.sum() == 0:
            continue
        frac = disagree_mask[mask].mean()
        print(f"  alt [{lo:.1f}, {hi:.1f}{']' if is_last else ')'}: "
              f"{100*frac:.1f}% disagree  (n={int(mask.sum())} reversals)")

    print(f"\n=== Sizing implication ===")
    if disagrees:
        peaks_d = np.array([e["peak_arcsec_before_flip"] for e in disagrees])
        print(f"  To suppress the median physics-disagreeing reversal, T_leave would need to exceed "
              f"~{np.median(peaks_d):.2f}\" (its typical peak-before-flip magnitude).")
    if agrees:
        peaks_a = np.array([e["peak_arcsec_before_flip"] for e in agrees])
        frac_would_delay = float(np.mean(peaks_a < (np.median(peaks_d) if disagrees else 0)))
        print(f"  At that same threshold, {100*frac_would_delay:.1f}% of GENUINE (physics-agreeing) "
              f"reversals would ALSO be delayed -- the false-suppression cost of that threshold choice.")


if __name__ == "__main__":
    main()
