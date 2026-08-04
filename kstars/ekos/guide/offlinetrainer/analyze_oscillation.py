#!/usr/bin/env python3
"""
analyze_oscillation.py — Oscillation diagnostics for ACTIVE-mode AI Guider sessions.

Unlike evaluate_shadow.py (which only handles SHADOW-mode counterfactual data), this
script reads the AI debug CSV from a real closed-loop session — AI-active, Shadow-mode
baseline, or plain standard guiding with Shadow Mode logging enabled — and helps answer:

  Is the AI+PID blend double-correcting (loop-gain / resonance), or is it a PE
  phase mismatch (the AI confidently pushing at the wrong point in the cycle)?

It relies on the blended-pulse breakdown columns (ra_algorithm, ra_prop_response_ms,
ra_integral_response_ms, ra_ai_response_ms, ra_active_prop_gain, ra_total_pulse_ms,
ra_direction, ra_suppressed, and the dec_* equivalents) that record what was ACTUALLY
sent to the mount each frame, not just the AI's internal prediction. Older CSVs (from
before this logging was added) lack these columns; the script degrades gracefully and
skips the blend-specific analyses for them.

Frame quality policy is adapted from EVALUATION.md Sec 4a in the ekos-ai-guider design
repo (dropped frames via dt outliers, restart-gap re-convergence, error spikes, saturated
pulses). Two columns that policy also uses — ErrorCode and SNR — are not present in this
CSV (they live in the separate PHD2-format guide log), so those two criteria are omitted
here; note this if you need bullet-proof frame exclusion.

The ringing diagnostic is adapted from acf_plots.py's autocorrelation approach (also in
that design repo) rather than an ad hoc sign-flip counter: a genuinely oscillating
closed loop shows strong NEGATIVE autocorrelation at lag 1 (and often lag 2) in the
actual pulse series — a cleaner statistical signature than counting sign flips.

Usage:
    python3 analyze_oscillation.py --log path/to/ai_guider_*.csv
    python3 analyze_oscillation.py --logdir ~/.local/share/kstars/ai_debug_logs/ --latest 1
    python3 analyze_oscillation.py --log active.csv --baseline standard_baseline.csv
    python3 analyze_oscillation.py --log active.csv --weights weights_harmonic.json
"""

import argparse
import glob
import json
import os
import sys

import numpy as np
import pandas as pd

BLEND_COLUMNS = [
    "ra_algorithm", "ra_prop_response_ms", "ra_integral_response_ms", "ra_ai_response_ms",
    "ra_active_prop_gain", "ra_total_pulse_ms", "ra_direction", "ra_suppressed",
    "dec_algorithm", "dec_prop_response_ms", "dec_integral_response_ms", "dec_ai_response_ms",
    "dec_active_prop_gain", "dec_total_pulse_ms", "dec_direction", "dec_suppressed",
]

REQUIRED_COLUMNS = [
    "t_session", "dt", "ra_error_arcsec", "dec_error_arcsec", "conf", "ai_state",
]


def load_csv(path: str) -> pd.DataFrame:
    """Load an AI debug CSV. Tags df.attrs['has_blend_columns'] for graceful degradation."""
    df = pd.read_csv(path)

    missing = [c for c in REQUIRED_COLUMNS if c not in df.columns]
    if missing:
        raise ValueError(f"CSV missing required columns: {missing}")

    has_blend = all(c in df.columns for c in BLEND_COLUMNS)
    df.attrs["has_blend_columns"] = has_blend
    if not has_blend:
        print(f"  [warn] {os.path.basename(path)}: old-format CSV (no blend breakdown columns) — "
              "skipping blend-specific analyses (algorithm/total-pulse breakdown, ACF on pulse series).",
              file=sys.stderr)
    return df


