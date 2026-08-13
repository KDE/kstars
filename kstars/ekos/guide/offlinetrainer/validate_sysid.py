#!/usr/bin/env python3
"""
offline_trainer/validate_sysid.py — quick QC pass over a sysid_data_*.json file,
usable *during* a live AI Guiding Assistant / PID Auto-Tune run (the file is
rewritten after every completed session) or on a finished one, without waiting
for the full trainer.

Written after a real bug (2026-08-03): once PID Auto-Tune was reordered to run
first in the protocol, pulse-response pulses could fire before guide calibration
had actually finished, silently recording ra_raw_px/dec_raw_px = 0.0 for every
single frame. Nothing about that looked wrong until someone loaded the JSON and
checked the raw numbers. This script exists to make that check a one-liner
instead of an ad hoc investigation.

Usage:
    python3 validate_sysid.py <sysid_data.json> [--verbose]
    python3 validate_sysid.py --latest              # newest file in ai_training_logs/
    python3 validate_sysid.py --latest --pid-lambda-factor 4.0

SPDX-License-Identifier: GPL-2.0-or-later
"""

import argparse
import glob
import json
import os
import sys

import numpy as np

from pulse_response_fit import fit_pulse_response
from pid_autotune import recommend_pid_gains, SIMC_LAMBDA_L_FACTOR

try:
    from train_harmonic import _estimate_pe, _effective_pixel_scale
    HAVE_PE = True
except ImportError:
    HAVE_PE = False


AI_TRAINING_LOGS_DIR = os.path.expanduser("~/.local/share/kstars/ai_training_logs")


def _latest_sysid_file():
    files = sorted(glob.glob(os.path.join(AI_TRAINING_LOGS_DIR, "sysid_data_*.json")))
    if not files:
        print(f"No sysid_data_*.json files found in {AI_TRAINING_LOGS_DIR}", file=sys.stderr)
        sys.exit(1)
    return files[-1]


def _pixel_scale(sysid):
    if HAVE_PE:
        return _effective_pixel_scale(sysid)
    eq = sysid.get("equipment", {})
    return float(eq.get("pixel_scale_arcsec_per_px", 1.0) or 1.0)


def _rms_arcsec(frames, axis_key, pixel_scale):
    vals = [f[axis_key] for f in frames if f.get("error_code", 0) == 0]
    if not vals:
        return None
    return float(np.sqrt(np.mean(np.square(vals)))) * pixel_scale


def _check_pulse_response(sysid, guide_exp, pixel_scale, lambda_l_factor, verbose):
    print("\n=== Pulse-response / PID Auto-Tune ===")
    pr_sessions = [s for s in sysid["sessions"] if s.get("type") == "pulse_response"]
    print(f"  {len(pr_sessions)} pulse_response session(s) collected")
    if not pr_sessions:
        print("  (none yet -- nothing to check)")
        return

    zero_sessions = 0
    for s in pr_sessions:
        frames = s.get("response_frames", [])
        if frames and all(f.get("ra_raw_px", 0.0) == 0.0 and f.get("dec_raw_px", 0.0) == 0.0 for f in frames):
            zero_sessions += 1
    if zero_sessions:
        print(f"  *** WARNING: {zero_sessions}/{len(pr_sessions)} session(s) have ra_raw_px/dec_raw_px "
              f"exactly 0.0 in EVERY frame. This is the calibration-not-ready bug (or the guider "
              f"lost the star) -- pulses fired before real RA/DEC error was available. Don't trust "
              f"any gain recommendation derived from this data.")
    else:
        print("  OK: no all-zero pulse_response sessions detected.")

    for axis in ("RA", "DEC"):
        axis_sessions = [s for s in pr_sessions if s.get("pulse_axis") == axis]
        if not axis_sessions:
            continue
        kappa, tau, info = fit_pulse_response(sysid, axis, guide_exp, verbose, return_fits=True)
        fits = info["fits"]
        print(f"  [{axis}] {len(axis_sessions)} sessions -> {len(fits)} usable step-response fit(s), "
              f"sign_consistent={info['sign_consistent']}")
        if fits:
            amps = [abs(f["P_fit_px"]) for f in fits]
            print(f"         amplitude range: {min(amps):.2f}-{max(amps):.2f} px "
                  f"(residual_std range: {min(f['residual_std_px'] for f in fits):.3f}"
                  f"-{max(f['residual_std_px'] for f in fits):.3f} px)")

    cal_note = ("(no calibrated ms/arcsec on record yet -- recommendation unavailable "
                "until a standard_guiding session with ra/dec_ms_per_arcsec exists)")
    gains = recommend_pid_gains(sysid, guide_exp, verbose, lambda_l_factor)
    fp = sysid.get("model_fingerprint", {})
    for axis in ("ra", "dec"):
        g = gains[axis]
        if g["confidence"] == "unavailable":
            print(f"  [{axis.upper()}] recommendation unavailable: {g.get('reason', cal_note)}")
            continue
        res_flag = "  *** resolution-limited (L pinned at first-sample time -- tau/L not really resolved)" \
            if g.get("resolution_limited") else ""
        print(f"  [{axis.upper()}] recommended proportional_gain={g['proportional_gain']:.3f} "
              f"integral_gain={g['integral_gain']:.3f} (confidence={g['confidence']}, "
              f"n_fits={g['n_fits']}, K={g['process_gain_arcsec_per_ms']:.5f}\"/ms, "
              f"tau={g['tau_s']:.2f}s, L={g['dead_time_s']:.2f}s){res_flag}")

        # Cross-check: the live C++ gain-lock (a simplified plateau-average, no curve fit)
        # may have already applied a gain -- compare it against this fuller offline recompute.
        # A big disagreement is worth investigating (bad data, or the two implementations
        # diverging) rather than assuming either one is automatically right.
        live_gain = fp.get(f"{axis}_proportional_gain")
        if live_gain is not None and g["proportional_gain"] > 0:
            delta_pct = 100.0 * (live_gain - g["proportional_gain"]) / g["proportional_gain"]
            if abs(delta_pct) > 15.0:
                print(f"         *** live-locked gain ({live_gain:.3f}) differs from this recompute "
                      f"by {delta_pct:+.0f}% -- worth checking why (C++ plateau-average vs. Python "
                      f"curve fit can differ some, but not usually this much)")


