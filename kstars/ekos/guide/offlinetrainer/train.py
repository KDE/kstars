#!/usr/bin/env python3
"""
offline_trainer/train.py — Main entry point for the Ekos AI Guider offline trainer.

Dispatches to the appropriate per-mount-type trainer based on sysid data.
All models are small enough to train on CPU in under 10 minutes.

Usage:
    python train.py --sysid-data ./sysid_data.json --output ./weights.json
    python train.py --sysid-data ./sysid_data.json --mount-type WORM_GEAR --output ./weights_eq6r.json
    python train.py --sysid-data ./sysid_data.json --mount-type HARMONIC_DRIVE --gpu --output ./weights_am5.json

SPDX-License-Identifier: GPL-2.0-or-later
"""

import argparse
import json
import sys
from pathlib import Path


def parse_args():
    p = argparse.ArgumentParser(description="Ekos AI Guider offline trainer")
    p.add_argument("--sysid-data",  required=True, type=Path,
                   help="Path to sysid_data.json produced by Guide AI Assistant")
    p.add_argument("--output",      required=True, type=Path,
                   help="Output path for trained weights JSON")
    p.add_argument("--mount-type",  default=None,
                   choices=["WORM_GEAR", "HARMONIC_DRIVE", "DIRECT_DRIVE"],
                   help="Override mount type (auto-detected from sysid data if not specified)")
    p.add_argument("--gpu",         action="store_true",
                   help="Use GPU if available (optional — all models train fine on CPU)")
    p.add_argument("--epochs",      type=int, default=None,
                   help="Override default epoch count for neural models")
    p.add_argument("--simulate",    action="store_true",
                   help="Run closed-loop simulation after training completes")
    p.add_argument("--plot",        action="store_true",
                   help="Generate matplotlib plots during simulation (requires --simulate)")
    p.add_argument("--verbose",     action="store_true")
    return p.parse_args()


def load_sysid(path: Path) -> dict:
    with open(path) as f:
        data = json.load(f)
    # Validate format version
    assert data.get("format_version") == "1.0", \
        f"Unsupported sysid format version: {data.get('format_version')}. " \
        f"Please upgrade your Guide AI Assistant."
    return data


def detect_mount_type(sysid: dict) -> str:
    """Auto-detect mount type from sysid equipment block."""
    mount_type = sysid["equipment"].get("mount_type", "")
    valid = {"WORM_GEAR", "HARMONIC_DRIVE", "DIRECT_DRIVE"}
    if mount_type not in valid:
        sys.exit(
            f"[ERROR] Unknown or missing mount_type '{mount_type}' in sysid data.\n"
            "Please specify --mount-type WORM_GEAR | HARMONIC_DRIVE | DIRECT_DRIVE"
        )
    return mount_type


def main():
    args = parse_args()

    print(f"[Ekos AI Trainer] Loading sysid data from {args.sysid_data}")
    sysid = load_sysid(args.sysid_data)

    mount_type = args.mount_type or detect_mount_type(sysid)
    print(f"[Ekos AI Trainer] Mount type: {mount_type}")
    print(f"[Ekos AI Trainer] Mount name: {sysid['equipment'].get('mount_name', 'unknown')}")

    # Dispatch to the appropriate trainer
    if mount_type == "DIRECT_DRIVE":
        from train_direct_drive import train_direct_drive
        weights = train_direct_drive(sysid, verbose=args.verbose)

    elif mount_type == "WORM_GEAR":
        from train_worm_gear import train_worm_gear
        weights = train_worm_gear(sysid, gpu=args.gpu,
                                  epochs=args.epochs, verbose=args.verbose)

    elif mount_type == "HARMONIC_DRIVE":
        from train_harmonic import train_harmonic
        weights = train_harmonic(sysid, gpu=args.gpu,
                                 epochs=args.epochs, verbose=args.verbose)

    else:
        sys.exit(
            f"[ERROR] Unsupported mount type: {mount_type}. "
            "Select from: WORM_GEAR, HARMONIC_DRIVE, DIRECT_DRIVE"
        )

    # Write output
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with open(args.output, "w") as f:
        json.dump(weights, f, indent=2)

    print(f"[Ekos AI Trainer] ✓ Weights saved to {args.output}")
    print(f"[Ekos AI Trainer] Load this file in KStars Equipment Profile → AI Guiding → Weights File")

    if args.simulate:
        print(f"\n[Ekos AI Trainer] Launching simulation...")
        import subprocess
        sim_cmd = [
            sys.executable, str(Path(__file__).parent / "simulate_worm_gear.py"),
            "--weights", str(args.output),
            "--sysid-data", str(args.sysid_data),
            "--hold-out-last", "1",
        ]
        if args.verbose:
            sim_cmd.append("--verbose")
        if args.plot:
            sim_cmd.append("--plot")
        subprocess.run(sim_cmd)

if __name__ == "__main__":
    main()
