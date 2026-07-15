#!/usr/bin/env python3
"""
evaluate_shadow.py — Offline counterfactual evaluator for AI Guider shadow sessions.

Reads the AI debug CSV produced by KStars when shadow mode is enabled, and computes
an APPROXIMATE comparison between standard-only guiding and what AI-blended guiding
would have produced.


This script's results are NOT 100% accurate:

Shadow mode runs the AI silently alongside the standard controller. The standard
controller sends its own pulses, which shape the star's trajectory. The AI never
actually influences the mount. This means:

What this script CAN reliably tell you:
  Does the AI understand the mount's periodic error pattern?
  Are the AI's predictions correlated with the standard controller's residual error?
  Is the AI predicting in the right direction (sign check)?
  Rough order-of-magnitude improvement estimate (±30% accuracy)

What this script CANNOT tell you:
  Exact RMS you would achieve with AI active
  Whether the AI would cause oscillations or instability in closed loop


Usage:
    python3 evaluate_shadow.py --log path/to/ai_guider_*.csv [--gain 1.0]
    python3 evaluate_shadow.py --logdir ~/.local/share/kstars/ai_debug_logs/
"""

import argparse
import glob
import os
import sys

import numpy as np
import pandas as pd


def load_csv(path: str) -> pd.DataFrame:
    """Load and validate an AI debug CSV."""
    df = pd.read_csv(path)

    required = [
        "t_session", "dt", "ra_error_arcsec", "dec_error_arcsec",
        "conf", "pred_ra_arcsec", "pred_dec_arcsec",
        "physics_ra_arcsec", "mlp_ra_arcsec",
        "physics_dec_arcsec", "mlp_dec_arcsec",
    ]
    missing = [c for c in required if c not in df.columns]
    if missing:
        raise ValueError(f"CSV missing required columns: {missing}")

    return df


def evaluate_session(df: pd.DataFrame, ai_gain: float = 1.0,
                     warmup_frames: int = 30,
                     label: str = "") -> dict:
    """
    Evaluate a single shadow session.
    """

    # Filter to post-warmup frames only
    active = df.iloc[warmup_frames:].copy()
    if len(active) < 10:
        return {"label": label, "frames": len(active), "error": "Too few frames after warmup"}

    # Check if this is a shadow session
    has_ai_state = "ai_state" in active.columns
    if has_ai_state:
        shadow_mask = active["ai_state"] == "SHADOW"
        active_mask = active["ai_state"] == "ACTIVE"
        shadow_frac = shadow_mask.sum() / len(active) if len(active) > 0 else 0
    else:
        shadow_frac = 0.0

    # ── Standard-only RMS (what actually happened) ──
    ra_rms_std = np.sqrt(np.mean(active["ra_error_arcsec"] ** 2))
    dec_rms_std = np.sqrt(np.mean(active["dec_error_arcsec"] ** 2))
    total_rms_std = np.sqrt(ra_rms_std**2 + dec_rms_std**2)

    # ── Counterfactual: single-frame approximation ──
    # If the AI had been active, the error for frame i would be APPROXIMATELY:
    #   residual_i ≈ error_i - (ai_gain * conf_i * pred_i)
    #
    # This is a first-order approximation. In reality the pulse would change the
    # mount position, which would change error_i+1, etc. (trajectory divergence).
    conf = active["conf"].values
    pred_ra = active["pred_ra_arcsec"].values
    pred_dec = active["pred_dec_arcsec"].values
    error_ra = active["ra_error_arcsec"].values
    error_dec = active["dec_error_arcsec"].values

    # The AI correction that WOULD have been applied (arcsec, feed-forward)
    ai_correction_ra = ai_gain * conf * pred_ra
    ai_correction_dec = ai_gain * conf * pred_dec

    # Approximate residual if AI was active
    # The prediction aims to cancel drift, so the residual is error minus correction
    residual_ra = error_ra - ai_correction_ra
    residual_dec = error_dec - ai_correction_dec

    ra_rms_ai = np.sqrt(np.mean(residual_ra ** 2))
    dec_rms_ai = np.sqrt(np.mean(residual_dec ** 2))
    total_rms_ai = np.sqrt(ra_rms_ai**2 + dec_rms_ai**2)

    # ── Correlation analysis ──
    # If the AI truly understands the mount, its predictions should be positively
    # correlated with the error (both positive when star drifts positive).
    ra_corr = np.corrcoef(error_ra, pred_ra)[0, 1] if np.std(pred_ra) > 1e-10 else 0.0
    dec_corr = np.corrcoef(error_dec, pred_dec)[0, 1] if np.std(pred_dec) > 1e-10 else 0.0

    # ── Sign agreement ──
    # Fraction of frames where AI prediction has the same sign as the error
    ra_sign_agree = np.mean(np.sign(error_ra) == np.sign(pred_ra)) if len(error_ra) > 0 else 0
    dec_sign_agree = np.mean(np.sign(error_dec) == np.sign(pred_dec)) if len(error_dec) > 0 else 0

    # ── Mean confidence ──
    mean_conf = np.mean(conf)

    # ── Improvement estimate ──
    ra_improvement = (1.0 - ra_rms_ai / ra_rms_std) * 100 if ra_rms_std > 1e-10 else 0.0
    dec_improvement = (1.0 - dec_rms_ai / dec_rms_std) * 100 if dec_rms_std > 1e-10 else 0.0
    total_improvement = (1.0 - total_rms_ai / total_rms_std) * 100 if total_rms_std > 1e-10 else 0.0

    return {
        "label": label,
        "frames": len(active),
        "shadow_fraction": shadow_frac,
        "mean_confidence": mean_conf,
        # Standard-only (actual)
        "ra_rms_standard": ra_rms_std,
        "dec_rms_standard": dec_rms_std,
        "total_rms_standard": total_rms_std,
        # Counterfactual AI (approximate)
        "ra_rms_ai_approx": ra_rms_ai,
        "dec_rms_ai_approx": dec_rms_ai,
        "total_rms_ai_approx": total_rms_ai,
        # Improvement estimates
        "ra_improvement_pct": ra_improvement,
        "dec_improvement_pct": dec_improvement,
        "total_improvement_pct": total_improvement,
        # Quality indicators
        "ra_correlation": ra_corr,
        "dec_correlation": dec_corr,
        "ra_sign_agreement": ra_sign_agree,
        "dec_sign_agreement": dec_sign_agree,
    }