def _check_free_drift(sysid, pixel_scale, verbose):
    print("\n=== Free-drift sessions ===")
    sessions = [s for s in sysid["sessions"] if s.get("type") == "free_drift"]
    print(f"  {len(sessions)} free_drift session(s)")
    for s in sessions:
        frames = s.get("frames", [])
        dur = s.get("duration_s", 0)
        rms_ra = _rms_arcsec(frames, "ra_raw_px", pixel_scale)
        rms_dec = _rms_arcsec(frames, "dec_raw_px", pixel_scale)
        hf = s.get("hf_motion_arcsec")
        flag = ""
        if len(frames) < 10:
            flag = "  *** WARNING: very few frames -- segment likely too short to be useful"
        print(f"  {s.get('session_id', '?'):45s} dur={dur:5d}s frames={len(frames):4d} "
              f"RMS(ra/dec)={rms_ra if rms_ra is None else round(rms_ra,2)}/"
              f"{rms_dec if rms_dec is None else round(rms_dec,2)}\" "
              f"hf_noise={hf if hf is None else round(hf,2)}\"{flag}")


def _check_standard_guiding(sysid, pixel_scale, verbose):
    print("\n=== Standard-guiding sessions ===")
    sessions = [s for s in sysid["sessions"] if s.get("type") == "standard_guiding"]
    print(f"  {len(sessions)} standard_guiding session(s)")
    for s in sessions:
        frames = s.get("frames", [])
        dur = s.get("duration_s", 0)
        rms_ra = _rms_arcsec(frames, "ra_raw_px", pixel_scale)
        rms_dec = _rms_arcsec(frames, "dec_raw_px", pixel_scale)
        gain_ra = s.get("aggressiveness_ra")
        gain_dec = s.get("aggressiveness_dec")
        lost = sum(1 for f in frames if f.get("error_code", 0) != 0)
        flag = ""
        if len(frames) < 10:
            flag = "  *** WARNING: very few frames -- segment likely too short to be useful"
        elif lost > 0.1 * len(frames):
            flag = f"  *** WARNING: {lost}/{len(frames)} frames flagged star-lost"
        print(f"  {s.get('session_id', '?'):45s} dur={dur:5d}s frames={len(frames):4d} "
              f"gain(ra/dec)={gain_ra}/{gain_dec} "
              f"RMS(ra/dec)={rms_ra if rms_ra is None else round(rms_ra,2)}/"
              f"{rms_dec if rms_dec is None else round(rms_dec,2)}\"{flag}")


