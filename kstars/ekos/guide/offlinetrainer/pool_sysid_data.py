#!/usr/bin/env python3
"""
pool_sysid_data.py — Stage 0 of the AI Guider v2 architecture proposal
(/home/stellarmate/ai_guider_v2_architecture.md, section 4 and section 15).

Pools free_drift data across multiple nights of sysid collection for the SAME physical mount,
phase-aligning each night's cycles to a common reference (each night's own fitted fundamental
period + phase) before pooling by phase bin. Raw, unaligned pooling would wash the PE signal out
-- different nights start at different, unknown worm positions -- so alignment is the whole point
of this script, not an optional refinement.

This is deliberately standalone and produces no runtime-facing output: its only job is to answer
the go/no-go question in the architecture doc's section 14.1 -- does phase-aligned pooling across
nights actually sharpen the shape estimate (lower per-phase-bin variance / clearer structure) than
any single night's own data provides? Everything else in the v2 design depends on this being true.

Usage:
    python pool_sysid_data.py --sysid-dir ~/.local/share/kstars/ai_training_logs --axis ra
    python pool_sysid_data.py --sysid-dir ~/.local/share/kstars/ai_training_logs --axis dec --post-backlash-fix-after "2026-08-25T22:34:00"

SPDX-License-Identifier: GPL-2.0-or-later
"""

import argparse
import glob
import json
import sys
from pathlib import Path
from datetime import datetime

import numpy as np
import scipy.stats

sys.path.insert(0, str(Path(__file__).parent))
from train_worm_gear import _estimate_pe_from_fft, _estimate_refraction, _estimate_dec_drift, N_HARMONICS


def load_sysid_files(sysid_dir: Path):
    """Load every sysid_data_*.json in the directory, skipping ones with no free_drift data."""
    loaded = []
    for path in sorted(glob.glob(str(sysid_dir / "sysid_data_*.json"))):
        try:
            d = json.load(open(path))
        except Exception as e:
            print(f"  [SKIP] {Path(path).name}: failed to parse ({e})")
            continue
        n_fd = sum(len(s.get("frames", [])) for s in d.get("sessions", []) if s.get("type") == "free_drift")
        if n_fd < 20:
            print(f"  [SKIP] {Path(path).name}: only {n_fd} free_drift frames, too few to fit a phase reference")
            continue
        loaded.append((path, d))
    return loaded


def filter_same_mount(loaded, verbose=True):
    """
    Keep only files whose equipment fingerprint (mount_name, pixel_scale, focal_length) matches
    the majority value across the set. Pooling across genuinely different physical rigs would be
    a real, silent correctness bug -- their worm gears have nothing in common.
    """
    fingerprints = []
    for path, d in loaded:
        eq = d.get("equipment", {})
        fp = (eq.get("mount_name"), eq.get("pixel_scale_arcsec_per_px"), eq.get("focal_length_mm"))
        fingerprints.append(fp)

    from collections import Counter
    counts = Counter(fingerprints)
    majority_fp, majority_count = counts.most_common(1)[0]

    kept = []
    for (path, d), fp in zip(loaded, fingerprints):
        if fp == majority_fp:
            kept.append((path, d))
        else:
            if verbose:
                print(f"  [EXCLUDE] {Path(path).name}: fingerprint {fp} != majority {majority_fp}")
    if verbose:
        print(f"  [Mount identity] {len(kept)}/{len(loaded)} files match fingerprint "
              f"mount={majority_fp[0]!r} pixel_scale={majority_fp[1]} focal={majority_fp[2]}mm")
    return kept


def parse_collection_time(path: str):
    """sysid_data_YYYYMMDD_HHMMSS.json -> datetime. Used for the DEC post-backlash-fix cutoff."""
    stem = Path(path).stem  # sysid_data_20260826_002358
    parts = stem.split("_")
    try:
        return datetime.strptime(parts[-2] + parts[-1], "%Y%m%d%H%M%S")
    except (ValueError, IndexError):
        return None