# ── Frame quality policy (adapted from EVALUATION.md Sec 4a) ─────────────────────────────
def classify_frames(df: pd.DataFrame) -> pd.Series:
    """EXCLUDED mask: True = exclude. Dropped frames (dt outlier) + post-restart re-convergence.

    Note: ErrorCode and SNR criteria from EVALUATION.md Sec 4a are not applicable here — this
    CSV does not carry those columns (they live in the separate PHD2-format guide log).
    """
    median_dt = df["dt"].median()
    excluded = df["dt"] > 3 * median_dt

    # Restart gap: dt > 30s marks a guide abort/restart; the first 10 frames after are
    # re-convergence, not steady-state behavior.
    gap_idx = df.index[df["dt"] > 30.0]
    restart_frames = set()
    for idx in gap_idx:
        pos = df.index.get_loc(idx)
        for i in range(pos, min(pos + 10, len(df))):
            restart_frames.add(df.index[i])
    excluded = excluded | df.index.isin(restart_frames)

    return excluded


def classify_flagged(df: pd.DataFrame, valid_mask: pd.Series) -> pd.Series:
    """Among valid frames: error spikes (excluded from ringing/periodogram analysis only —
    they're real events but break the linear/steady-state assumptions).

    Deliberately NOT flagging "saturated pulses" here the way EVALUATION.md Sec 4a does:
    that policy uses pulse_ms >= 0.95 * the CONFIGURED max-pulse ceiling, which isn't logged
    per-frame in this CSV. Using the session's own observed max pulse as a proxy instead
    would backfire on exactly the failure mode this script hunts for — a session genuinely
    oscillating at sustained high amplitude would have most of its frames sit near its own
    max and get flagged out, hiding the ringing signal. So only the error-spike criterion
    (scaled off the error series, not the pulse series) is applied here.
    """
    valid = df[valid_mask]
    if len(valid) == 0:
        return pd.Series(False, index=df.index)

    ra_rms = float(np.sqrt(np.mean(valid["ra_error_arcsec"] ** 2)))
    dec_rms = float(np.sqrt(np.mean(valid["dec_error_arcsec"] ** 2)))

    flagged = (df["ra_error_arcsec"].abs() > 5 * max(ra_rms, 1e-6)) | \
              (df["dec_error_arcsec"].abs() > 5 * max(dec_rms, 1e-6))

    return valid_mask & flagged


# ── Confidence segmentation ───────────────────────────────────────────────────────────────
def segment_by_confidence(df: pd.DataFrame, valid_mask: pd.Series, min_confidence: float = 0.5) -> dict:
    valid = df[valid_mask]
    if len(valid) == 0:
        return {"error": "No valid frames after quality filtering"}

    confident = valid[(valid["ai_state"].isin(["ACTIVE", "SHADOW"])) & (valid["conf"] >= min_confidence)]
    not_confident = valid[~valid.index.isin(confident.index)]

    def axis_rms(frame, col):
        return float(np.sqrt(np.mean(frame[col] ** 2))) if len(frame) else float("nan")

    return {
        "total_frames": len(df),
        "valid_frames": len(valid),
        "confident_frames": len(confident),
        "not_confident_frames": len(not_confident),
        "ra_rms_confident": axis_rms(confident, "ra_error_arcsec"),
        "dec_rms_confident": axis_rms(confident, "dec_error_arcsec"),
        "ra_rms_not_confident": axis_rms(not_confident, "ra_error_arcsec"),
        "dec_rms_not_confident": axis_rms(not_confident, "dec_error_arcsec"),
        "mean_confidence": float(confident["conf"].mean()) if len(confident) else float("nan"),
    }


