#!/usr/bin/env python3
"""
train_shape_net.py — Stage 1 of the AI Guider v2 architecture proposal
(/home/stellarmate/ai_guider_v2_architecture.md, section 5 and section 15).

Trains the offline neural Shape Net (phase[, altitude] -> predicted detrended RA drift) on the
Stage 0 pooled, phase-aligned multi-night dataset (pool_sysid_data.py). Does NOT assume the
suggested 3->16->16->1 size from section 5.3 is correct -- runs the same kind of size sweep +
bootstrap confidence interval used to debunk the "bigger MLP needed" assumption for the old
per-session residual MLP, and additionally tests the actual hypothesis the whole v2 design rests
on: does a flexible net beat a FIXED 4-harmonic Fourier fit on the same pooled, held-out data.

Critically uses a per-night-stratified train/val split, not the naive sequential split that left
DEC with only 103 real training samples in the v1 investigation (architecture doc section 14.2)
-- every night contributes its own 80/20 split before concatenation, so no single night's data can
end up disproportionately in train or val.

SPDX-License-Identifier: GPL-2.0-or-later
"""

import argparse
import sys
from pathlib import Path

import numpy as np
import torch
import torch.nn as nn

sys.path.insert(0, str(Path(__file__).parent))
from pool_sysid_data import load_sysid_files, filter_same_mount, fit_night_reference, build_pooled_samples


def night_stratified_split(samples, val_frac=0.2, seed=0):
    """
    Split EACH night's own samples independently by CONTIGUOUS time block, then concatenate --
    guarantees every night is represented in both train and val (unlike a sequential split over
    the whole concatenated array, which starved DEC to 103 real training samples in the v1
    investigation because contamination and night order weren't independent), WITHOUT shuffling
    individual frames within a night first.

    Frame-level shuffling before splitting was tried first and rejected: consecutive frames are
    ~2s apart and highly autocorrelated, so a shuffled split puts temporally-adjacent frames on
    both sides of the train/val boundary. Caught 2026-08-26 by noticing held-out R^2 climbed
    monotonically with network size all the way to 34,049 params with no plateau -- for a 3-input
    function with ~2,900 training points, that's the signature of a flexible model locally
    interpolating near-duplicate neighboring frames across the leaky boundary, not real
    generalization. A contiguous per-night block split (last val_frac of each night's own frame
    order held out) removes almost all of that adjacency leakage.
    """
    rng = np.random.default_rng(seed)
    by_night = {}
    for s in samples:
        by_night.setdefault(s[3], []).append(s)

    train, val = [], []
    for night, night_samples in by_night.items():
        n = len(night_samples)
        n_val = max(1, int(n * val_frac))
        # Held-out block position varies by night (seeded) so the val portion isn't always the
        # exact same phase range across every night, which could otherwise bias which part of the
        # cycle gets tested.
        start = int(rng.uniform(0, n - n_val))
        val_range = set(range(start, start + n_val))
        for i, s in enumerate(night_samples):
            (val if i in val_range else train).append(s)
    return train, val


def to_tensors(samples, use_altitude=True):
    phase = np.array([s[0] for s in samples])
    value = np.array([s[1] for s in samples])
    alt = np.array([s[2] for s in samples])
    if use_altitude:
        X = np.column_stack([np.sin(phase), np.cos(phase), alt / 90.0]).astype(np.float32)
    else:
        X = np.column_stack([np.sin(phase), np.cos(phase)]).astype(np.float32)
    Y = value.astype(np.float32).reshape(-1, 1)
    return torch.tensor(X), torch.tensor(Y)


def make_net(in_dim, hidden):
    layers = []
    prev = in_dim
    for h in hidden:
        layers += [nn.Linear(prev, h), nn.ReLU()]
        prev = h
    layers += [nn.Linear(prev, 1)]
    return nn.Sequential(*layers)