def fit_night_reference(sysid: dict, guide_exp: float = 2.0, verbose: bool = False):
    """
    Fit this night's own period, fundamental phase, and refraction/polar-drift coefficients --
    the reference frame every one of this night's frames gets re-expressed against before
    pooling. Reuses the exact, already-validated period/refraction estimators from
    train_worm_gear.py (no reason to re-derive logic that's already correct and tested), and adds
    only the phase fit, which those functions don't need for their own (single-night) purposes.
    """
    pe_period, pe_harmonics = _estimate_pe_from_fft(sysid, guide_exp, False)
    k_ref, d_ra_extra = _estimate_refraction(sysid, guide_exp, False)
    d_polar, k_ref_dec = _estimate_dec_drift(sysid, guide_exp, k_ref, False)

    free_drift_sessions = [s for s in sysid["sessions"] if s["type"] == "free_drift"]
    longest = max(free_drift_sessions, key=lambda s: len(s["frames"]))
    frames = longest["frames"]

    t_vals = np.zeros(len(frames))
    ra_vals = np.zeros(len(frames))
    t = 0.0
    for i, f in enumerate(frames):
        t += f.get("dt", guide_exp)
        t_vals[i] = t
        ra_vals[i] = f["ra_raw_px"]

    # Same linear-detrend assumption _estimate_pe_from_fft already relies on (refraction rate is
    # ~constant over one ~10min free_drift session) -- reusing it here keeps this prototype
    # consistent with the existing, already-relied-upon code rather than introducing a second,
    # separately-unvalidated detrending assumption.
    slope, intercept, _, _, _ = scipy.stats.linregress(t_vals, ra_vals)
    ra_detrended = ra_vals - (slope * t_vals + intercept)

    omega = 2 * np.pi / pe_period
    X = np.column_stack([np.sin(omega * t_vals), np.cos(omega * t_vals)])
    coef, _, _, _ = np.linalg.lstsq(X, ra_detrended, rcond=None)
    sin_c, cos_c = coef
    phi = float(np.arctan2(cos_c, sin_c))  # matches the sign convention used elsewhere in this file

    if verbose:
        print(f"    period={pe_period:.1f}s  phi={phi:.3f}rad  k_ref={k_ref:.2e}  d_ra_extra={d_ra_extra:.2e}")

    return {
        "period": pe_period,
        "phi": phi,
        "k_ref": k_ref,
        "d_ra_extra": d_ra_extra,
        "d_polar": d_polar,
        "k_ref_dec": k_ref_dec,
    }


def build_pooled_samples(path, sysid, ref, guide_exp=2.0):
    """
    For every frame in every free_drift session of this night, compute (phase, detrended_value,
    altitude), phase-aligned against this night's own fitted reference. Uses ALL free_drift
    sessions in the file, not just the one used to fit the reference (more pooled data, and the
    reference session's own frames are a fair, unbiased subset to include too -- the reference fit
    doesn't overfit to individual frame noise the way a per-frame lookup would).
    """
    T = ref["period"]
    phi = ref["phi"]
    omega = 2 * np.pi / T

    samples = []
    for s in sysid["sessions"]:
        if s["type"] != "free_drift":
            continue
        frames = s["frames"]
        if len(frames) < 5:
            continue
        alt_deg = s.get("altitude_deg", 45.0)

        t_vals = np.zeros(len(frames))
        ra_vals = np.zeros(len(frames))
        t = 0.0
        for i, f in enumerate(frames):
            t += f.get("dt", guide_exp)
            t_vals[i] = t
            ra_vals[i] = f["ra_raw_px"]

        slope, intercept, _, _, _ = scipy.stats.linregress(t_vals, ra_vals)
        detrended = ra_vals - (slope * t_vals + intercept)

        phase = (omega * t_vals + phi) % (2 * np.pi)
        for p, v in zip(phase, detrended):
            samples.append((float(p), float(v), alt_deg, Path(path).name))
    return samples


