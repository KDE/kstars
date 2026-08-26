#!/usr/bin/env python3
"""
export_shape_net.py — produce a deployable weights.json with a v2 Shape Net section.

Retrains the LOOCV-validated architecture (3->16->16->1, see loocv_shape_net.py's result:
+0.0658 R^2 over a fixed 4-harmonic Fourier fit, 95% CI [0.0323,0.1095], won in 7/7 held-out
nights) on ALL currently available pooled, phase-aligned data -- no held-out split this time,
since architecture validation already happened separately via cross-validation. Standard
practice: validate the choice via CV, then retrain on everything for the actual deployed model.

Writes the result into a COPY of an existing (already-valid) weights.json, adding a new
"shape_net" section that WormGearGuider::loadWeights() reads if present, leaving every existing
field untouched. Does NOT overwrite the live weights file directly -- writes to an explicit
output path so the result can be reviewed/tested (e.g. via Shadow mode) before being deployed.

SPDX-License-Identifier: GPL-2.0-or-later
"""

import argparse
import json
import sys
from pathlib import Path

import numpy as np
import torch

sys.path.insert(0, str(Path(__file__).parent))
from pool_sysid_data import load_sysid_files, filter_same_mount, fit_night_reference, build_pooled_samples
from train_shape_net import to_tensors, make_net, train_net


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--sysid-dir", type=Path, required=True)
    p.add_argument("--base-weights", type=Path, required=True,
                   help="Existing weights.json to copy physics/mlp/etc. sections from unchanged.")
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--epochs", type=int, default=400)
    args = p.parse_args()

    print("=== Rebuilding pooled dataset ===")
    loaded = load_sysid_files(args.sysid_dir)
    kept = filter_same_mount(loaded, verbose=False)
    all_samples = []
    for path, sysid in kept:
        ref = fit_night_reference(sysid)
        all_samples.extend(build_pooled_samples(path, sysid, ref))
    print(f"  {len(all_samples)} samples across {len(kept)} nights")

    # Train on everything -- a tiny internal val split only to pick the best-val-loss checkpoint
    # (early stopping), not for reporting a final metric. The real validation already happened
    # in loocv_shape_net.py.
    rng = np.random.default_rng(0)
    idx = np.arange(len(all_samples))
    rng.shuffle(idx)
    n_val = max(1, int(0.1 * len(idx)))
    val_idx, train_idx = set(idx[:n_val].tolist()), None
    train_samples = [s for i, s in enumerate(all_samples) if i not in val_idx]
    val_samples = [s for i, s in enumerate(all_samples) if i in val_idx]

    X_train, Y_train = to_tensors(train_samples)
    X_val, Y_val = to_tensors(val_samples)

    print(f"\n=== Training 3->16->16->1 on {len(train_samples)} samples (LOOCV-validated architecture) ===")
    net = make_net(3, [16, 16])
    net, best_val_loss = train_net(net, X_train, Y_train, X_val, Y_val, epochs=args.epochs, weight_decay=1e-4)
    print(f"  best internal val loss: {best_val_loss:.5f}")

    state = net.state_dict()
    shape_net = {
        "w1":    state['0.weight'].cpu().numpy().flatten().tolist(),
        "b1":    state['0.bias'].cpu().numpy().flatten().tolist(),
        "w2":    state['2.weight'].cpu().numpy().flatten().tolist(),
        "b2":    state['2.bias'].cpu().numpy().flatten().tolist(),
        "w_out": state['4.weight'].cpu().numpy().flatten().tolist(),
        "b_out": state['4.bias'].cpu().numpy().flatten().tolist(),
    }

    base = json.loads(args.base_weights.read_text())
    ref_amplitude = base.get("physics", {}).get("pe_harmonic_amplitudes", [1.0])[0]
    shape_net["ref_amplitude_px"] = ref_amplitude
    print(f"  ref_amplitude_px = {ref_amplitude:.4f} (from base weights' physics.pe_harmonic_amplitudes[0])")

    base["shape_net"] = shape_net
    args.output.write_text(json.dumps(base, indent=2))
    print(f"\n  Wrote {args.output} -- base weights unchanged except for the new shape_net section.")
    print(f"  Load this file (e.g. via Shadow mode first) to test the v2 Shape Net path in the live runtime.")


if __name__ == "__main__":
    main()