# ── Periodogram (Lomb-Scargle, same tool train_harmonic.py uses for PE detection) ────────
def periodogram_report(df: pd.DataFrame, valid_mask: pd.Series, weights_path: str = None) -> dict:
    valid = df[valid_mask].sort_values("t_session")
    if len(valid) < 20:
        return {"error": "Too few valid frames for a periodogram"}

    try:
        import scipy.signal
        import scipy.stats
    except ImportError:
        return {"error": "scipy is required for periodogram_report (pip install scipy)"}

    t = valid["t_session"].values.astype(float)
    span = t[-1] - t[0]
    if span <= 0:
        return {"error": "Degenerate time span"}

    dt_med = float(np.median(np.diff(t))) if len(t) > 1 else 2.0
    nyquist = 0.5 / max(dt_med, 1e-3)
    f_min = max(2.0 / span, 0.002)
    f_max = min(0.5, nyquist * 0.9)
    if f_min >= f_max:
        return {"error": "Session too short to resolve any periodic structure"}

    f_search = np.geomspace(f_min, f_max, 4000)
    omega = 2 * np.pi * f_search

    result = {}
    for axis, col in (("ra", "ra_error_arcsec"), ("dec", "dec_error_arcsec")):
        y = valid[col].values.astype(float)
        slope, intercept, _, _, _ = scipy.stats.linregress(t, y)
        y_detrended = y - (slope * t + intercept)
        power = scipy.signal.lombscargle(t, y_detrended, omega, precenter=True)
        noise_floor = np.median(power) + 1e-12

        top_idx = np.argsort(power)[-3:][::-1]
        top_peaks = [(float(1.0 / f_search[i]), float(power[i] / noise_floor)) for i in top_idx]
        result[axis] = {"top_peaks_period_s_snr": top_peaks}

    if weights_path and os.path.exists(weights_path):
        try:
            with open(weights_path) as f:
                weights = json.load(f)
            if weights.get("mount_type") == "HARMONIC_DRIVE":
                phys = weights.get("physical", {})
                pe_period = phys.get("pe_period", 0.0)
                pe2_period = phys.get("pe2_period", 0.0)
                result["trained_pe_periods_s"] = [p for p in (pe_period, pe2_period) if p and p > 0]
        except (json.JSONDecodeError, OSError) as e:
            print(f"  [warn] could not read --weights {weights_path}: {e}", file=sys.stderr)

    return result


# ── ACF-based ringing diagnostic (adapted from acf_plots.py) ─────────────────────────────
def compute_autocorr(series: np.ndarray, max_lag: int) -> np.ndarray:
    n = len(series)
    s = series - series.mean()
    ac = np.correlate(s, s, mode="full")
    ac = ac[n - 1: n - 1 + max_lag]
    denom = ac[0] if ac[0] != 0 else 1e-12
    return ac / denom


def acf_ringing_report(df: pd.DataFrame, valid_mask: pd.Series, max_lag: int = 10) -> dict:
    """Strong NEGATIVE autocorrelation at lag 1 (sign flipping every frame) or lag 2 is the
    statistical signature of closed-loop ringing/double-correction. Applied to the residual
    (ra/dec_error_arcsec) and — when available — the actual total pulse sent, which is the
    more direct "is the mount being oscillated" signal.
    """
    valid = df[valid_mask]
    if len(valid) < max_lag + 5:
        return {"error": "Too few valid frames for autocorrelation"}

    n = len(valid)
    sig = 1.96 / np.sqrt(n)

    report = {"n_frames": n, "significance_bound": float(sig), "series": {}}

    series_map = {"ra_error": valid["ra_error_arcsec"].values, "dec_error": valid["dec_error_arcsec"].values}
    if df.attrs.get("has_blend_columns"):
        series_map["ra_total_pulse"] = valid["ra_total_pulse_ms"].values
        series_map["dec_total_pulse"] = valid["dec_total_pulse_ms"].values

    for name, series in series_map.items():
        ac = compute_autocorr(np.asarray(series, dtype=float), max_lag)
        lag1, lag2 = float(ac[1]), float(ac[2]) if max_lag > 2 else float("nan")
        ringing = bool(lag1 < -sig or (max_lag > 2 and lag2 < -sig))
        report["series"][name] = {
            "lag1": lag1,
            "lag2": lag2,
            "ringing_signature": ringing,
        }

    return report