def _check_artifacts(sysid, verbose):
    """
    Cross-session anomaly/coverage summary -- the things worth flagging for improving
    training data quality, the inference code, or the C++ AI guide engine itself, not
    just per-session numbers. Returns a list of (severity, message) tuples for the
    final verdict.
    """
    print("\n=== Artifacts & coverage ===")
    findings = []
    sessions = sysid.get("sessions", [])

    pr = [s for s in sessions if s.get("type") == "pulse_response"]
    zero_pr = sum(1 for s in pr if s.get("response_frames") and
                  all(f.get("ra_raw_px", 0.0) == 0.0 and f.get("dec_raw_px", 0.0) == 0.0
                      for f in s["response_frames"]))
    if zero_pr:
        findings.append(("HIGH", f"{zero_pr} pulse_response session(s) recorded all-zero "
                                  f"raw_px -- calibration-not-ready or star-lost artifact"))

    # Duration shortfall: sessions that ran noticeably shorter than the phase asked for
    # (dither/recenter interruptions, guider aborts) -- train.py silently accepts short
    # segments, but they dilute the fit; worth knowing how much of the data is like this.
    short_free_drift = short_std_guiding = 0
    for s in sessions:
        if s.get("type") not in ("free_drift", "standard_guiding"):
            continue
        frames = s.get("frames", [])
        # Rough expected frame count from duration_s / typical cadence isn't known here
        # without guide_exp context per-call; just flag near-empty segments directly.
        if len(frames) < 10:
            if s["type"] == "free_drift":
                short_free_drift += 1
            else:
                short_std_guiding += 1
    if short_free_drift:
        findings.append(("MEDIUM", f"{short_free_drift} free_drift segment(s) with <10 frames "
                                    f"(interrupted early or barely started)"))
    if short_std_guiding:
        findings.append(("MEDIUM", f"{short_std_guiding} standard_guiding segment(s) with <10 frames"))

    # Star-lost rate across all non-pulse sessions.
    total_frames = total_lost = 0
    for s in sessions:
        if s.get("type") not in ("free_drift", "standard_guiding"):
            continue
        for f in s.get("frames", []):
            total_frames += 1
            if f.get("error_code", 0) != 0:
                total_lost += 1
    if total_frames > 0:
        lost_pct = 100.0 * total_lost / total_frames
        print(f"  Star-lost frames: {total_lost}/{total_frames} ({lost_pct:.1f}%)")
        if lost_pct > 5.0:
            findings.append(("MEDIUM", f"{lost_pct:.1f}% of frames flagged star-lost across the run "
                                        f"-- check guide star SNR/exposure for this rig"))

    # DEC rate far below RA means the calibration ate DEC backlash.
    suspect_cal = []
    for s in sessions:
        if s.get("type") not in ("free_drift", "standard_guiding"):
            continue
        ra_cal, dec_cal = s.get("ra_ms_per_arcsec"), s.get("dec_ms_per_arcsec")
        if ra_cal and dec_cal and dec_cal > 2.0 * ra_cal:
            suspect_cal.append(f"{s.get('session_id', '?')} (dec={dec_cal:.0f} vs ra={ra_cal:.0f} ms/\")")
    if suspect_cal:
        findings.append(("HIGH", f"backlash-poisoned DEC calibration in {len(suspect_cal)} guiding "
                                 f"session(s): {', '.join(suspect_cal)} -- recalibrate DEC (rate "
                                 f"should roughly match RA) and re-collect before training"))

    # PE line check on the longest free drift (the one train_worm_gear's FFT uses).
    fd = [s for s in sessions if s.get("type") == "free_drift" and len(s.get("frames", [])) >= 60]
    if fd:
        import scipy.signal
        import scipy.stats
        s = max(fd, key=lambda x: len(x["frames"]))
        t = np.cumsum([f.get("dt", 2.0) for f in s["frames"]])
        ra = np.array([f["ra_raw_px"] for f in s["frames"]])
        slope, ic, _, _, _ = scipy.stats.linregress(t, ra)
        fv = np.linspace(0.001, 0.03, 2000)
        P = scipy.signal.lombscargle(t, ra - (slope * t + ic), 2 * np.pi * fv, precenter=True)
        pk = np.argmax(P)
        far = np.abs(fv - fv[pk]) > 0.2 * fv[pk]
        dom = P[pk] / max(P[far].max(), 1e-9)
        print(f"  Free-drift PE line: peak {1.0 / fv[pk]:.0f}s, dominance {dom:.2f} "
              f"(from {s.get('session_id', '?')})")
        if dom < 2.0:
            findings.append(("HIGH", f"no dominant PE line in the longest free drift "
                                     f"(dominance {dom:.2f}) -- the period estimate is unreliable; "
                                     f"check whether the drift phase actually guided (pulses "
                                     f"executed) and re-collect"))

    # Pier-side and sky coverage -- directly relevant both to training (drift/refraction fit
    # validity range) and to planning the later live-evaluation position matrix.
    pier_sides = {s.get("pier_side") for s in sessions if s.get("pier_side")}
    alts = [s.get("altitude_deg") for s in sessions if s.get("altitude_deg") is not None]
    azs = [s.get("azimuth_deg") for s in sessions if s.get("azimuth_deg") is not None]
    print(f"  Pier sides tested: {sorted(pier_sides) if pier_sides else 'unknown'}")
    if alts:
        print(f"  Altitude range covered: {min(alts):.0f}-{max(alts):.0f} deg")
    if len(pier_sides) < 2:
        findings.append(("INFO", f"Only pier side(s) {sorted(pier_sides)} tested in this data collection "
                                  f"run -- the live evaluation phase should deliberately cover the other "
                                  f"side (meridian flip) since drift/refraction/backlash can differ."))

    # PE harmonic count vs. the live HarmonicGuider's fixed 2-line cap.
    pe_lines = sysid.get("physical", {}).get("pe_lines") if "physical" in sysid else None
    # sysid files from a live (not-yet-trained) run don't have "physical" -- that's only in
    # a trained weights.json. Nothing to check here pre-training; see the trained-weights
    # inspection step in the test plan instead.

    noise_floor = sysid.get("noise_floor_arcsec")
    if noise_floor is not None:
        print(f"  Recorded seeing/noise floor (unguided HF star motion): {noise_floor:.2f}\"")
        if noise_floor > 2.0:
            findings.append(("INFO", f"Seeing/noise floor is {noise_floor:.2f}\" -- unusually high; "
                                      f"any RMS close to this floor is at the limit of what guiding "
                                      f"can physically improve, not a guider defect"))
    else:
        findings.append(("INFO", "No noise_floor_arcsec recorded yet (needs a free_drift session "
                                  ">30 frames to measure)"))

    if not findings:
        print("  No artifacts detected.")
    else:
        for severity, msg in findings:
            print(f"  [{severity}] {msg}")
    return findings


