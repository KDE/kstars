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

`--axis dec` (added 2026-08-29) answers a different question, motivated by a real live finding the
same night the WORM_GEAR DEC online drift tracker was built and tested: the tracker's benefit over
static physics looked altitude-dependent (strong positive ~35 deg, negative ~49 deg, flat ~59 deg)
across only 3-4 live-tested points in one night -- not enough to trust as more than a hypothesis.
DEC's dominant behavior isn't phase-periodic (see architecture doc section 8), so it reuses none of
the phase machinery above; instead it pools each night's free_drift sessions' physics-residual DEC
RATE (raw rate minus that night's own fitted physicsDECBase()), groups sessions into altitude
clusters discovered from the pooled data itself (not hardcoded altitude values -- this must stay
usable for mounts other than the one it was built on), and asks: within each altitude cluster, how
much of that residual's variance can a causal (past-only) online filter actually take out, validated
leave-one-night-out so the answer isn't just overfit alpha noise? The result is a
`dec_residual_trust(altitude)` table meant to drive an altitude-dependent multiplier on
WormGearGuider's m_kfRDec, not a belief about any single mount's constants.

Usage:
    python pool_sysid_data.py --sysid-dir ~/.local/share/kstars/ai_training_logs --axis ra
    python pool_sysid_data.py --sysid-dir ~/.local/share/kstars/ai_training_logs --axis dec --post-backlash-fix-after "2026-08-25T22:34:00"
    python pool_sysid_data.py --sysid-dir ~/.local/share/kstars/ai_training_logs --axis dec --output dec_trust.json

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


def build_dec_session_series(path, sysid, d_polar, k_ref_dec, guide_exp=2.0):
    """
    For every free_drift session in this night, compute the ordered (per-frame) physics-residual
    DEC RATE series: raw frame-to-frame dec_raw_px rate minus a physicsDECBase() prediction
    (d_polar + k_ref_dec * sin(q)/cos^2(alt)) -- exactly the quantity
    WormGearGuider::updateResidualKFDec() tracks online, computed here offline from clean free_drift
    data (no pulses, no closed-loop dynamics to disentangle).

    d_polar/k_ref_dec are passed in rather than fit here, deliberately -- see main()'s dec branch
    and --fixed-physics-from. Fitting a fresh per-night reference (like the RA path does) answers
    "how much residual is left after an optimal fit to THIS night's own data", which is nearly zero
    by construction (OLS already minimizes it) and is NOT the question the deployed online filter
    actually faces: it runs against physics constants trained once on PAST nights and never refit,
    so a real, exploitable gap can exist between deployed physics and tonight's true polar alignment
    -- confirmed by comparing the two modes on real data (2026-08-29): per-night-refit residual
    R^2 was flat and slightly negative (~-0.02) at every altitude, while the same data scored against
    the actually-deployed ai_guider_weights.json's fixed d_polar/k_ref_dec showed real, altitude-
    varying positive R^2. Always prefer --fixed-physics-from a real deployed weights file for this
    reason; per-night refit is kept only as an explicit, clearly-labeled contrast.

    Kept as one ordered array PER SESSION, not flattened into a bag of frames like the RA phase
    samples above -- a causal (past-only) filter's whole point is exploiting order, and nothing here
    may treat frames from different sessions/nights as adjacent in time.
    """
    series = []
    for s in sysid["sessions"]:
        if s["type"] != "free_drift":
            continue
        frames = s["frames"]
        if len(frames) < 5:
            continue
        alt_deg = s.get("altitude_deg", 45.0)
        alt_rad = np.radians(alt_deg)
        cos_alt = max(abs(np.cos(alt_rad)), 1e-4)

        residual = np.zeros(len(frames) - 1)
        t_prev = 0.0
        dec_prev = frames[0]["dec_raw_px"]
        for i in range(1, len(frames)):
            f = frames[i]
            dt = f.get("dt", guide_exp)
            if dt <= 0.0:
                dt = guide_exp
            dec_cur = f["dec_raw_px"]
            raw_rate = (dec_cur - dec_prev) / dt
            q_deg = f.get("parallactic_angle_deg", 0.0)
            physics_rate = d_polar + k_ref_dec * np.sin(np.radians(q_deg)) / (cos_alt ** 2)
            residual[i - 1] = raw_rate - physics_rate
            dec_prev = dec_cur

        series.append((float(alt_deg), Path(path).name, residual))
    return series


def cluster_altitudes(alts, gap_deg=8.0):
    """
    Group altitudes into clusters using single-linkage gaps, not a fixed bin count or fixed
    altitude thresholds -- this data happens to come from a sysid protocol that samples a handful
    of fixed altitudes per night (see aiguideprotocol.cpp's WORM_GEAR phase table), so uniform
    binning over the observed min/max range would routinely split or merge those natural clusters
    depending on how many bins were asked for. Discovering the clusters from the pooled gaps
    instead makes this work whether the underlying data is a few discrete altitudes (this mount) or
    genuinely continuous coverage (a future protocol/mount) -- in the continuous case this just
    degenerates to one big cluster unless gap_deg is tightened, which is a visible, not silent,
    failure mode (n_bins_found == 1 is easy to notice and re-run with a smaller gap_deg).
    """
    alts = np.asarray(alts, dtype=float)
    order = np.argsort(alts)
    sorted_alts = alts[order]
    cluster_ids_sorted = np.zeros(len(sorted_alts), dtype=int)
    cid = 0
    for i in range(1, len(sorted_alts)):
        if sorted_alts[i] - sorted_alts[i - 1] > gap_deg:
            cid += 1
        cluster_ids_sorted[i] = cid
    out = np.zeros(len(alts), dtype=int)
    out[order] = cluster_ids_sorted
    return out


def _causal_ema_sse(residual, alpha):
    """
    Sum of squared errors of a causal (past-only) EMA one-step-ahead prediction, and of the
    zero-prediction baseline (i.e. trusting the physics term alone, no online adjustment) --
    matches the live-session methodology this offline pass is meant to validate at scale. EMA state
    resets to 0 at the start of every session; never carried across sessions.
    """
    ema = 0.0
    sse_model = 0.0
    sse_baseline = 0.0
    for r in residual:
        sse_model += (r - ema) ** 2
        sse_baseline += r ** 2
        ema = alpha * r + (1.0 - alpha) * ema
    return sse_model, sse_baseline, len(residual)


def loocv_dec_altitude_bins(session_series, gap_deg=8.0, alphas=None):
    """
    Cluster pooled free_drift sessions (across all kept nights) by altitude, then for each cluster
    estimate how much of the physics-residual DEC rate's variance a causal EMA filter can explain --
    validated leave-one-NIGHT-out so the reported number isn't just alpha overfit to this specific
    pooled sample (the same overfitting risk loocv_shape_net.py already guards against for the MLP,
    applied here to a 1-parameter model). For each held-out night, alpha is chosen from the OTHER
    nights' sessions in the same cluster (max R^2), then scored on the held-out night's own sessions;
    the reported R^2 is the pooled (SSE-summed, not naively averaged) result across all folds.
    """
    if alphas is None:
        alphas = [0.02, 0.05, 0.08, 0.1, 0.15, 0.2, 0.3, 0.4, 0.5]

    alts = [s[0] for s in session_series]
    cluster_ids = cluster_altitudes(alts, gap_deg)

    results = {}
    for cid in sorted(set(cluster_ids)):
        members = [s for s, c in zip(session_series, cluster_ids) if c == cid]
        nights = sorted(set(m[1] for m in members))
        member_alts = [m[0] for m in members]

        total_sse_model = 0.0
        total_sse_baseline = 0.0
        total_n = 0
        fold_r2 = []
        best_alphas = []

        if len(nights) < 2:
            # Can't hold a night out of a 1-night cluster; fall back to in-sample best alpha,
            # clearly flagged as such in the output so it isn't mistaken for a validated number.
            best = None
            for alpha in alphas:
                sm, sb, n = 0.0, 0.0, 0
                for _, _, residual in members:
                    m, b, k = _causal_ema_sse(residual, alpha)
                    sm += m; sb += b; n += k
                r2 = 1.0 - sm / sb if sb > 0 else float("nan")
                if best is None or r2 > best[1]:
                    best = (alpha, r2, sm, sb, n)
            results[cid] = {
                "alt_min": min(member_alts), "alt_max": max(member_alts),
                "alt_mean": float(np.mean(member_alts)),
                "n_sessions": len(members), "n_nights": len(nights), "n_frames": best[4],
                "best_alpha": best[0], "r2": best[1], "validated": False,
            }
            continue

        for held_out_night in nights:
            train = [m for m in members if m[1] != held_out_night]
            test = [m for m in members if m[1] == held_out_night]

            best_alpha, best_r2 = None, -np.inf
            for alpha in alphas:
                sm, sb = 0.0, 0.0
                for _, _, residual in train:
                    m, b, _ = _causal_ema_sse(residual, alpha)
                    sm += m; sb += b
                r2 = 1.0 - sm / sb if sb > 0 else -np.inf
                if r2 > best_r2:
                    best_alpha, best_r2 = alpha, r2
            best_alphas.append(best_alpha)

            sm_test, sb_test, n_test = 0.0, 0.0, 0
            for _, _, residual in test:
                m, b, k = _causal_ema_sse(residual, best_alpha)
                sm_test += m; sb_test += b; n_test += k
            total_sse_model += sm_test
            total_sse_baseline += sb_test
            total_n += n_test
            fold_r2.append(1.0 - sm_test / sb_test if sb_test > 0 else float("nan"))

        pooled_r2 = 1.0 - total_sse_model / total_sse_baseline if total_sse_baseline > 0 else float("nan")
        results[cid] = {
            "alt_min": min(member_alts), "alt_max": max(member_alts),
            "alt_mean": float(np.mean(member_alts)),
            "n_sessions": len(members), "n_nights": len(nights), "n_frames": total_n,
            "best_alpha": float(np.median(best_alphas)), "r2": float(pooled_r2),
            "per_fold_r2": [float(x) for x in fold_r2], "validated": True,
        }
    return results


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--sysid-dir", type=Path, required=True)
    p.add_argument("--axis", choices=["ra", "dec"], default="ra",
                   help="ra: phase-align and pool by worm phase (section 8/14.1 go/no-go). "
                        "dec: DEC's dominant behavior isn't phase-periodic, so instead pool each "
                        "night's physics-residual DEC rate and bin by altitude (added 2026-08-29, "
                        "see this file's module docstring).")
    p.add_argument("--n-bins", type=int, default=40, help="RA only: number of phase bins.")
    p.add_argument("--alt-cluster-gap-deg", type=float, default=8.0,
                   help="dec only: altitude gap (deg) between sorted sessions that starts a new "
                        "cluster. See cluster_altitudes() docstring.")
    p.add_argument("--alpha-grid", type=str, default="0.02,0.05,0.08,0.1,0.15,0.2,0.3,0.4,0.5",
                   help="dec only: comma-separated causal-EMA alpha values to search over per fold.")
    p.add_argument("--fixed-physics-from", type=Path, default=None,
                   help="dec only: load d_polar/k_ref_dec from this ai_guider_weights.json-format "
                        "file and use it, unchanged, as the physics reference for EVERY night, "
                        "instead of refitting per-night. Strongly recommended -- see "
                        "build_dec_session_series()'s docstring for why per-night refit gives a "
                        "misleadingly flat/near-zero answer.")
    p.add_argument("--output", type=Path, default=None,
                   help="If set, write the pooled samples (ra) or altitude-bin results (dec) as JSON.")
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

    if args.axis == "ra":
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

    elif args.axis == "dec":
        alphas = [float(x) for x in args.alpha_grid.split(",")]

        if args.fixed_physics_from:
            fixed_physics = json.loads(args.fixed_physics_from.read_text())["physics"]
            fixed_d_polar = fixed_physics["d_polar"]
            fixed_k_ref_dec = fixed_physics["k_ref_dec"]
            print(f"\n=== Using FIXED physics reference from {args.fixed_physics_from.name} for every "
                  f"night: d_polar={fixed_d_polar:.4e}, k_ref_dec={fixed_k_ref_dec:.4e} ===")
        else:
            print(f"\n=== WARNING: no --fixed-physics-from given -- falling back to each night's own "
                  f"freshly-refit d_polar/k_ref_dec, which tends to leave near-zero residual by "
                  f"construction and understates the deployed filter's real opportunity (see "
                  f"build_dec_session_series()'s docstring) ===")

        print(f"\n=== Building per-session physics-residual DEC rate series ===")
        session_series = []
        for path, sysid, ref in night_refs:
            d_polar = fixed_d_polar if args.fixed_physics_from else ref["d_polar"]
            k_ref_dec = fixed_k_ref_dec if args.fixed_physics_from else ref["k_ref_dec"]
            series = build_dec_session_series(path, sysid, d_polar, k_ref_dec)
            session_series.extend(series)
            print(f"  {Path(path).name}: {len(series)} free_drift sessions, "
                  f"altitudes {sorted(round(s[0], 1) for s in series)}")
        print(f"  TOTAL: {len(session_series)} sessions across {len(night_refs)} nights")

        print(f"\n=== Clustering by altitude (gap > {args.alt_cluster_gap_deg} deg starts a new cluster) ===")
        results = loocv_dec_altitude_bins(session_series, args.alt_cluster_gap_deg, alphas)
        print(f"  Found {len(results)} altitude cluster(s)")

        print(f"\n=== dec_residual_trust(altitude): leave-one-night-out causal-EMA R^2 per cluster ===")
        for cid in sorted(results):
            r = results[cid]
            tag = "" if r["validated"] else "  [NOT LOOCV-validated -- only 1 night in this cluster]"
            print(f"  alt [{r['alt_min']:.1f}, {r['alt_max']:.1f}] (mean {r['alt_mean']:.1f} deg): "
                  f"R^2={r['r2']:.3f}  best_alpha={r['best_alpha']:.3g}  "
                  f"n_sessions={r['n_sessions']}  n_nights={r['n_nights']}  n_frames={r['n_frames']}{tag}")
            if r["validated"] and "per_fold_r2" in r:
                print(f"      per-night-held-out R^2: "
                      + ", ".join(f"{x:.3f}" for x in r["per_fold_r2"]))

        r2_values = [r["r2"] for r in results.values() if r["validated"] and not np.isnan(r["r2"])]
        if len(r2_values) >= 2 and (max(r2_values) - min(r2_values)) > 0.05:
            print(f"\n  RESULT: R^2 varies meaningfully across altitude clusters "
                  f"({min(r2_values):.3f} to {max(r2_values):.3f}) -- consistent with the live-tested "
                  f"altitude-dependence hypothesis. An altitude-dependent multiplier on m_kfRDec, keyed "
                  f"off this table, is supported by pooled multi-night data, not just one night's 3-4 "
                  f"points.")
        elif r2_values:
            print(f"\n  RESULT: R^2 is roughly flat across altitude clusters "
                  f"({min(r2_values):.3f} to {max(r2_values):.3f}) in this pooled free_drift data -- "
                  f"the live single-night altitude effect did not clearly replicate here. Worth "
                  f"checking whether that's because free_drift (uncontaminated, but not closed-loop)"
                  f" is a genuinely different regime than the closed-loop guiding data the live test"
                  f" used, before concluding the hypothesis is wrong.")

        if args.output:
            out = {
                "alt_cluster_gap_deg": args.alt_cluster_gap_deg,
                "alpha_grid": alphas,
                "fixed_physics_from": str(args.fixed_physics_from) if args.fixed_physics_from else None,
                "nights": [Path(p).name for p, _, _ in night_refs],
                "night_references": {Path(p).name: r for p, _, r in night_refs},
                "dec_residual_trust": [
                    {"cluster": int(cid), **r} for cid, r in sorted(results.items())
                ],
            }
            args.output.write_text(json.dumps(out, indent=2))
            print(f"\n  Altitude-binned DEC trust table written to {args.output}")


if __name__ == "__main__":
    main()
