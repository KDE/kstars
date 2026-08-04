# AI Guider Evaluation Strategy

> This document lives in-tree at `kstars/ekos/guide/offlinetrainer/EVALUATION.md`, alongside `AI_ARCHITECTURE.md` (the full design reference) and `evaluate_shadow.py` (the tool this document describes the use of).

**This document answers three questions:**
1. How do we evaluate the AI guider safely, before trusting it with real corrections?
2. How does AI state and per-frame prediction data get logged in KStars?
3. How do we interpret `evaluate_shadow.py`'s output to decide whether to activate AI guiding?

---

## Table of Contents

1. [The Core Problem: Evaluation Under a Closed Loop](#1-the-core-problem)
2. [Operating Modes](#2-operating-modes)
3. [Shadow Mode: The Safe Evaluation Path](#3-shadow-mode-the-safe-evaluation-path)
4. [Counterfactual RMS Math](#4-counterfactual-rms-math)
5. [Frame Quality Policy](#5-frame-quality-policy)
6. [AI State and the Debug Log](#6-ai-state-and-the-debug-log)
7. [Using evaluate_shadow.py](#7-using-evaluate_shadowpy)
8. [Interpreting Results: Go / No-Go Criteria](#8-interpreting-results)
9. [Future Work](#9-future-work)

---

## 1. The Core Problem

Evaluating a guiding correction algorithm is fundamentally different from evaluating a classifier or a regression model. The output of the algorithm (the correction pulse) **changes the state of the system** (the mount position), which changes the next input to the algorithm. This is a closed-loop control system, not an open-loop prediction problem.

This means:

- You **cannot** evaluate the AI on historical passive guide logs by comparing its predicted correction to what the classic controller did. The classic controller corrected for an error, so the AI never sees what the error would have been without that correction.
- You **cannot** simply swap in the AI and look at RMS, because if the AI is worse, you have already degraded the user's guiding session.
- You **need** a way to observe what the AI would have done, without actually doing it, while the classic controller continues to safely guide.

This is called **shadow mode** evaluation.

---

## 2. Operating Modes

### 2.1 What the Typical User Sees

A user who has run the Guide AI Assistant sees exactly two user-visible states:

```
Guiding session starts
        │
        ▼
┌──────────────────┐
│  WARMUP           │  AI builds its internal history (Kalman/RLS state, confidence ramp).
│                   │  Classic controller issues all pulses — no change in behaviour.
│  Shown in UI as:  │  Guide graph looks identical to non-AI guiding.
│  "AI: Warming up" │
└────────┬─────────┘
         │ warmup complete
         ▼
┌──────────────────┐
│  ACTIVE           │  AI feed-forward corrections blend in automatically.
│                   │
│  Shown in UI as:  │
│  "AI: Active      │◄────► FALLBACK (auto, invisible to user)
│   conf: 0.82"     │       If confidence drops, AI authority reduces toward zero.
└──────────────────┘       Classic controller takes over. AI re-engages when
                           confidence recovers. No user action required.
```

Run the Guide AI Assistant once per equipment profile, select an AI algorithm for RA and/or DEC in Guide Options, start guiding. The AI becomes active automatically once warmup completes. That's the complete user story.

### 2.2 Enabling AI Guiding

There's no separate master "AI enabled" toggle. Enabling AI **is** selecting the AI algorithm as `RAGuidePulseAlgorithm` and/or `DECGuidePulseAlgorithm` — the same per-axis combo box used to choose Standard/Hysteresis/Linear/GPG. If an AI algorithm is selected on either axis and matching, fingerprint-valid weights are loaded, that axis moves through WARMUP into ACTIVE automatically.

If an AI algorithm **is** selected but the weights file fails to load, or its fingerprint doesn't match the current guide settings, guiding **aborts outright** with an error notification rather than silently falling back to the classical controller. This is deliberate: a session where the user believes AI is active but it silently isn't is worse than one that refuses to start and says why.

### 2.3 The Five Internal States

`AIGuideState` (`internalguide/gmath.h`):

```cpp
enum class AIGuideState { DISABLED, WARMUP, SHADOW, ACTIVE, FALLBACK };
```

- **DISABLED** — no weights loaded, or no AI algorithm selected and shadow mode off. Zero overhead.
- **WARMUP** — an AI algorithm is selected and weights are loaded; building history. Classic controller issues all pulses.
- **ACTIVE** — feed-forward blended with the proportional controller. Normal production mode.
- **FALLBACK** — was ACTIVE; confidence dropped. AI authority reduces toward zero; keeps running in the background and re-engages automatically once confidence recovers.
- **SHADOW** — developer/QA evaluation mode, this document's main subject.

### 2.4 When Is SHADOW Mode Used?

SHADOW mode is **not part of the normal user flow**. It's reached when:
- Neither `RAGuidePulseAlgorithm` nor `DECGuidePulseAlgorithm` is set to AI, **and**
- `Options::aIShadowMode()` is **true** ("Shadow mode logging" in AI Guiding options), **and**
- A weights file is loaded for the current equipment profile

The AI runs full inference every frame and logs predictions (§6) without ever touching the dispatched pulse. This is specifically for:
1. **Developers** testing a new model implementation before it ships
2. **Researchers** validating that `evaluate_shadow.py`'s counterfactual analysis matches real ACTIVE-mode results
3. **Power users** who want to verify their trained model improves RMS on their specific setup before trusting it with real corrections

A typical user never encounters SHADOW mode — selecting an AI algorithm goes straight through WARMUP → ACTIVE.

### 2.5 The Complete User Flow

```
FIRST TIME (once per equipment profile, ~20-70 minutes depending on mount class)
──────────────────────────────────────────────────────
1. Open Guide AI Assistant wizard in KStars
2. Wizard detects mount type from the connected mount's name; confirm or override
3. Wizard runs PID Auto-Tune, then slews to calibration positions and collects
   sysid data (free-drift + guided + pulse-response depending on mount type)
4. Click "Train Model" (local Python trainer, or forward to EkosLive)
   → weights.json saved to the equipment profile
5. Wizard shows: "Model trained. AI Guiding is ready."

EVERY SUBSEQUENT SESSION (no setup required)
────────────────────────────────────────────
1. Select the AI algorithm for RA and/or DEC in Guide Options [stays selected]
2. Start guiding as usual
3. Once warmup completes: AI becomes ACTIVE automatically
4. Guide graph shows improved RMS ← this is the expected result

IF SOMETHING CHANGES (settings, new OTA, seasonal)
───────────────────────────────────────────────────
- Change any Category A guide setting (exposure, binning, gain) → fingerprint
  mismatch → guiding refuses to start with an AI algorithm selected until the
  Guide AI Assistant is re-run
- Swap OTA / change balance significantly → retrain recommended (PE amplitude changes)
```

---

## 3. Shadow Mode: The Safe Evaluation Path

In shadow mode:

1. Each guide frame, `predict()` is called exactly as it would be in ACTIVE mode.
2. The resulting `GuideOutput` (correction arcsec, confidence, physics/mlp breakdown) is logged to the AI debug CSV (§6).
3. The actual correction pulse dispatched to the mount is computed by the classic proportional controller only — the AI output never touches the pulse duration.
4. The result: a CSV where every row has both what the classic controller actually did, and what the AI would have added.

This log is then fed into `evaluate_shadow.py` (§7) to compute counterfactual RMS.

### Safety Guarantees of Shadow Mode

- Guiding performance is identical to classic-only guiding from the mount's perspective.
- No risk of the AI degrading a live session.
- The AI's full runtime inference path is exercised, validating the C++ code end-to-end.
- Multiple sessions of shadow data can be collected before ever activating the AI.

---

## 4. Counterfactual RMS Math

### The Approximation

At frame N, the classic controller sees the RA distance (arcsec) and dispatches:
```
classic_correction_arcsec = raDistance[N] × proportional_gain × pixel_scale
```

The AI, in shadow mode, computes a feed-forward term logged to the CSV as `pred_ra_arcsec`. In ACTIVE mode, the total correction would have been:
```
total_correction_arcsec = classic_correction_arcsec + ai_feedforward_arcsec
```

The observed residual at frame N+1 (`ra_error_arcsec[N+1]`) is the actual RA error after the classic correction was applied. The counterfactual residual — what the error at N+1 would have been if the AI correction had also been applied — is:
```
counterfactual_ra_residual[N+1] ≈ ra_error_arcsec[N+1] - ai_gain × conf[N] × pred_ra_arcsec[N]
```

**Why this works**: the classic pulse moved the mount by `classic_correction_arcsec`. If the AI had added `ai_feedforward_arcsec` on top of that, the mount would have moved by that additional amount, reducing the next-frame error by that amount. This is a first-order linear approximation.

### Validity Conditions

This approximation is most accurate when:
1. The mount response is approximately linear (valid for small corrections)
2. The AI feed-forward is small relative to the total correction
3. The classic correction didn't hit the max-pulse ceiling
4. The frame interval is the standard guide cadence (not a dropped frame)

`evaluate_shadow.py` does not currently detect or exclude frames that violate 3 or 4 — see §5 and §9.

---

## 5. Frame Quality Policy

### 5.1 Sysid / Training Side (implemented)

The offline trainers (`train_worm_gear.py`, `train_harmonic.py`, `train_direct_drive.py`) apply a real frame-exclusion policy before fitting anything — see `AI_ARCHITECTURE.md` §5.4 for the full table (`error_code ≠ 0`, low SNR, dropped-frame gaps, post-restart re-convergence windows, large error spikes are all excluded from training). `validate_sysid.py` runs the same checks as a standalone QC pass over a sysid file.

### 5.2 Shadow-Mode Evaluation Side (`evaluate_shadow.py`)

`evaluate_shadow.py` applies exactly one filter today: it skips a configurable number of leading frames (`--warmup`, default 30) and computes RMS/correlation over everything after that. It does **not** currently exclude dithered frames, `ErrorCode ≠ 0` frames, post-restart re-convergence windows, or flag large error spikes / saturated pulses / low-SNR frames the way the sysid/training side does. In practice this means:
- Keep shadow-mode evaluation sessions dither-free and abort-free — nothing currently cleans that up after the fact.
- A session with a few large seeing spikes or a dropped-frame gap will bias the reported RMS numbers with no warning from the script itself.

Porting the training-side exclusion policy over to `evaluate_shadow.py` is a well-scoped, self-contained improvement — see §9.

---

## 6. AI State and the Debug Log

All AI-specific logging lives in `internalguide/gmath.h`/`.cpp` — the PHD2-format guide log (`guidelog.h`/`.cpp`) carries no AI-specific fields; it remains exactly the general-purpose guide-log writer it always was.

`cgmath` owns a dedicated debug log file, opened once per session at `<AppLocalDataLocation>/ai_debug_logs/ai_guider_<timestamp>.csv`, written once per guide frame regardless of which algorithm is active (`AIGuideState` transitions are also handled in `cgmath::performProcessing()`). The header:

```
t_session,dt,altitude_deg,azimuth_deg,parallactic_angle_deg,ra_error_arcsec,uncorrected_ra_delta_px,
dec_error_arcsec,uncorrected_dec_delta_px,conf,pred_ra_arcsec,physics_ra_arcsec,mlp_ra_arcsec,
pred_dec_arcsec,physics_dec_arcsec,mlp_dec_arcsec,ai_state,pe_statestring,
ra_algorithm,ra_prop_response_ms,ra_integral_response_ms,ra_ai_response_ms,ra_active_prop_gain,
ra_total_pulse_ms,ra_direction,ra_suppressed,
dec_algorithm,dec_prop_response_ms,dec_integral_response_ms,dec_ai_response_ms,dec_active_prop_gain,
dec_total_pulse_ms,dec_direction,dec_suppressed
```

This is deliberately richer than just "the AI's opinion": `ra_algorithm`/`dec_algorithm` (`AI | AI-Dark | GPG | GPG-Dark | Linear | Hysteresis | Standard`) and the per-axis response breakdown record what was *actually sent to the mount* that frame, alongside the AI's prediction split into `physics`/`mlp` components for diagnostics. This is exactly the data `evaluate_shadow.py`'s counterfactual analysis needs — the classic response and the AI's counterfactual contribution are both present in the same row. See `AI_ARCHITECTURE.md` §6.2, §8, and §10 for the full inference architecture and blend logic that produces these values.

---

## 7. Using evaluate_shadow.py

```bash
cd kstars/ekos/guide/offlinetrainer

# Evaluate one or more AI debug CSVs directly (glob patterns supported)
python3 evaluate_shadow.py --log ~/.local/share/kstars/ai_debug_logs/ai_guider_20260613_211502.csv

# Evaluate every CSV in a directory
python3 evaluate_shadow.py --logdir ~/.local/share/kstars/ai_debug_logs/

# Only the N most recently modified files
python3 evaluate_shadow.py --logdir ~/.local/share/kstars/ai_debug_logs/ --latest 3

# Match the counterfactual gain to what would actually be applied at your current
# Options::aIPredictionGain() (the script has no access to live KStars config,
# so pass it explicitly — default is 1.0, not the UI's 0.5 default)
python3 evaluate_shadow.py --log foo.csv --gain 0.5

# Change how many leading frames are skipped as warmup (default 30)
python3 evaluate_shadow.py --log foo.csv --warmup 50
```

With no `--log`/`--logdir` given, it defaults to scanning `~/.local/share/kstars/ai_debug_logs/ai_guider_*.csv`.

### Output

```
Found 1 CSV file(s) to evaluate.

==============================================================================
  AI GUIDER — SHADOW MODE EVALUATION REPORT
==============================================================================

  ⚠  ACCURACY WARNING: These results are APPROXIMATE due to
     closed-loop distribution shift. See script header for details.
     True accuracy requires closed-loop simulation with free-drift data.

  Session: ai_guider_20260613_211502.csv
  Frames (post-warmup): 806
  Shadow fraction: 100%
  Mean confidence: 0.72

  Metric                             Standard  AI (approx)          Δ
  ------------------------------ ------------ ------------ ----------
  RA RMS (arcsec)                       0.821        0.509     +38.0%
  DEC RMS (arcsec)                      0.341        0.318      +6.7%
  Total RMS (arcsec)                    0.622        0.415     +33.3%

  Quality Indicators:
    RA pred-error correlation:   +0.612  [GOOD]
    DEC pred-error correlation:  +0.341  [GOOD]
    RA sign agreement:           78%
    DEC sign agreement:          71%

  VERDICT: AI model looks promising for RA. Consider enabling AI guiding.

------------------------------------------------------------------------------
```

### What the Script Actually Checks

- **Correlation and sign agreement are first-class outputs, not just RMS.** A model can show a positive RMS "improvement" purely from a lucky bias cancellation on a specific session while having near-zero or negative correlation with the actual error. The verdict logic correctly weights correlation as the primary gate (`ra_correlation > 0.3`) with RMS improvement as a secondary check: a model that "truly understands the mount" should have positive-correlated predictions with the error.
- **The verdict is a single generic rule, not the mount-type-specific targets from §8.** `ra_correlation > 0.3 and ra_improvement_pct > 5` → "promising"; `ra_correlation < 0` → "do NOT enable" (a hard stop — likely a fingerprint mismatch or stale training data); otherwise a middle "modest improvement" or "needs retraining" verdict. Compare the printed RA/DEC/total improvement percentages against §8's table by hand, per mount type — that table is the project's actual acceptance bar.
- **No plots.** The raw per-frame data needed to build a time-series overlay or rolling-RMS window plot is already in the CSV (`t_session`, `ra_error_arcsec`, `pred_ra_arcsec`, etc.) if that's ever wanted.

### What to Look For

| Signal | Meaning |
|---|---|
| AI CF RMS < Classic RMS, correlation > 0.3 | AI is correctly predicting systematic error |
| AI CF RMS > Classic RMS | AI is over-correcting — do NOT activate |
| Negative correlation | AI predictions are anti-correlated with error — the hard "do NOT enable" case; likely a fingerprint mismatch or stale training data, not just noise |
| Improvement concentrated in RA | PE model working (expected for worm gear and harmonic drive) |
| High variance in counterfactual | Model is inconsistent — check warmup period |
| Low sign agreement despite positive correlation | Model gets direction right on average but is noisy frame-to-frame — expect this to matter more at low SNR |

---

## 8. Interpreting Results

### Go Criteria (SAFE to activate AI)

| Mount Type | RA Improvement | DEC Improvement | Safety |
|---|---|---|---|
| WORM_GEAR | > +15% | > +5% | CF never worse than classic by > 5% |
| HARMONIC_DRIVE | > +8% combined | > +5% | CF never worse by > 5% |
| DIRECT_DRIVE | > +5% combined | > +5% | CF never worse by > 5% |

### No-Go Signals

1. **Negative total improvement**: the AI is making things worse in aggregate. The model is either the wrong type, badly fitted, or the session is seeing-dominated.
2. **CF degradation in RA by > 5%**: a hard no-go. Even occasional over-corrections at specific PE phases can cause large spike errors.
3. **Mean AI confidence < 0.4**: the model doesn't trust its own predictions. Check: was warmup long enough? Do the weights' fingerprint match current guide settings?
4. **Improvement disappears after a few hundred frames**: the PE phase estimate may be drifting — check whether online phase adaptation is running (`WormGearGuider`'s RLS phase tracker, `AI_ARCHITECTURE.md` §6.1).

### The "One Session Rule"

Don't make a go/no-go decision from a single session. Collect at least 3 sessions on the same equipment profile: the first may have unusual seeing or balance; by the third, the model has "seen" the PE cycle multiple times and its phase/Kalman state has had a chance to show it's stable. If all sessions independently satisfy the go criteria, activate the AI.

---

## Appendix: Shadow Mode Session Checklist

Before collecting a shadow-mode session for evaluation:

- [ ] AI weights file is loaded for the current equipment profile (`Options::aIGuiderWeightsFile()` points at a valid, fingerprint-matching file)
- [ ] RA and DEC guide-pulse algorithms are **not** set to AI (so the session runs SHADOW, not ACTIVE)
- [ ] `Options::aIShadowMode()` is **true** ("Shadow mode logging" in AI Guiding options)
- [ ] Guide settings fingerprint matches the training fingerprint (check the AI Guiding options panel)
- [ ] Session is at least 30 minutes long (need several PE cycles for worm gear / harmonic drive mounts)
- [ ] Seeing is at least moderate (SNR > 15 consistently)
- [ ] No dithering during the session — `evaluate_shadow.py` doesn't currently filter dithered/error/restart frames (§5.2), so they'll bias the RMS numbers rather than just reducing sample size

---

## 9. Future Work

- **Port the sysid-side frame quality policy (§5.1) into `evaluate_shadow.py`**: dither/`ErrorCode≠0`/restart-gap exclusion, plus flagging (not excluding) large spikes and saturated pulses from the counterfactual specifically. `validate_sysid.py` is a reasonable template for the classification logic.
- **Plots**: a time-series overlay (classic vs. counterfactual error) and a rolling-RMS window plot would help visualize warmup convergence and consistency — the underlying per-frame data is already in the CSV.
- **Closed-loop simulation**: the counterfactual approximation in §4 breaks down for large corrections or saturated pulses; a proper closed-loop simulation (replaying free-drift ground truth through a simulated controller with the AI actually driving pulses) would give a more accurate before-activation estimate than the linear approximation, at the cost of needing free-drift data and a mount response model.