def train_net(net, X_train, Y_train, X_val, Y_val, epochs=400, weight_decay=1e-4, seed=0):
    torch.manual_seed(seed)
    opt = torch.optim.Adam(net.parameters(), lr=1e-3, weight_decay=weight_decay)
    sched = torch.optim.lr_scheduler.CosineAnnealingLR(opt, T_max=epochs)
    criterion = nn.MSELoss()
    best_val, best_state = float("inf"), None
    for epoch in range(epochs):
        net.train()
        opt.zero_grad()
        loss = criterion(net(X_train), Y_train)
        loss.backward()
        torch.nn.utils.clip_grad_norm_(net.parameters(), 1.0)
        opt.step()
        sched.step()
        net.eval()
        with torch.no_grad():
            vl = criterion(net(X_val), Y_val).item()
        if vl < best_val:
            best_val, best_state = vl, {k: v.clone() for k, v in net.state_dict().items()}
    net.load_state_dict(best_state)
    return net, best_val


def r2(pred, target):
    pred, target = pred.flatten(), target.flatten()
    mean = target.mean()
    mse_model = ((pred - target) ** 2).mean()
    mse_const = ((mean - target) ** 2).mean()
    return float(1 - mse_model / mse_const) if mse_const > 0 else float("nan")


def bootstrap_r2_ci(pred, target, n_boot=2000, seed=0):
    pred, target = pred.flatten().numpy(), target.flatten().numpy()
    n = len(pred)
    rng = np.random.default_rng(seed)
    boot = []
    for _ in range(n_boot):
        idx = rng.choice(n, size=n, replace=True)
        boot.append(r2(torch.tensor(pred[idx]), torch.tensor(target[idx])))
    lo, hi = np.percentile(boot, [2.5, 97.5])
    return float(lo), float(hi)