def print_report(results: list[dict]):
    """Print a human-readable evaluation report."""

    print("=" * 78)
    print("  AI GUIDER — SHADOW MODE EVALUATION REPORT")
    print("=" * 78)
    print()
    print("  ⚠  ACCURACY WARNING: These results are APPROXIMATE due to")
    print("     closed-loop distribution shift. See script header for details.")
    print("     True accuracy requires closed-loop simulation with free-drift data.")
    print()

    for r in results:
        if "error" in r:
            print(f"  [{r['label']}] ERROR: {r['error']}")
            continue

        print(f"  Session: {r['label']}")
        print(f"  Frames (post-warmup): {r['frames']}")
        if r["shadow_fraction"] > 0:
            print(f"  Shadow fraction: {r['shadow_fraction']:.0%}")
        print(f"  Mean confidence: {r['mean_confidence']:.2f}")
        print()

        print(f"  {'Metric':<30} {'Standard':>12} {'AI (approx)':>12} {'Δ':>10}")
        print(f"  {'-'*30} {'-'*12} {'-'*12} {'-'*10}")

        print(f"  {'RA RMS (arcsec)':<30} {r['ra_rms_standard']:>12.3f} {r['ra_rms_ai_approx']:>12.3f} {r['ra_improvement_pct']:>+9.1f}%")
        print(f"  {'DEC RMS (arcsec)':<30} {r['dec_rms_standard']:>12.3f} {r['dec_rms_ai_approx']:>12.3f} {r['dec_improvement_pct']:>+9.1f}%")
        print(f"  {'Total RMS (arcsec)':<30} {r['total_rms_standard']:>12.3f} {r['total_rms_ai_approx']:>12.3f} {r['total_improvement_pct']:>+9.1f}%")
        print()

        # Quality indicators
        print(f"  Quality Indicators:")
        ra_qual = "[GOOD]" if r["ra_correlation"] > 0.3 else ("[WEAK]" if r["ra_correlation"] > 0 else "[BAD]")
        dec_qual = "[GOOD]" if r["dec_correlation"] > 0.3 else ("[WEAK]" if r["dec_correlation"] > 0 else "[BAD]")
        print(f"    RA pred-error correlation:   {r['ra_correlation']:+.3f}  {ra_qual}")
        print(f"    DEC pred-error correlation:  {r['dec_correlation']:+.3f}  {dec_qual}")
        print(f"    RA sign agreement:           {r['ra_sign_agreement']:.0%}")
        print(f"    DEC sign agreement:          {r['dec_sign_agreement']:.0%}")
        print()

        # Verdict
        if r["ra_correlation"] > 0.3 and r["ra_improvement_pct"] > 5:
            print("  VERDICT: AI model looks promising for RA. Consider enabling AI guiding.")
        elif r["ra_correlation"] > 0 and r["ra_improvement_pct"] > 0:
            print("  VERDICT: AI model shows modest improvement. More data may help.")
        elif r["ra_correlation"] < 0:
            print("  VERDICT: AI predictions are ANTI-correlated with error. Do NOT enable.")
            print("           The model may need retraining with fresh sysid data.")
        else:
            print("  VERDICT: AI predictions are uncorrelated. Model needs retraining.")
        print()
        print("-" * 78)
        print()