def phase_bin_stats(samples, n_bins=40):
    """Per-phase-bin mean/std/count, for comparing individual-night vs pooled sharpness."""
    phases = np.array([s[0] for s in samples])
    values = np.array([s[1] for s in samples])
    bin_edges = np.linspace(0, 2 * np.pi, n_bins + 1)
    bin_idx = np.clip(np.digitize(phases, bin_edges) - 1, 0, n_bins - 1)

    means = np.full(n_bins, np.nan)
    stds = np.full(n_bins, np.nan)
    counts = np.zeros(n_bins, dtype=int)
    for b in range(n_bins):
        vals = values[bin_idx == b]
        counts[b] = len(vals)
        if len(vals) > 0:
            means[b] = vals.mean()
            if len(vals) > 1:
                stds[b] = vals.std()
    return bin_edges, means, stds, counts


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--sysid-dir", type=Path, required=True)
    p.add_argument("--axis", choices=["ra"], default="ra",
                   help="Only RA is implemented -- DEC's dominant behavior isn't phase-periodic "
                        "(see architecture doc section 8), and per section 4.5/17 there's currently "
                        "only 1 post-backlash-fix night, not enough to meaningfully pool yet anyway.")
    p.add_argument("--n-bins", type=int, default=40)
    p.add_argument("--output", type=Path, default=None,
                   help="If set, write the pooled (phase, value, altitude, night) samples as JSON.")
    p.add_argument("--verbose", action="store_true")
    args = p.parse_args()

    print(f"=== Loading sysid files from {args.sysid_dir} ===")
    loaded = load_sysid_files(args.sysid_dir)
    print(f"  {len(loaded)} files have usable free_drift data")

    print(f"\n=== Filtering to same physical mount ===")
    kept = filter_same_mount(loaded)

    print(f"\n=== Fitting per-night phase/period reference ===")
    night_refs = []
    for path, sysid in kept:
        print(f"  {Path(path).name}:")
        ref = fit_night_reference(sysid, verbose=True)
        night_refs.append((path, sysid, ref))

    print(f"\n=== Building pooled, phase-aligned dataset ===")
    all_samples = []
    per_night_samples = {}
    for path, sysid, ref in night_refs:
        samples = build_pooled_samples(path, sysid, ref)
        per_night_samples[path] = samples
        all_samples.extend(samples)
        print(f"  {Path(path).name}: {len(samples)} samples")
    print(f"  TOTAL POOLED: {len(all_samples)} samples across {len(night_refs)} nights")

    # --- The actual go/no-go validation (architecture doc section 14.1) ---
    print(f"\n=== Validation: does pooling sharpen the shape estimate? ===")
    _, pooled_means, pooled_stds, pooled_counts = phase_bin_stats(all_samples, args.n_bins)
    pooled_valid = pooled_counts > 0
    # Standard error of the per-bin mean is the right sharpness metric here (not raw std) --
    # pooling more samples into a bin should shrink our UNCERTAINTY about that bin's true mean,
    # even if the underlying per-frame noise (std) doesn't change.
    pooled_sem = np.where(pooled_counts > 1, pooled_stds / np.sqrt(np.maximum(pooled_counts, 1)), np.nan)
    print(f"  Pooled: {pooled_valid.sum()}/{args.n_bins} bins populated, "
          f"mean per-bin SEM = {np.nanmean(pooled_sem):.4f}px, "
          f"mean per-bin count = {np.nanmean(pooled_counts[pooled_valid]):.1f}")

    single_night_sems = []
    for path, samples in per_night_samples.items():
        if len(samples) < args.n_bins * 2:
            continue
        _, m, s, c = phase_bin_stats(samples, args.n_bins)
        valid = c > 1
        sem = np.where(c > 1, s / np.sqrt(np.maximum(c, 1)), np.nan)
        mean_sem = np.nanmean(sem)
        single_night_sems.append(mean_sem)
        print(f"  {Path(path).name} alone: {valid.sum()}/{args.n_bins} bins populated, "
              f"mean per-bin SEM = {mean_sem:.4f}px, mean per-bin count = {np.nanmean(c[valid]):.1f}")

    if single_night_sems:
        best_single = min(single_night_sems)
        pooled_sharpness = np.nanmean(pooled_sem)
        print(f"\n  Best single night SEM: {best_single:.4f}px")
        print(f"  Pooled SEM:            {pooled_sharpness:.4f}px")
        if pooled_sharpness < best_single:
            print(f"  RESULT: pooling sharpens the estimate by {100*(1 - pooled_sharpness/best_single):.0f}% "
                  f"vs the best single night. Stage 0 premise holds -- proceed to section 5 (Shape Net).")
        else:
            print(f"  RESULT: pooling did NOT sharpen the estimate vs the best single night. "
                  f"Stage 0 premise does NOT hold as measured -- revisit before building further "
                  f"(architecture doc section 15 treats each stage as a real go/no-go decision).")

    if args.output:
        out = {
            "n_bins": args.n_bins,
            "nights": [Path(p).name for p, _, _ in night_refs],
            "night_references": {Path(p).name: r for p, _, r in night_refs},
            "samples": [{"phase": p, "value": v, "altitude_deg": a, "night": n} for p, v, a, n in all_samples],
        }
        args.output.write_text(json.dumps(out, indent=2))
        print(f"\n  Pooled dataset written to {args.output}")


if __name__ == "__main__":
    main()