def _check_pe(sysid, guide_exp, verbose):
    if not HAVE_PE:
        return
    print("\n=== PE period preview (train_harmonic._estimate_pe) ===")
    period, amplitude, lines = _estimate_pe(sysid, guide_exp, verbose)
    if period <= 0:
        print("  No significant PE detected yet (may need more free_drift/standard_guiding data).")
        return
    print(f"  Primary period: {period:.1f}s  amplitude: {amplitude:.3f}px")
    for line in lines[1:]:
        print(f"  Secondary: {line['period_s']:.1f}s  amplitude={line['amplitude_px']:.3f}px  SNR={line['snr']:.0f}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("sysid_file", nargs="?", help="Path to sysid_data_*.json")
    ap.add_argument("--latest", action="store_true", help="Use the newest file in ai_training_logs/")
    ap.add_argument("--pid-lambda-factor", type=float, default=SIMC_LAMBDA_L_FACTOR)
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    if args.latest or not args.sysid_file:
        path = _latest_sysid_file()
    else:
        path = args.sysid_file

    print(f"Loading {path}")
    with open(path) as f:
        sysid = json.load(f)

    eq = sysid.get("equipment", {})
    fp = sysid.get("model_fingerprint", {})
    guide_exp = eq.get("guide_exposure_ms", 1000.0) / 1000.0
    pixel_scale = _pixel_scale(sysid)

    print(f"\nmount_type={eq.get('mount_type')}  mount_name={eq.get('mount_name')}  "
          f"camera={eq.get('camera')}  pixel_scale={pixel_scale:.4f}\"/px  guide_exp={guide_exp:.2f}s")
    print(f"Current fingerprint: ra_gain={fp.get('ra_proportional_gain')} "
          f"dec_gain={fp.get('dec_proportional_gain')} "
          f"ra_int={fp.get('ra_integral_gain')} dec_int={fp.get('dec_integral_gain')}")

    from collections import Counter
    counts = Counter(s.get("type") for s in sysid.get("sessions", []))
    print(f"Sessions so far: {dict(counts)}")

    _check_pulse_response(sysid, guide_exp, pixel_scale, args.pid_lambda_factor, args.verbose)
    _check_free_drift(sysid, pixel_scale, args.verbose)
    _check_standard_guiding(sysid, pixel_scale, args.verbose)
    findings = _check_artifacts(sysid, args.verbose)
    _check_pe(sysid, guide_exp, args.verbose)

    print("\n=== Verdict ===")
    high = [m for sev, m in findings if sev == "HIGH"]
    medium = [m for sev, m in findings if sev == "MEDIUM"]
    if high:
        print("  NOT READY -- fix the HIGH-severity issue(s) above before training or trusting any "
              "gain recommendation from this data:")
        for m in high:
            print(f"    - {m}")
    elif medium:
        print("  USABLE WITH CAVEATS -- training will run, but review the MEDIUM findings above:")
        for m in medium:
            print(f"    - {m}")
    else:
        print("  Looks OK -- no high/medium-severity issues detected.")

    print("\nDone.")


if __name__ == "__main__":
    main()