# ── Session comparison ────────────────────────────────────────────────────────────────────
def compare_sessions(primary: dict, baseline: dict) -> dict:
    def improvement(base, cand):
        if base is None or cand is None or base != base or cand != cand or base < 1e-9:
            return float("nan")
        return (1.0 - cand / base) * 100.0

    return {
        "ra_improvement_pct": improvement(baseline.get("ra_rms_confident"), primary.get("ra_rms_confident")),
        "dec_improvement_pct": improvement(baseline.get("dec_rms_confident"), primary.get("dec_rms_confident")),
    }


# ── Reporting ──────────────────────────────────────────────────────────────────────────────
def print_report(label: str, seg: dict, periodogram: dict, acf: dict):
    print("=" * 78)
    print(f"  AI GUIDER — OSCILLATION DIAGNOSTIC: {label}")
    print("=" * 78)

    if "error" in seg:
        print(f"  {seg['error']}")
        return
    print(f"  Total frames        : {seg['total_frames']}")
    print(f"  Valid frames        : {seg['valid_frames']}")
    print(f"  Confident frames    : {seg['confident_frames']}  (mean conf {seg['mean_confidence']:.2f})")
    print(f"  Not-confident frames: {seg['not_confident_frames']}")
    print()
    print(f"  {'Axis':<6} {'RMS (confident)':>18} {'RMS (not confident)':>22}")
    print(f"  {'RA':<6} {seg['ra_rms_confident']:>17.3f}\" {seg['ra_rms_not_confident']:>21.3f}\"")
    print(f"  {'DEC':<6} {seg['dec_rms_confident']:>17.3f}\" {seg['dec_rms_not_confident']:>21.3f}\"")
    print()

    if "error" in periodogram:
        print(f"  Periodogram: {periodogram['error']}")
    else:
        trained = periodogram.get("trained_pe_periods_s")
        for axis in ("ra", "dec"):
            peaks = periodogram[axis]["top_peaks_period_s_snr"]
            desc = ", ".join(f"{p:.1f}s (SNR {s:.0f})" for p, s in peaks)
            print(f"  {axis.upper()} residual periodogram top peaks: {desc}")
        if trained:
            print(f"  Trained PE period(s): {', '.join(f'{p:.1f}s' for p in trained)}")
            print("  -> Power concentrated near a trained PE period suggests PHASE MISMATCH.")
            print("  -> Power concentrated elsewhere (short/near-Nyquist) suggests LOOP-GAIN /")
            print("     double-correction (AI + P fighting each other), not a PE tracking issue.")
    print()

    if "error" in acf:
        print(f"  ACF ringing check: {acf['error']}")
    else:
        print(f"  ACF ringing check (significance bound ±{acf['significance_bound']:.3f}):")
        for name, s in acf["series"].items():
            flag = "  <-- RINGING SIGNATURE" if s["ringing_signature"] else ""
            print(f"    {name:<16} lag1={s['lag1']:+.3f}  lag2={s['lag2']:+.3f}{flag}")
        any_ringing = any(s["ringing_signature"] for s in acf["series"].values())
        pulse_ringing = any(s["ringing_signature"] for n, s in acf["series"].items() if "pulse" in n)
        error_ringing = any(s["ringing_signature"] for n, s in acf["series"].items() if "error" in n)
        print()
        if pulse_ringing:
            print("  VERDICT: the actual pulse sent to the mount is ringing (strong negative lag-1/2")
            print("           autocorrelation) — consistent with AI+PID double-correction. Check")
            print("           Options::aIProportionalBackoff / aIPredictionGain, or try the AI-only")
            print("           reload with a lower aIPredictionGain via reloadAIWeights() and re-watch.")
        elif error_ringing:
            print("  VERDICT: the residual error is ringing but the pulse series isn't (or isn't")
            print("           logged) — check the PE-period periodogram above for a phase-mismatch signature.")
        elif any_ringing:
            print("  VERDICT: some ringing signature detected — see per-series breakdown above.")
        else:
            print("  VERDICT: no autocorrelation ringing signature detected in this session.")
    print()
    print("-" * 78)
    print()