def main():
    parser = argparse.ArgumentParser(
        description="Evaluate AI Guider shadow mode sessions (counterfactual analysis).",
        epilog="NOTE: Results are approximate. See --help-accuracy for details."
    )
    parser.add_argument("--log", type=str, nargs="*",
                        help="Path(s) to AI debug CSV files (supports glob patterns)")
    parser.add_argument("--logdir", type=str, default=None,
                        help="Directory containing AI debug CSVs (evaluates all)")
    parser.add_argument("--gain", type=float, default=1.0,
                        help="AI prediction gain for counterfactual (default: 1.0)")
    parser.add_argument("--warmup", type=int, default=30,
                        help="Number of warmup frames to skip (default: 30)")
    parser.add_argument("--latest", type=int, default=0,
                        help="Only evaluate the N most recent CSVs (0=all)")

    args = parser.parse_args()

    # Collect CSV files
    csv_files = []
    if args.log:
        for pattern in args.log:
            csv_files.extend(glob.glob(os.path.expanduser(pattern)))
    if args.logdir:
        logdir = os.path.expanduser(args.logdir)
        csv_files.extend(glob.glob(os.path.join(logdir, "ai_guider_*.csv")))

    if not csv_files:
        # Default location
        default_dir = os.path.expanduser("~/.local/share/kstars/ai_debug_logs/")
        csv_files = glob.glob(os.path.join(default_dir, "ai_guider_*.csv"))
        if not csv_files:
            print("No AI debug CSV files found. Specify --log or --logdir.", file=sys.stderr)
            sys.exit(1)

    csv_files = sorted(set(csv_files), key=os.path.getmtime)
    if args.latest > 0:
        csv_files = csv_files[-args.latest:]

    print(f"Found {len(csv_files)} CSV file(s) to evaluate.")
    print()

    results = []
    for path in csv_files:
        try:
            df = load_csv(path)
            label = os.path.basename(path)
            r = evaluate_session(df, ai_gain=args.gain,
                                 warmup_frames=args.warmup, label=label)
            results.append(r)
        except Exception as e:
            results.append({"label": os.path.basename(path), "error": str(e)})

    print_report(results)

    # Summary across all sessions
    valid = [r for r in results if "error" not in r]
    if len(valid) > 1:
        print("=" * 78)
        print("  AGGREGATE SUMMARY")
        print("=" * 78)
        avg_ra_imp = np.mean([r["ra_improvement_pct"] for r in valid])
        avg_dec_imp = np.mean([r["dec_improvement_pct"] for r in valid])
        avg_ra_corr = np.mean([r["ra_correlation"] for r in valid])
        print(f"  Sessions evaluated: {len(valid)}")
        print(f"  Mean RA improvement (approx):   {avg_ra_imp:+.1f}%")
        print(f"  Mean DEC improvement (approx):  {avg_dec_imp:+.1f}%")
        print(f"  Mean RA correlation:            {avg_ra_corr:+.3f}")
        print()


if __name__ == "__main__":
    main()