def fourier_baseline_r2(X_train, Y_train, X_val, Y_val, n_harmonics=4):
    """
    Fixed-harmonic linear baseline on the SAME pooled, held-out split -- this is what v1's
    physics-only model represents (a Fourier sum up to n_harmonics), evaluated fairly against the
    exact same data the neural Shape Net sees. If the net can't beat this, flexibility isn't
    buying anything on this dataset and the v2 design's core bet is wrong.

    MUST include the same altitude input the Shape Net gets (as a linear additive term) or this
    isn't a fair comparison -- caught 2026-08-26 by checking whether the Shape Net's own learned
    curve was well-explained by a 4-harmonic refit (it was, 97.6%), which meant its R^2 advantage
    over an altitude-blind Fourier baseline couldn't honestly be attributed to capturing
    non-harmonic/transient shape at all.
    """
    phase_train = np.arctan2(X_train[:, 1].numpy(), X_train[:, 0].numpy())
    phase_val = np.arctan2(X_val[:, 1].numpy(), X_val[:, 0].numpy())
    alt_train = X_train[:, 2].numpy()
    alt_val = X_val[:, 2].numpy()

    def build_cols(phase, alt):
        cols = [np.ones_like(phase), alt]
        for k in range(1, n_harmonics + 1):
            cols.append(np.sin(k * phase))
            cols.append(np.cos(k * phase))
        return np.column_stack(cols)

    A_train = build_cols(phase_train, alt_train)
    A_val = build_cols(phase_val, alt_val)
    y_train = Y_train.numpy().flatten()
    y_val = Y_val.numpy().flatten()

    coef, _, _, _ = np.linalg.lstsq(A_train, y_train, rcond=None)
    pred_val = A_val @ coef
    return r2(torch.tensor(pred_val), torch.tensor(y_val)), pred_val


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--sysid-dir", type=Path, required=True)
    p.add_argument("--n-harmonics-baseline", type=int, default=4)
    p.add_argument("--epochs", type=int, default=400)
    p.add_argument("--output", type=Path, default=None, help="Write best net's weights as JSON")
    args = p.parse_args()

    print("=== Rebuilding pooled dataset (Stage 0) ===")
    loaded = load_sysid_files(args.sysid_dir)
    kept = filter_same_mount(loaded, verbose=False)
    all_samples = []
    for path, sysid in kept:
        ref = fit_night_reference(sysid)
        all_samples.extend(build_pooled_samples(path, sysid, ref))
    print(f"  {len(all_samples)} total pooled samples across {len(kept)} nights")

    print("\n=== Night-stratified train/val split ===")
    train_samples, val_samples = night_stratified_split(all_samples)
    print(f"  train={len(train_samples)}  val={len(val_samples)}")
    train_nights = set(s[3] for s in train_samples)
    val_nights = set(s[3] for s in val_samples)
    print(f"  nights represented in train: {len(train_nights)}  in val: {len(val_nights)} "
          f"(should be equal -- every night split internally, not assigned wholesale)")

    X_train, Y_train = to_tensors(train_samples)
    X_val, Y_val = to_tensors(val_samples)

    print(f"\n=== Fixed {args.n_harmonics_baseline}-harmonic Fourier baseline (what v1's physics-only model represents) ===")
    fourier_r2, fourier_pred = fourier_baseline_r2(X_train, Y_train, X_val, Y_val, args.n_harmonics_baseline)
    print(f"  Fourier baseline held-out R^2: {fourier_r2:.4f}")

    print(f"\n=== Shape Net size sweep ===")
    candidates = {
        "3->8->1":          [8],
        "3->16->1":         [16],
        "3->16->16->1":     [16, 16],
        "3->24->16->1":     [24, 16],
        "3->32->16->1":     [32, 16],
    }
    results = {}
    print(f"{'Architecture':<20}{'n_params':>10}{'val_loss':>12}{'R2':>10}")
    for name, hidden in candidates.items():
        net = make_net(3, hidden)
        n_params = sum(p.numel() for p in net.parameters())
        net, val_loss = train_net(net, X_train, Y_train, X_val, Y_val, epochs=args.epochs)
        net.eval()
        with torch.no_grad():
            pred_val = net(X_val)
        r2_val = r2(pred_val, Y_val)
        results[name] = (net, hidden, n_params, val_loss, r2_val, pred_val)
        print(f"{name:<20}{n_params:>10}{val_loss:>12.5f}{r2_val:>10.4f}")

    best_name = max(results, key=lambda k: results[k][4])
    best_net, best_hidden, best_params, best_loss, best_r2, best_pred = results[best_name]
    print(f"\n  Best: {best_name} ({best_params} params), held-out R^2 = {best_r2:.4f}")

    print(f"\n=== Bootstrap 95% CI on best Shape Net's held-out R^2 (2000 resamples) ===")
    lo, hi = bootstrap_r2_ci(best_pred, Y_val)
    print(f"  {best_name}: point R^2={best_r2:.4f}  95% CI=[{lo:.4f}, {hi:.4f}]")

    print(f"\n=== Shape Net vs Fourier baseline, same held-out data ===")
    print(f"  Fourier ({args.n_harmonics_baseline}h, {2*args.n_harmonics_baseline+1} params): R^2={fourier_r2:.4f}")
    print(f"  Shape Net ({best_name}, {best_params} params):        R^2={best_r2:.4f}")
    if best_r2 > fourier_r2:
        print(f"  RESULT: Shape Net beats the fixed-harmonic baseline by {best_r2 - fourier_r2:.4f} R^2 "
              f"on the same held-out data -- the flexibility is earning something real, not just noise.")
    else:
        print(f"  RESULT: Shape Net does NOT beat the fixed-harmonic baseline on held-out data. "
              f"The v2 design's core bet (flexibility beats fixed harmonics given enough pooled data) "
              f"is NOT supported by this measurement -- worth understanding why before proceeding "
              f"(more data needed? transient too rare/small to resolve even pooled? architecture doc "
              f"section 16.4's caveat may apply here too).")

    if args.output:
        state = best_net.state_dict()
        out = {
            "architecture": best_name,
            "hidden_layers": best_hidden,
            "input": "sin(phase), cos(phase), altitude/90",
            "held_out_r2": best_r2,
            "held_out_r2_95ci": [lo, hi],
            "fourier_baseline_r2": fourier_r2,
            "weights": {k: v.numpy().tolist() for k, v in state.items()},
        }
        import json
        args.output.write_text(json.dumps(out, indent=2))
        print(f"\n  Best Shape Net weights written to {args.output}")


if __name__ == "__main__":
    main()
