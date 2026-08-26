#!/usr/bin/env python3
"""
loocv_shape_net.py — leave-one-night-out cross-validation for the Stage 1 Shape Net question:
is the Shape Net's held-out advantage over a fixed 4-harmonic Fourier fit real, or noise from a
small single held-out slice?

Unlike train_shape_net.py's single 80/20-within-night split (723 held-out samples), this trains on
6 nights and tests on the 7th held out ENTIRELY, rotating which night is held out, so every one of
the pooled dataset's ~3,600 samples gets used as a genuine out-of-fold test point exactly once --
without needing to collect any new nights.

Uses a NIGHT-level (cluster) bootstrap for the significance test, not a per-frame one: frames
within a held-out night share that night's fold membership and aren't independent test replicates
of each other, so resampling individual frames would overstate how much independent evidence
exists. Resampling nights (with replacement, 7 at a time) is the statistically correct unit here.

SPDX-License-Identifier: GPL-2.0-or-later
"""

import sys
from pathlib import Path

import numpy as np
import torch

sys.path.insert(0, str(Path(__file__).parent))
from pool_sysid_data import load_sysid_files, filter_same_mount, fit_night_reference, build_pooled_samples
from train_shape_net import to_tensors, make_net, train_net, fourier_baseline_r2


def r2np(pred, target):
    mean = target.mean()
    mse_model = np.mean((pred - target) ** 2)
    mse_const = np.mean((mean - target) ** 2)
    return 1 - mse_model / mse_const if mse_const > 0 else np.nan


def main():
    sysid_dir = Path("/home/stellarmate/.local/share/kstars/ai_training_logs")
    print("=== Rebuilding pooled dataset ===")
    loaded = load_sysid_files(sysid_dir)
    kept = filter_same_mount(loaded, verbose=False)
    all_samples = []
    for path, sysid in kept:
        ref = fit_night_reference(sysid)
        all_samples.extend(build_pooled_samples(path, sysid, ref))

    by_night = {}
    for s in all_samples:
        by_night.setdefault(s[3], []).append(s)
    nights = sorted(by_night.keys())
    print(f"  {len(all_samples)} samples across {len(nights)} nights: {nights}")

    print(f"\n=== Leave-one-night-out cross-validation ({len(nights)} folds, Shape Net = 3->16->16->1) ===")
    oof_shape_pred = {}   # night -> array of predictions
    oof_fourier_pred = {}
    oof_target = {}

    for held_out in nights:
        train_samples = [s for n, ss in by_night.items() if n != held_out for s in ss]
        val_samples = by_night[held_out]

        X_train, Y_train = to_tensors(train_samples)
        X_val, Y_val = to_tensors(val_samples)

        net = make_net(3, [16, 16])
        net, _ = train_net(net, X_train, Y_train, X_val, Y_val, epochs=400, weight_decay=1e-4, seed=0)
        net.eval()
        with torch.no_grad():
            shape_pred = net(X_val).numpy().flatten()

        _, fourier_pred = fourier_baseline_r2(X_train, Y_train, X_val, Y_val, 4)

        target = Y_val.numpy().flatten()
        oof_shape_pred[held_out] = shape_pred
        oof_fourier_pred[held_out] = fourier_pred
        oof_target[held_out] = target

        r2_shape = r2np(shape_pred, target)
        r2_fourier = r2np(fourier_pred, target)
        print(f"  held out {held_out}: n={len(target):4d}  Shape Net R2={r2_shape:7.4f}  Fourier R2={r2_fourier:7.4f}  diff={r2_shape-r2_fourier:+.4f}")

    # Aggregate: every sample now has an out-of-fold prediction from a model that never saw its
    # night during training.
    all_shape = np.concatenate([oof_shape_pred[n] for n in nights])
    all_fourier = np.concatenate([oof_fourier_pred[n] for n in nights])
    all_target = np.concatenate([oof_target[n] for n in nights])

    print(f"\n=== Aggregate out-of-fold performance (all {len(all_target)} samples, each tested by a model that never saw its own night) ===")
    agg_r2_shape = r2np(all_shape, all_target)
    agg_r2_fourier = r2np(all_fourier, all_target)
    print(f"  Shape Net:  R2 = {agg_r2_shape:.4f}")
    print(f"  Fourier:    R2 = {agg_r2_fourier:.4f}")
    print(f"  Difference: {agg_r2_shape - agg_r2_fourier:+.4f}")

    print(f"\n=== Night-level (cluster) bootstrap on the paired difference, 5000 resamples ===")
    rng = np.random.default_rng(2)
    diffs = []
    shape_r2s = []
    fourier_r2s = []
    n_nights = len(nights)
    for _ in range(5000):
        resample_nights = rng.choice(nights, size=n_nights, replace=True)
        boot_shape = np.concatenate([oof_shape_pred[n] for n in resample_nights])
        boot_fourier = np.concatenate([oof_fourier_pred[n] for n in resample_nights])
        boot_target = np.concatenate([oof_target[n] for n in resample_nights])
        r2s = r2np(boot_shape, boot_target)
        r2f = r2np(boot_fourier, boot_target)
        shape_r2s.append(r2s)
        fourier_r2s.append(r2f)
        diffs.append(r2s - r2f)

    diffs = np.array(diffs)
    shape_r2s = np.array(shape_r2s)
    fourier_r2s = np.array(fourier_r2s)

    lo_s, hi_s = np.percentile(shape_r2s, [2.5, 97.5])
    lo_f, hi_f = np.percentile(fourier_r2s, [2.5, 97.5])
    lo_d, hi_d = np.percentile(diffs, [2.5, 97.5])

    print(f"  Shape Net R2:  point={agg_r2_shape:.4f}  95% CI=[{lo_s:.4f}, {hi_s:.4f}]")
    print(f"  Fourier R2:    point={agg_r2_fourier:.4f}  95% CI=[{lo_f:.4f}, {hi_f:.4f}]")
    print(f"  Difference:    point={agg_r2_shape-agg_r2_fourier:+.4f}  95% CI=[{lo_d:.4f}, {hi_d:.4f}]")
    print(f"  Fraction of night-resamples where Shape Net beat Fourier: {(diffs > 0).mean()*100:.1f}%")

    if lo_d > 0:
        print(f"\n  RESULT: the Shape Net's advantage is statistically real at the night-cluster level "
              f"(CI excludes zero) -- the v2 design's flexibility bet is supported by the full pooled "
              f"dataset, not just a lucky single split.")
    else:
        print(f"\n  RESULT: still not statistically distinguishable from zero at the night-cluster level, "
              f"even using every night as an independent out-of-fold test. With only {n_nights} nights, "
              f"this is the practical ceiling of what existing data can resolve -- genuinely need more "
              f"nights (not just a different split) to settle this, OR treat the effect as real-but-small "
              f"and decide whether it's worth the added complexity regardless of significance.")


if __name__ == "__main__":
    main()