def analyze_one(path: str, weights_path: str, min_confidence: float, max_lag: int):
    df = load_csv(path)
    excluded = classify_frames(df)
    valid_mask = ~excluded
    flagged = classify_flagged(df, valid_mask)
    analysis_mask = valid_mask & ~flagged

    seg = segment_by_confidence(df, valid_mask, min_confidence)
    periodogram = periodogram_report(df, analysis_mask, weights_path)
    acf = acf_ringing_report(df, analysis_mask, max_lag)
    return seg, periodogram, acf


def main():
    parser = argparse.ArgumentParser(
        description="Diagnose AI Guider oscillations from ACTIVE-mode (or Shadow-mode baseline) debug CSVs.")
    parser.add_argument("--log", type=str, nargs="*", help="Path(s) to AI debug CSV files (glob supported)")
    parser.add_argument("--logdir", type=str, default=None, help="Directory containing AI debug CSVs")
    parser.add_argument("--latest", type=int, default=0, help="Only analyze the N most recent CSVs (0=all)")
    parser.add_argument("--baseline", type=str, default=None,
                         help="A second CSV (e.g. Shadow-mode/standard-guiding session) for side-by-side RMS")
    parser.add_argument("--weights", type=str, default=None,
                         help="weights_*.json to annotate the periodogram with the trained PE period(s)")
    parser.add_argument("--min-confidence", type=float, default=0.5,
                         help="Confidence threshold splitting AI-active vs not-yet-confident frames (default 0.5)")
    parser.add_argument("--flip-window", type=int, default=5,
                         help="Max lag (frames) for the ACF ringing check (default 5)")
    args = parser.parse_args()

    csv_files = []
    if args.log:
        for pattern in args.log:
            csv_files.extend(glob.glob(os.path.expanduser(pattern)))
    if args.logdir:
        logdir = os.path.expanduser(args.logdir)
        csv_files.extend(glob.glob(os.path.join(logdir, "ai_guider_*.csv")))
    if not csv_files:
        default_dir = os.path.expanduser("~/.local/share/kstars/ai_debug_logs/")
        csv_files = glob.glob(os.path.join(default_dir, "ai_guider_*.csv"))
        if not csv_files:
            print("No AI debug CSV files found. Specify --log or --logdir.", file=sys.stderr)
            sys.exit(1)

    csv_files = sorted(set(csv_files), key=os.path.getmtime)
    if args.latest > 0:
        csv_files = csv_files[-args.latest:]

    print(f"Found {len(csv_files)} CSV file(s) to analyze.")
    print()

    baseline_seg = None
    if args.baseline:
        try:
            baseline_seg, _, _ = analyze_one(args.baseline, args.weights, args.min_confidence, args.flip_window)
            print_report(f"BASELINE: {os.path.basename(args.baseline)}", baseline_seg, {"error": "n/a for baseline"},
                         {"error": "n/a for baseline"})
        except Exception as e:
            print(f"Could not analyze baseline {args.baseline}: {e}", file=sys.stderr)

    for path in csv_files:
        try:
            seg, periodogram, acf = analyze_one(path, args.weights, args.min_confidence, args.flip_window)
            print_report(os.path.basename(path), seg, periodogram, acf)
            if baseline_seg and "error" not in seg:
                cmp = compare_sessions(seg, baseline_seg)
                print(f"  vs. baseline: RA {cmp['ra_improvement_pct']:+.1f}%  "
                      f"DEC {cmp['dec_improvement_pct']:+.1f}%")
                print()
        except Exception as e:
            print(f"[{os.path.basename(path)}] ERROR: {e}", file=sys.stderr)


if __name__ == "__main__":
    main()
