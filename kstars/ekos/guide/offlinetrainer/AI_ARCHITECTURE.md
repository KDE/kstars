# Ekos AI Guiding — Full Architecture Design

> **Status**: Implemented and shipped in KStars, under `kstars/ekos/guide/`. This document lives in-tree at `kstars/ekos/guide/offlinetrainer/AI_ARCHITECTURE.md` and is the canonical design reference for the feature — keep it up to date as the code evolves, the same way any other living architecture doc in this repo would be maintained.
>
> **Companion feature already in KStars**: `kstars/ekos/guide/internalguide/MPI_IS_gaussian_process/` is a vendored copy of the Max-Planck-Institute for Intelligent Systems' Gaussian Process Guiding (GPG) library — the same predictive-RA-PE algorithm used by PHD2's "Predictive PEC", based on Klenske, Zeilinger, Schölkopf & Hennig, *"Gaussian Process-Based Predictive Control for Periodic Error Correction,"* IEEE Trans. Control Systems Technology, 2015 (reported ~20% RMS reduction on a real mount). GPG and the AI Guider described in this document are **mutually exclusive** at the algorithm-selection level (`RAGuidePulseAlgorithm`/`DECGuidePulseAlgorithm`, see §9.1) — GPG is a narrower, single-purpose RA-PE predictor; the AI Guider is a broader system covering multiple mount classes and error types (drift, refraction, DEC, elastic wind-up) with an offline per-mount training step. See §12.9 for how the two relate.

---

## Table of Contents

1. [Vision and Goals](#1-vision-and-goals)
2. [Guiding Error Taxonomy](#2-guiding-error-taxonomy)
3. [Mount Classification System](#3-mount-classification-system)
4. [Guide AI Assistant — System Identification Protocol](#4-guide-ai-assistant--system-identification-protocol)
5. [Training Pipeline](#5-training-pipeline)
6. [Per-Mount-Type Model Architectures](#6-per-mount-type-model-architectures)
7. [Feature Set Specification](#7-feature-set-specification)
8. [C++ Runtime Architecture](#8-c-runtime-architecture)
9. [Guide Settings Classification and Model Validity](#9-guide-settings-classification-and-model-validity)
10. [KStars Integration](#10-kstars-integration)
11. [Evaluation Framework](#11-evaluation-framework)
12. [Risks and Limitations](#12-risks-and-limitations)
13. [Future Work and Known Limitations](#13-future-work-and-known-limitations)

---

## 1. Vision and Goals

### What We Are Building

An AI-powered guiding layer for Ekos that learns the mechanical and systematic error signature of a specific mount and optical train, and uses that knowledge to issue **predictive (feed-forward) corrections** that reduce guiding RMS beyond what a reactive proportional-integral controller can achieve alone.

The system consists of three major components:

1. **Guide AI Assistant**: An automated data collection wizard that characterizes the mount's error signature through controlled experiments (system identification). Runs once per equipment profile, not every session.

2. **Per-Mount-Type Predictive Model**: A trained model, specific to the mount class and optical train, that predicts the residual mount drift at each guide step. Three model classes: worm gear, harmonic drive, and direct drive.

3. **Confidence-Gated Controller**: Combines the standard proportional controller (existing Ekos behavior) with the AI's feed-forward prediction. Confidence gates the blend, ensuring the AI only increases correction when its predictions are demonstrably accurate. The proportional fallback is always safe.

### What This System Explicitly Does NOT Do

- It does **not** replace the existing proportional controller. It augments it.
- It does **not** predict atmospheric seeing. Seeing is stochastic white noise at guide timescales. Any model that claims to predict seeing is over-fitting to noise.
- It does **not** train from scratch every session. Models are trained once per equipment profile and loaded at session start.
- It does **not** require cloud connectivity. EkosLive is an optional enhancement, not a dependency (see §5.4).
- It does **not** require a GPU for inference. All models run in real-time C++ with straightforward arithmetic (a Kalman filter update and small MLP evaluations, each a handful of microseconds per frame).

---

## 2. Guiding Error Taxonomy

Understanding what types of errors exist is prerequisite to knowing which can be predicted. All guiding error can be decomposed into three components:

### 2.1 Predictable Systematic Errors (AI Can Help)

| Error Type | Description | Predictable? | Dominant Mount Class |
|---|---|---|---|
| **Periodic Error (PE)** | Deterministic oscillation from gear tooth mesh (worm gear) or strain-wave flexspline flex (harmonic drive). Highly repeatable. | Strongly | Worm gear, Harmonic drive |
| **Polar Alignment Drift** | Slow DEC drift from imperfect polar alignment. Near-linear over a session. | Strongly | All mounts |
| **Atmospheric Refraction Drift (RA)** | RA tracking rate error from differential refraction, varies with altitude as `1/cos²(alt)`. | Moderate | All mounts |
| **Atmospheric Refraction Drift (DEC)** | DEC component of refraction drift. Proportional to `sin(parallactic_angle)/cos²(alt)`. Zero on the meridian, maximum near the horizon, and **changes sign** as the object crosses the meridian. Often overlooked but significant at low altitudes or far from the meridian. | Moderate | All mounts |
| **Elastic Wind-up** | Harmonic drive flexspline absorbs a correction pulse as spring energy, releasing it over a short time constant. In practice this calibrates to a negligible fraction on every rig tested so far — see §6.2.1 for the physical reasoning. | Weak in practice | Harmonic drive |
| **Mount Momentum** | Error tends to persist in the same direction for 2-5 frames due to mount inertia. | Weakly | All mounts |

### 2.2 Partially Predictable Errors

| Error Type | Description | Predictable? | Notes |
|---|---|---|---|
| **Stick-Slip Jumps** | Sudden position jump when static friction in harmonic drive gear releases. | Partially | Identifiable by SNR stability + error magnitude; timing is random |
| **Backlash** | Play in DEC gears; creates lag on direction reversal. | Partially | Amount is fixed per mount but reversal timing is controller-dependent |
| **Gravity-Dependent Tube Flexure** | Physical bending of the OTA tube, saddle plate, and focuser under their own weight. Changes continuously as the mount tracks. Manifests as a slow systematic DEC (and sometimes RA) drift whose rate and direction depend on current hour angle and DEC. | Moderate | Relevant for guide scope users (OAG eliminates this). Predictable if hour angle + DEC are features — `hour_angle_deg` is reserved in the feature set for this (§7.2) but not yet wired into a live guider. |
| **Differential Flexure (guide scope only)** | Relative flex between a separate guide scope and the imaging scope. The guide star moves independently of the imaging field. Manifests as systematic DEC drift that varies with pointing position. | Partially | Eliminated by using an OAG. With a guide scope, this is the dominant residual error source after worm PE correction. `guide_optics_oag` is reserved in the feature set to distinguish the two cases (§7.3) but is not yet consumed by a live guider. |
| **Bearing Wobble / Gear Eccentricity** | Non-circular worm wheel or bearing causing PE at multiple harmonics of the fundamental frequency (e.g., 2x and 3x the worm period). | Partially | `WormGearGuider`'s residual MLP is explicitly fed the PE phase's harmonics up to the 4th (§6.1); `HarmonicGuider` models up to two independent PE lines in its Kalman state (§6.2). |

### 2.3 Stochastic Noise (AI Cannot Help)

| Error Type | Description | Notes |
|---|---|---|
| **Atmospheric Seeing** | Turbulent air cells deflect the star position frame-to-frame. | Uncorrelated at guide cadence. White/pink noise. Truly unpredictable. Typical amateur-astrophotography guiding RMS is seeing-limited in the ~0.3″ (good seeing) to ~1.0-1.5″ (poor seeing) range — this is the practical floor no amount of mechanical modeling can improve on. |
| **Wind Shake** | Impulse noise from wind buffeting the telescope. | Random. No structure to learn. |
| **Centroiding Noise** | Statistical uncertainty in finding the guide star center from pixel data. | Irreducible at the SNR of the guide star. |

> **Key insight**: The practical RMS improvement from AI guiding depends almost entirely on the ratio of predictable systematic error to stochastic noise. A mount with strong PE in good seeing conditions will show large improvement. A mount with a small PE amplitude in poor seeing will show minimal or negative improvement (from over-correction into noise) — this is exactly what the confidence gating (§8.3) exists to detect and back away from.

---

## 3. Mount Classification System

### 3.1 The Three Mount Classes

```
WORM_GEAR       — Traditional equatorial mounts with worm-and-wheel drive (dominant error: worm PE)
HARMONIC_DRIVE  — Strain wave gear mounts (dominant error: PE, high-frequency on some rigs)
DIRECT_DRIVE    — Friction/torque drives with no gears (dominant error: refraction drift)
```

### 3.2 Mount Name Lookup Table

`kstars/data/mount_types.json` is a living name→class lookup table, matched case-insensitively against the mount's INDI device name:

```json
{
  "HARMONIC_DRIVE": [
    "ZWO AM5", "ZWO AM3",
    "iOptron HarmonicDrive 30", "iOptron HarmonicDrive 45", "iOptron HarmonicDrive 60",
    "Pegasus NYX-101", "Pegasus NYX-201",
    "Avalon Linear", "Avalon M-Uno", "Avalon M-Due",
    "Vixen AXJ", "Vixen AXD2",
    "Sky-Watcher EQM-35 Pro",
    "Rainbow Astro RST-135", "Rainbow Astro RST-300",
    "Planewave EQ L-350", "Planewave EQ L-500"
  ],
  "WORM_GEAR": [
    "Sky-Watcher HEQ5", "Sky-Watcher EQ6-R", "Sky-Watcher EQ8-R",
    "iOptron CEM25P", "iOptron CEM40", "iOptron CEM60", "iOptron GEM45",
    "Losmandy G11", "Losmandy Gemini",
    "Orion Atlas EQ-G", "Orion Sirius EQ-G",
    "Celestron CGX", "Celestron CGX-L", "Celestron CGEM II",
    "Meade LX200", "Meade LX600",
    "10Micron GM1000", "10Micron GM2000"
  ],
  "DIRECT_DRIVE": [
    "Astro-Physics 1100GTO", "Astro-Physics 1600GTO", "Astro-Physics 900GTO",
    "Sidereal Technology SiTech"
  ]
}
```

`MountGuiderFactory::detectMountType(mountName)` loads this file (via `KSPaths::locate`) and returns the matching class string, or `"NOT_FOUND"` if the mount isn't listed or the file can't be read.

### 3.3 Auto-Detection and User Override

`AIGuideProtocol::detectMountType()` calls the factory lookup above against the currently connected mount's device name (`m_Guide->mount()->getDeviceName()`). The Guide AI Assistant wizard calls this once, the first time it's shown, and pre-selects the "Detected Mount Type" combo box (`Worm Gear` / `Harmonic Drive` / `Direct Drive`) accordingly, logging which type it detected. If detection fails (unlisted mount, or no mount connected yet), the combo box is left at its default and the user picks manually — a log line says so either way. **The user can always change the combo box selection before starting the protocol**, which matters for:
- OEM mounts sold under different brand names not yet in the lookup table
- Mounts with hardware upgrades (e.g., a worm gear mount with a harmonic drive upgrade kit)
- A user who knows their mount's behavior doesn't match its nominal class

At inference time (loading a trained model, independent of the wizard), the mount class actually used comes from the `mount_type` field baked into the weights JSON itself (`MountGuiderFactory::readMountTypeFromWeights()`/`createFromWeights()`) — not from re-running detection — since a model is only valid for the class it was trained as.

---

## 4. Guide AI Assistant — System Identification Protocol

The Guide AI Assistant is a one-time setup wizard (`AIGuideWizard`/`AIGuideProtocol`) that collects high-quality, controlled training data by characterizing the mount's error signature through deliberate experiments. This is fundamentally different from passive guide log collection.

### 4.1 Why Controlled Sysid Data Is Better Than Passive Logs

Passive guide logs are generated with unknown aggressiveness, unknown minimum pulse cutoff, unknown guiding algorithm, and mixed seeing conditions. The AI has to untangle all of these confounds.

The Guide AI Assistant runs experiments where:
- Known pulses are sent and their responses measured precisely
- Free-drift periods (no corrections) directly reveal the raw mount motion
- Multiple sky positions characterize altitude-dependent effects
- All experimental conditions are recorded exactly

### 4.2 Protocol State Machine

`AIGuideProtocol` drives the wizard through an explicit state machine, not a single linear script:

```cpp
enum ProtocolState {
    STATE_IDLE, STATE_PRECHECK, STATE_HORIZON_SCAN, STATE_SLEWING, STATE_SETTLING,
    STATE_CAPTURING_DATA, STATE_DRIFT_RECENTER, STATE_PULSE_RESPONSE_INIT,
    STATE_PULSE_SENDING, STATE_PULSE_RECORDING, STATE_PULSE_SETTLING,
    STATE_DONE, STATE_TRAINING, STATE_TRAINING_DONE, STATE_ERROR
};
```

`AIGuideProtocol::start(mountType)` builds a `QVector<ProtocolPhase>` for the selected mount class (each phase: target alt/az offset, duration, whether it's free-drift, and — for pulse-response phases — axis/direction/magnitude/response-frame-count), then walks the list generically: horizon-scan → slew → settle → capture/pulse-respond, for every phase. Consecutive phases at the same sky position short-circuit the slew to a quick settle rather than re-slewing.

### 4.3 PID Auto-Tune: The Common First Phase (All Mount Types)

Before any system-identification data is collected, the protocol runs **PID Auto-Tune**: 3 repetitions of 8 pulse-response probes (RA EAST/WEST and DEC NORTH/SOUTH, each at 500ms and 1000ms), 24 pulses total, fired at Position 1's sky location. `AIGuideProtocol::applyPIDAutoTuneGainLock()` fits a per-axis FOPDT (First-Order-Plus-Dead-Time) step-response model from the paired-direction pulses and locks a base RA/DEC proportional+integral gain via `Options::setRA/DECProportionalGain()` — a live C++ port of `offline_trainer/pid_autotune.py`'s SIMC/IMC-style calculation (using a plateau-average instead of the offline trainer's full nonlinear curve fit).

This runs first, before any other data collection, for two reasons:
1. **Fingerprint validity**: the base gain is baked into the model fingerprint (§9.3). Changing it after sysid data is already collected under a different gain would invalidate that data.
2. **Data quality**: the long standard-guiding phases that follow reconstruct PE and drift from the *residual* error after correction — that reconstruction assumes a well-behaved, non-oscillating controller. Collecting it under a badly-tuned gain (or whatever the user happened to leave set, possibly from a different optical train) would corrupt everything downstream.

Enabled by default (`Options::aIPIDAutoTune()`), adds roughly 10 minutes to the protocol. See §12.9 for why running a classical controller tune-up before a learned/predictive layer is standard practice, not specific to this project.

### 4.4 Mount-Class-Specific Protocol (After PID Auto-Tune)

#### WORM_GEAR

The worm PE has a well-defined period (typically 300-600s for consumer EQ mounts). At least two full cycles are needed to robustly estimate amplitude and phase.

```
Position 1: Altitude ~65°, azimuth offset -45°
  - Standard guiding: 480s
  - Free drift: 600s (captures at least one full worm cycle for T<=450s)

Position 2: Altitude ~40°, azimuth offset -45°
  - Standard guiding: 480s
  - Free drift: 400s (captures altitude-dependent refraction rate)

Position 3: Altitude ~65°, azimuth offset +45°
  - Standard guiding: 480s
  - Free drift: 400s (parallactic-angle spread for the DEC refraction fit, opposite sign from Position 1/2)
```

**What this determines**: Worm period T (via FFT/autocorrelation of raw RA error), PE amplitude and phase offset, DEC polar drift rate, RA refraction drift rate as a function of altitude, pier-side asymmetry in PE phase.

#### HARMONIC_DRIVE

```
Position 1: Altitude ~65°
  - Free drift: 480s
  - Standard guiding: 1800s (30 min — long enough to resolve PE fundamentals in the
    288-865s range seen on tested rigs; several full cycles are needed for a reliable
    FFT/Lomb-Scargle fit, not just the ~2 cycles a shorter window would give)

Position 2: Altitude ~45°, azimuth offset +45° (east of the meridian)
  - Free drift: 120s
  - Standard guiding: 300s (parallactic-angle contrast for the DEC refraction fit)
```

The elastic-windup/spring characterization piggybacks on the same 24-pulse PID Auto-Tune batch from §4.3 rather than a separate dedicated pulse-response phase — see §6.2.1 for why that fit calibrates to a negligible spring fraction on every rig tested so far, and what a dedicated follow-up experiment would look like if this is ever revisited.

**What this determines**: PE period(s) and amplitude(s) (up to two independent lines, §6.2), spring absorption fraction and release time constant (currently negligible in practice, §6.2.1), mechanical drift rate as a function of altitude.

#### DIRECT_DRIVE (anything not classified as Worm Gear or Harmonic Drive)

Direct drive mounts have negligible PE. The protocol only needs to characterize refraction drift and any residual slow-moving systematic error.

```
Position 1: Altitude ~70°, azimuth ~0°
  - Standard guiding: 120s
  - Free drift: 180s

Position 2: Altitude ~50°, azimuth offset -60°
  - Standard guiding: 120s
  - Free drift: 180s

Position 3: Altitude ~35°, azimuth offset +60°
  - Standard guiding: 120s
  - Free drift: 180s
```

> **Why positions must span different azimuths, not just altitudes**: The DEC refraction model requires `k_ref_dec * sin(q) / cos²(alt)`. Fitting `k_ref_dec` requires variation in `sin(q)` across the dataset. Positions at different azimuths have different-sign parallactic angles, providing the contrast needed to separate the refraction DEC component from the constant polar drift `d_polar`. If all three positions were near the meridian, `sin(q) ≈ 0` everywhere and `k_ref_dec` couldn't be fitted — the trainer falls back to `k_ref_dec = k_ref` and warns the user in that case.

**What this determines**: RA drift rate as a function of altitude (`k_ref`), DEC polar drift rate (`d_polar`), DEC refraction coefficient (`k_ref_dec`, from variation in parallactic angle across positions).

### 4.5 Guide Exposure Recommendation

The assistant **does not change the guide exposure** — the exposure set in Guide Options at the time the wizard runs is what gets baked into the model fingerprint and must be used for every subsequent session. Step 1 of the wizard shows a fixed checklist:

| Mount Class | Recommended guide exposure | Why |
|---|---|---|
| **WORM_GEAR** | 2.0 s (default) | Worm PE periods are 300-600 s — easily observable at 2 s cadence. Longer exposure = better SNR per frame. |
| **HARMONIC_DRIVE** | 0.5 to 1.0 s (depends on guide star SNR) | Some harmonic-drive PE components run faster than a worm's; shorter exposure resolves them without aliasing. Requires a bright enough guide star at the shorter exposure — fall back to 1.0s if SNR is marginal. |
| **DIRECT_DRIVE** | 1.0 to 3.0 s | No PE to resolve. Exposure only affects SNR. |

The wizard's checklist is a fixed informational panel (not a per-mount dynamic green/amber indicator) — it's shown regardless of which mount type is selected, alongside a reminder that the exposure/aggressiveness/pulse settings active when the assistant runs are locked into the fingerprint. An Artificial Horizon (Settings → Artificial Horizon) should be defined before running, since the protocol slews by Alt/Az and uses it to pick safe targets.

### 4.6 Wizard Steps

- Step 1: Mount type (auto-detected, user can override) + exposure recommendation checklist
- Step 2: Protocol preview (estimated duration, sky positions to slew to)
- Step 3: Execution (progress bar, live log of protocol phases)
- Step 4: Export (local training, or forward to EkosLive — §5.4)
- Step 5: Result (weights saved to equipment profile, ready for use)

The assistant is safely abortable at any step. If aborted mid-sequence, partial data is still usable for training (just with less coverage) — `validate_sysid.py` (§4 of `EVALUATION.md`) is the tool for checking whether a partial or completed run has usable data before committing to training on it.

---

## 5. Training Pipeline

### 5.1 Two Training Paths

```
                 +----------------------------------+
                 |   Guide AI Assistant             |
                 |   (sysid data collection)        |
                 +---------------+------------------+
                                 |  sysid_data.json
                    +------------+-----------+
                    |                        |
                    v                        v
        +-------------------+   +----------------------+
        |  LOCAL TRAINER    |   |  EKOSLIVE (optional)  |
        |  (Python + CPU)   |   |  cloud training        |
        |  offlinetrainer/  |   |  request/response      |
        +--------+----------+   +-----------+----------+
                 |                          |
                 +----------+---------------+
                            |  weights.json
                  +---------v----------+
                  |  Equipment Profile |
                  |  (KStars)          |
                  +---------+----------+
                            |  loaded at session start
                  +---------v----------+
                  | MountSpecificGuider|
                  |  (C++ inference)   |
                  +--------------------+
```

### 5.2 Local Offline Trainer

A Python package at `kstars/ekos/guide/offlinetrainer/` that users (or developers) run directly. CPU-only; no GPU required for the small models used here.

```bash
cd kstars/ekos/guide/offlinetrainer
python3 -m venv venv && source venv/bin/activate && pip install numpy scipy torch

python3 train.py --sysid-data ~/.local/share/kstars/ai_training_logs/sysid_data_<TIMESTAMP>.json \
                  --output ~/weights_<TIMESTAMP>.json --verbose
```

`train.py` dispatches to `train_worm_gear.py`, `train_harmonic.py`, or `train_direct_drive.py` based on the `mount_type` recorded in the sysid data. This step is advisory in the sense that nothing in KStars auto-applies its output back into live settings — the base RA/DEC proportional/integral gain is *already* set live by the PID Auto-Tune gain-lock during the wizard run (§4.3); the offline trainer's own `recommended_*` fields are a second, fuller (non-simplified) calculation over the same data, useful as a cross-check.

### 5.3 Sysid Data Format

All data collected by the Guide AI Assistant is stored in a structured JSON file before training:

```json
{
  "format_version": "1.0",
  "equipment": {
    "mount_name": "Sky-Watcher EQ6-R",
    "mount_type": "WORM_GEAR",
    "camera": "ZWO ASI290MM Mini",
    "focal_length_mm": 400,
    "pixel_size_um": 2.9,
    "pixel_scale_arcsec_per_px": 1.496,
    "guide_exposure_ms": 2000,
    "guide_optics_type": "GUIDE_SCOPE",
    "kstars_version": "3.8.0"
  },
  "sessions": [
    {
      "session_id": "20260613_pos1_west",
      "type": "free_drift",
      "altitude_deg": 64.5,
      "azimuth_deg": 192.0,
      "pier_side": "WEST",
      "aggressiveness_ra": 0.75,
      "aggressiveness_dec": 0.75,
      "min_pulse_ra_ms": 0.0,
      "min_pulse_dec_ms": 0.0,
      "frames": [
        {
          "t": 0.0,
          "ra_raw_px": 0.021,
          "dec_raw_px": -0.003,
          "snr": 42.1,
          "dt": 2.001,
          "ra_pulse_ms": 0.0,
          "dec_pulse_ms": 0.0
        }
      ]
    },
    {
      "session_id": "20260613_pos1_pulse_response_ra",
      "type": "pulse_response",
      "altitude_deg": 64.5,
      "pier_side": "WEST",
      "pulse_axis": "RA",
      "pulse_direction": "EAST",
      "pulse_magnitude_ms": 500.0,
      "baseline_frames": [
        { "t": -2.0, "ra_raw_px": 0.015, "snr": 41.8 }
      ],
      "response_frames": [
        { "t": 0.0, "ra_raw_px": 0.021, "snr": 42.1 },
        { "t": 2.001, "ra_raw_px": -0.187, "snr": 42.3 }
      ]
    }
  ]
}
```

Key properties of this format:
- `aggressiveness_ra/dec` and `min_pulse_*_ms` are always recorded, removing a training ambiguity that undermined the project's earlier GRU prototype (an aggressiveness-agnostic model can't tell how much of the residual it sees was already suppressed by the controller).
- `pulse_response` sessions include a dedicated `baseline_frames` block (a short pre-pulse reference window) in addition to `response_frames` — this is what `pulse_response_fit.py` uses as the true zero-reference for the step-response fit, rather than assuming the first response frame is the baseline.
- Frame timestamps are absolute (not relative), enabling cross-session alignment.

### 5.4 Frame Quality Policy During Sysid Collection

Not every frame captured during data collection is suitable for training. The sysid JSON format includes an `error_code` field per frame, and the offline trainer applies the same frame quality policy as `evaluate_shadow.py` (see `EVALUATION.md` §4).

A frame is **excluded from training** if any of the following are true:

| Condition | Threshold | Reason |
|---|---|---|
| `error_code ≠ 0` | Any non-zero | Star lost or centroiding failed. The position measurement is unreliable. |
| `snr < 10` | — | Low SNR: centroid noise dominates. The position is not a reliable measurement of mount motion. |
| `dt > 3 × median_dt` | — | Dropped frame: the mount drifted for an unusually long time. The next-frame error is not comparable to normal frames. |
| Frame is during a guide abort/restart | Gap > 30s | The first frames after a restart are during re-convergence, not steady-state. |
| Error spike | `|error| > 5 × session_rms` | Large spikes are likely seeing events or stick-slip jumps, not the systematic drift the model is trying to learn. |

#### Minimum Data Requirements for Training

| Mount Type | Minimum valid frames | Minimum PE cycles (WormGear) | Action if insufficient |
|---|---|---|---|
| WORM_GEAR | 500 | 2 complete cycles | Warn and refuse to train — PE period cannot be reliably estimated |
| HARMONIC_DRIVE | 200 | N/A | Warn if pulse response test has < 3 pulses per direction |
| DIRECT_DRIVE | 100 | N/A | Warn if altitude range < 20° (refraction model needs altitude variation) |

`validate_sysid.py` (in the same directory) runs this same quality/sufficiency check as a fast QC pass over a still-growing or finished sysid file, without waiting for the full trainer — see the project's test-plan documentation for day-to-day usage.

### 5.5 EkosLive Cloud Training (Optional)

Training can optionally be forwarded to EkosLive instead of run locally: `AIGuideWizard::trainInEkosLiveRequested` carries the collected sysid data to `Guide::newTrainingData`, and the resulting weights (or an error) arrive back on the same `trainingComplete()`/`trainingError()` signal path as local training. This is a thin, optional integration point — the local Python trainer is the source of truth and works fully offline; EkosLive is a convenience for users who'd rather not set up a local Python environment.

---

## 6. Per-Mount-Type Model Architectures

### 6.1 WormGearGuider — Physics Layer + Residual MLP

**Philosophy**: The worm PE is a deterministic sinusoid with known physical constraints (period set by worm gear ratio, amplitude a few arcseconds). Encode this as a physics layer with an online phase tracker. Use a small MLP only for the unexplained residual — non-sinusoidal PE waveform shape, gear eccentricity harmonics, backlash signature.

#### Physics Layer

```
PE_ra(t)          = A * sin(2*pi*t/T + phi)
drift_ra(alt)     = k_ref / cos²(alt)
drift_dec(alt, q) = d_polar + k_ref_dec * sin(q) / cos²(alt)
```

where `q` is the parallactic angle (the angle between the zenith and the north celestial pole as seen from the target; computed from altitude, azimuth, and site latitude).

`A` (PE amplitude) and `T` (PE period) are fixed at training time from an FFT fit over the sysid data. The phase `phi` is tracked online, every session, by a **4-state Recursive Least Squares (RLS) filter** (state `[sin_coeff, cos_coeff, v, C]`, forgetting factor λ=0.999) that estimates the sinusoid's in-phase/quadrature amplitude, plus a linear drift and constant term, from the accumulated uncorrected position each frame. This handles PE phase drift between sessions without retraining.

#### Residual MLP

A **15 → 32 → 16 → 2** network. Inputs: `[alt_norm, sin(φ), cos(φ), sin(2φ), cos(2φ), sin(3φ), cos(3φ), sin(4φ), cos(4φ), snr_norm, ra_pulse_norm, dec_pulse_norm, dt_norm, q_norm, pier_side_norm]` — i.e. it's fed the PE phase's harmonics up to the 4th explicitly, so it can pick up non-sinusoidal PE waveform shape and gear-eccentricity harmonics (§2.2) rather than being a blind function of raw features. Output: `[ra_residual, dec_residual]`, added to the physics layer's prediction.

`warmupFrames() = 50`.

#### Limitations

- Assumes the physics layer's fundamental-frequency model is a good starting point; heavy reliance on the residual MLP for harmonic content means very distorted PE waveforms get less benefit from the physically-motivated part of the model.
- The period `T` must be observable in the sysid data — a session containing only one PE cycle for a long-period worm gives a noisy FFT estimate.
- Balance changes or OTA swaps within a session change `A` gradually; the model does not adapt this online (only phase `phi` adapts online).

---

### 6.2 HarmonicGuider — Kalman Filter with Spring and PE States

**Philosophy**: The harmonic drive's elastic spring creates a hidden state (spring tension) that affects how correction pulses translate to actual mount motion, and PE on these mounts is folded directly into the same estimator rather than handled by a separate physics layer. A Kalman filter is the natural estimator for this: linear dynamics, a small number of interpretable states, and a principled way to blend model prediction against noisy measurement.

#### Kalman State Vector (14 states)

```
x = [ra_error,     ra_velocity,   spring_ra,    pe_sin_ra,  pe_cos_ra,
     dec_error,    dec_velocity,  spring_dec,   pe_sin_dec, pe_cos_dec,
     pe2_sin_ra,   pe2_cos_ra,    pe2_sin_dec,  pe2_cos_dec]
```

Ten base states (error, velocity, and spring tension per axis, plus a primary PE oscillator per axis as an in-phase/quadrature pair) plus four states for a **secondary** PE oscillator (one in-phase/quadrature pair per axis). The offline trainer (`train_harmonic.py`) detects up to four candidate PE lines via FFT/Lomb-Scargle and records them all in the weights JSON (`pe_lines`, primary first); the runtime model loads the primary line plus the strongest secondary line that's well-separated in period from the primary, and tracks those two as rotating states. This covers the two-line case well; a third or fourth line, if one exists and carries meaningful energy on a given rig, is not separately tracked — it shows up as unmodeled residual/process noise rather than a distinct predicted term.

#### State Transition

```
ra_error(t+1)    = ra_error(t) + ra_velocity(t)*dt
                   - effective_ra_pulse(t)
                   + mechanical_drift_ra(alt)*dt

ra_velocity(t+1) = ra_velocity(t)             # near-constant over short windows

spring_ra(t+1)   = (1 - dt/tau_ra) * spring_ra(t)   # spring releases exponentially
                   + kappa_ra * raw_ra_pulse(t)       # spring absorbs fraction of pulse

effective_ra_pulse = raw_ra_pulse(t) - spring_ra(t)  # only non-absorbed part moves mount

pe_sin_ra, pe_cos_ra rotate at the fitted PE angular frequency each step
(DEC and the secondary PE oscillator follow the same pattern)
```

#### Parameters

| Parameter | Symbol | How learned |
|---|---|---|
| Spring absorption fraction | kappa_ra, kappa_dec | Fit from paired-direction pulse-response data (`pulse_response_fit.py`): fraction of pulse that does NOT immediately appear in position. Calibrates to **0.0** on every rig tested so far — see §6.2.1. |
| Spring release time constant | tau_ra, tau_dec | Fit from the decay curve of position after a pulse; defaults to **1.5s** when kappa is 0 (the time constant is then moot). |
| PE period(s) and amplitude(s) | pe_period, pe_amplitude, pe2_period, pe2_amplitude | FFT/Lomb-Scargle over free-drift and standard-guiding sysid sessions |
| Mechanical drift (RA, DEC) | v_ra(alt), v_dec | Fit from free-drift sessions at multiple altitudes |

These are directly interpretable physical parameters, readable and sanity-checkable by a developer inspecting the weights JSON — no opaque weight matrix for the physical part of the model.

#### The Neural Component: Dynamic Process Noise Q

A small **5 → 8 → 2** MLP predicts the process noise covariance Q dynamically each frame:

```
Input to Q-net: [snr_norm, snr_delta_norm, |Δra_raw_px|, |Δdec_raw_px|, dt_norm]
  — the frame-to-frame tracking-error delta, a computationally simple proxy for
    "something surprising just happened" (a stick-slip jump or a seeing spike both
    show up as a large frame-to-frame jump), rather than the full Kalman innovation.

Output: [log_Q_ra, log_Q_dec]  (exponentiated, clamped to [1e-4, 100])
```

When the recent tracking error has moved a lot, Q increases (trust the measurement more, trust the model less); when it's been quiet, Q stays small (trust the model). This is the mechanism that answers "was this a mechanical jump or seeing spike?" without needing a separate classifier.

#### `predict()` and Confidence

`predict()` captures the filter's posterior state, runs the pure time-update step (`kalmanPredict()` — pulses are applied separately, in `update()`), and returns the change in position implied by that propagation as the feed-forward correction (in arcsec, via the frame's pixel scale), split for diagnostics into a `physics` term (the velocity/trend component) and an `mlp` term (spring + PE, the "learned" part) even though both come from the same Kalman machinery rather than a literal separate neural network — the split is for the debug log (§10), not a second model.

Confidence follows a rolling ~20-sample innovation RMS per axis against an EMA-tracked "typical RMS" baseline, an SNR factor (`clamp((snr-10)/20, 0, 1)`), a warmup ramp (`0.3 + 0.7 * min(1, (frameCount - warmupFrames) / 20)`), and a Lorentzian prediction-quality term `1 / (1 + error_ratio²)` where `error_ratio = innovation_rms / typical_rms`. Final: `confidence = clamp(warmup_factor * snr_factor * prediction_quality, 0, 1)`. The Lorentzian's fatter tails (vs. an exponential decay) degrade confidence more gracefully for moderately-bad predictions and more sharply only for genuinely bad ones. `warmupFrames() = 30`.

#### Limitations

- The linear Kalman filter assumes Gaussian noise and linear dynamics. Stick-slip jumps are neither; the Q-net compensates partially but large stick-slip events still cause a frame or two of recovery time.
- Only the top two PE lines are tracked (above); a rig with significant energy in a third harmonic gets that energy folded into unmodeled noise rather than corrected.
- kappa and tau may in principle change with temperature (thermal expansion of the flexspline); since kappa calibrates to zero in practice (§6.2.1) this is currently moot, but would matter if a future rig or experiment ever resolves a non-trivial kappa.

---

### 6.2.1 Elastic Wind-Up: Expected Magnitude and Why kappa Calibrates to Zero

The spring/wind-up mechanism in §6.2 is real code, exercised every frame — but on every rig tested so far, the pulse-response fit (`pulse_response_fit.py`'s `fit_pulse_response()`) returns `kappa = 0.0`, i.e. no detectable spring absorption, and the trainer uses that as the default rather than a fitted value. This is expected, physically-grounded behavior, not a broken feature:

**What "elastic windup" is, and how big it is when deliberately provoked.** Torsional windup in a strain-wave (harmonic drive) gearset is well characterized in mechanism/robotics engineering: manufacturer specifications define **"lost motion"** as the hysteresis measured by applying **±4% of the gearset's rated torque** to the output while the input is locked, and quote standard harmonic-drive gearheads at **< 1 arc-minute (60 arcsec)** of lost motion under that test — a large, deliberately-provoked, full-reversal torque swing, the worst case the spec is designed to bound.

**What a guide correction pulse actually does, by comparison.** A guide pulse nudges the mount's commanded rate for a few hundred milliseconds — nowhere near a ±4%-of-rated-torque full load reversal. On the RA axis specifically, the motor is already continuously loaded by sidereal tracking; a guide pulse modulates the drive rate slightly faster or slower for a moment, it does not reverse the direction of load on the gearset the way the manufacturer's lost-motion test does. DEC is different: DEC has no continuous baseline load, and a DEC guide pulse genuinely can reverse direction (North vs. South) — closer in kind, if not in scale, to the mechanism the lost-motion spec measures. The current pulse-response protocol and fit don't distinguish this asymmetry; both axes are fit with the same model and acceptance gates.

**Comparing the plausible windup amplitude against the actual noise floor.** If windup scales roughly with the size of the load perturbation (reasonable away from the reversal deadband, since strain-wave torsional compliance is quasi-linear outside the hysteresis loop's corners), a guide-pulse-scale perturbation — orders of magnitude smaller than the ±4%-rated-torque reversal the 60-arcsec spec is measured at — plausibly produces windup on the order of a small fraction of an arcsecond, not tens of arcseconds. Guiding RMS in typical amateur astrophotography is seeing-limited at roughly 0.3″ (good seeing) to 1.0-1.5″ (poor seeing) — a windup signal a fraction of an arcsecond in size, riding on top of a ~1″ noise floor, is exactly what the pulse-response fit's own acceptance gate (`|P| < 2×residual_std → treat as noise, use defaults`) is designed to reject. Independent corroboration: manufacturer specs for harmonic-drive astrophotography mounts (e.g. ZWO AM5) quote zero backlash and single-digit-arcsecond periodic error, with typical guided RMS well under an arcsecond — the same order of magnitude this analysis predicts for windup, not the tens-of-arcseconds regime the reversal-hysteresis spec describes.

**Is it worth correcting for, if it could somehow be resolved?** Not as a priority. Even in an optimistic case where a future experiment resolved a small non-zero kappa, the achievable RMS contribution would be small relative to (a) the PE component, 1-2 orders of magnitude larger in raw amplitude and already the model's main target via the two-oscillator Kalman state, and (b) the seeing noise floor, which no mechanical modeling can reduce. The acceptance criteria this feature is held to (§11.3: **+8% total RMS for HARMONIC_DRIVE**) is realistically won or lost on PE and drift/refraction correction, not windup — which is why `kappa=0.0, tau=1.5` is the shipped default rather than something actively fit per session.

**What would be worth trying, if this is ever revisited**: a dedicated, one-off diagnostic using a much larger, deliberately-reversing DEC pulse pair (closer to the manufacturer's ±4%-rated-torque reversal test than to a real guide correction), run once as a side experiment purely to check whether the mechanism is detectable *at all* on a given rig — not folded into the normal PID Auto-Tune/sysid protocol. If that also comes back at the noise floor, that's a strong signal the mechanism simply isn't relevant at guide-pulse scale on that rig.

---

### 6.3 DirectDriveGuider — Parametric Refraction Model

**Philosophy**: Direct drive mounts have no gear-induced PE. The dominant systematic error is atmospheric refraction (both RA and DEC components) and polar alignment drift. A purely analytic model is sufficient. No neural component.

#### Model

```
drift_ra(alt)      = k_ref / cos(alt)^2 + d_ra_extra          [RA refraction + residual RA drift]
drift_dec(alt, q)  = d_polar + k_ref_dec * sin(q) / cos(alt)^2 [polar misalignment + DEC refraction]

predicted_next_ra_px  = current_ra_px  + drift_ra(alt)       * dt - applied_ra_pulse_px
predicted_next_dec_px = current_dec_px + drift_dec(alt, q)   * dt - applied_dec_pulse_px
```

where `q` is the parallactic angle, computed each frame from altitude, azimuth, and the observer's latitude:

```
sin(q) = sin(azimuth) * cos(latitude) / cos(dec_target)
```

The parallactic angle is zero on the meridian and changes sign as the object crosses it, so the DEC refraction component reverses direction at meridian crossing — a constant `d_dec` would be systematically wrong for half of any session that spans the meridian.

#### Parameters (4 used at inference, fit by analytical least-squares from free-drift sysid data)

- `k_ref` — RA refraction coefficient, fitted per optical train
- `k_ref_dec` — DEC refraction coefficient; physically the same constant as `k_ref`, fitted independently to absorb any optical-train asymmetry (falls back to `k_ref_dec = k_ref` if the sysid data's parallactic-angle range is < 20°, per §4.4's DIRECT_DRIVE azimuth-spread requirement)
- `d_polar` — DEC polar alignment drift rate (constant component)
- `d_ra_extra` — any residual RA drift not explained by refraction (e.g. RA motor rate error)

A fifth quantity, `phi_drift` (the polar-misalignment-vector angle), is stored in the weights JSON for diagnostic display but is not read by the inference formulas above.

`confidence()` is a fixed **0.95** once weights are loaded — there's no online prediction-error signal to adapt to for a purely analytical model. `warmupFrames() = 10` (much shorter than the other two guiders' 30-50, since there's no phase or Kalman state that needs frames to converge). `update()` is a no-op: there are no online-learnable parameters.

#### Limitations

- Only useful for true direct-drive mounts with negligible PE. If misclassified, the model under-corrects any real PE present.
- Does not model thermal effects on the mount structure.
- For guide scope users, differential flexure adds a position-dependent DEC drift on top of the refraction model that this analytic form doesn't separately capture (see §7's note on `guide_optics_oag`).

---

## 7. Feature Set Specification

### 7.1 Features Populated Every Frame

`GuideFrameData` (`mount_guider.h`) is built once per guide frame in `gmath.cpp` and passed to whichever guider is active. The following fields are computed and populated every frame, for every mount type:

| Feature | Description | Units |
|---|---|---|
| `ra_raw_px` / `dec_raw_px` | RA/DEC tracking error from the guide star centroid | pixels |
| `ra_pulse_ms` / `dec_pulse_ms` | Last correction pulse magnitude, signed | ms |
| `ra_ms_per_arcsec` / `dec_ms_per_arcsec` | Calibration conversion factor | ms/arcsec |
| `snr` | Guide star signal-to-noise ratio | dimensionless |
| `dt` | Time since last guide frame | seconds |
| `pixel_scale` | Optical train pixel scale | arcsec/pixel |
| `altitude_deg` / `azimuth_deg` | Current telescope pointing | degrees |
| `parallactic_angle_deg` | Computed from altitude/azimuth/site latitude each frame; zero on the meridian, changes sign at meridian crossing | degrees |
| `pier_side_east` | Current pier side | bool |
| `t_session_sec` | Time since guiding started this session | seconds |

### 7.2 Reserved Features (Not Yet Populated)

Four fields exist in `GuideFrameData` for error types the design anticipates but no shipped guider currently consumes:

| Feature | Intended purpose | Status |
|---|---|---|
| `hour_angle_deg` | Together with `altitude_deg`, fully describes the gravity direction on the OTA — needed to model tube flexure and (with `guide_optics_oag`) differential guide-scope flexure as they change across a session (§2.2) | Declared in the struct; not yet computed in `gmath.cpp` |
| `guide_optics_oag` | Whether the guide star shares the imaging optical path (OAG, no differential flexure) or comes from a separate guide scope (differential flexure possible) — available from the Ekos optical train configuration | Declared in the struct; not yet computed in `gmath.cpp` |
| `snr_delta` | Change in SNR from the previous frame — a seeing-change indicator | Declared in the struct; each guider that wants this currently computes its own internal delta from consecutive `frame.snr` values rather than reading a pre-computed field |
| `ra_pulse_suppressed` / `dec_pulse_suppressed` | Whether the last pulse was suppressed by the minimum-pulse threshold | Declared in the struct; not yet computed in `gmath.cpp` |

Wiring these up is a well-scoped, self-contained piece of future work if tube flexure or differential guide-scope flexure turns out to matter enough on a given rig to be worth the residual MLP/Kalman capacity: hour angle is derivable from mount RA + local sidereal time (both already available elsewhere in Ekos), and OAG-vs-guide-scope is already known from the optical train configuration.

### 7.3 Features Explicitly Not Used

| Feature | Reason Excluded |
|---|---|
| `wind_speed`, `wind_gust` | Not available from standard guide logs; weather station data isn't present in most setups |
| `temperature` | Would help with seasonal variation; a reasonable future addition |
| `star_mass` | Correlated with SNR; adds noise without independent information |
| `dx`, `dy` (raw pixel offsets) | Redundant with `ra_raw_px`/`dec_raw_px` after calibration rotation |

---

## 8. C++ Runtime Architecture

All of this lives under `kstars/ekos/guide/internalguide/`.

### 8.1 Class Hierarchy

```cpp
// -- Abstract base --------------------------------------------------------
class MountSpecificGuider {
public:
    virtual ~MountSpecificGuider() = default;

    virtual bool loadWeights(const QString& weightsPath) = 0;
    virtual void resetSession(bool forceReset) = 0;

    // Per-frame inference. Returns predicted feed-forward correction in
    // arcseconds. valid=false during warmup.
    virtual GuideOutput predict(const GuideFrameData& frame) = 0;

    // Prediction with no fresh guide frame (dark guiding).
    virtual GuideOutput darkPredict(double dt_sec) = 0;

    // Called after each frame with the observed residual, enabling online
    // adaptation (RLS phase tracking, Kalman update, or a no-op for
    // DirectDriveGuider).
    virtual void update(double ra_error_px, double dec_error_px,
                        double uncorrected_drift_ra_px, double uncorrected_drift_dec_px,
                        double snr, double ra_pulse_px, double dec_pulse_px) = 0;

    virtual double confidence() const = 0;
    virtual int warmupFrames() const = 0;
    virtual QString stateString() const = 0;
    virtual bool isLoaded() const = 0;
    QString fingerprintError() const;   // concrete accessor, backed by m_FingerprintError
};

// -- Concrete implementations, ~200 / ~90 / 4 parameters respectively ------
class WormGearGuider    : public MountSpecificGuider { /* physics layer + residual MLP */ };
class HarmonicGuider    : public MountSpecificGuider { /* 14-state Kalman filter + Q-net */ };
class DirectDriveGuider : public MountSpecificGuider { /* analytic refraction model    */ };

// -- Factory --------------------------------------------------------------
class MountGuiderFactory {
public:
    static QString detectMountType(const QString& mountName);          // §3.2
    static QString readMountTypeFromWeights(const QString& weightsPath);
    static std::unique_ptr<MountSpecificGuider> create(const QString& mountType);
    static std::unique_ptr<MountSpecificGuider> createFromWeights(const QString& weightsPath);
};

// -- Data types -----------------------------------------------------------
struct GuideFrameData {
    double ra_raw_px, dec_raw_px;
    double ra_pulse_ms, dec_pulse_ms;
    double snr, snr_delta;
    double dt, pixel_scale;
    double ra_ms_per_arcsec, dec_ms_per_arcsec;
    double altitude_deg, hour_angle_deg, azimuth_deg, parallactic_angle_deg;
    bool   guide_optics_oag;
    bool   pier_side_east;
    double t_session_sec;
    bool   ra_pulse_suppressed, dec_pulse_suppressed;
};

struct GuideOutput {
    bool    valid;              // false during warmup
    double  ra_correction_arcsec, dec_correction_arcsec;
    double  physics_ra_arcsec, physics_dec_arcsec;  // trend/physics component, for debug
    double  mlp_ra_arcsec, mlp_dec_arcsec;          // learned/residual component, for debug
    double  confidence;
    QString debug_log;
};
```

### 8.2 Confidence-Gated Control Blending

The blend happens in **`cgmath::processAxis()`**, `internalguide/gmath.cpp`, called per-axis from `calculatePulses()` (itself called from `performProcessing()`). For the RA/DEC branch, AI algorithm selected, guiding normally (not dark-guiding):

```cpp
const double conf   = m_lastAIPrediction.confidence;
const double aiGain = Options::aIPredictionGain();

const double proportionalResponse = arcsecDrift * in_params.proportional_gain[k] * pulseConverter;
const double integralResponse     = drift_integral[k] * in_params.integral_gain[k] * pulseConverter;
const double aiResponse           = ai_pulse_arcsec * pulseConverter;

// Scale down proportional response when AI is confident (if enabled)
double activePropGain = 1.0;
if (Options::aIProportionalBackoff())
    activePropGain -= (aiGain * conf * 0.5); // Reduce P-gain by up to 50%

double total = (proportionalResponse * activePropGain) + integralResponse + (aiGain * conf * aiResponse);
```

The feed-forward term (`aiGain * conf * aiResponse`) is added on top of the classical proportional+integral response. The optional **`AIProportionalBackoff`** mechanism additionally shrinks the classical proportional term itself by up to 50%, in proportion to AI confidence, when enabled — once the AI is genuinely confident, letting the reactive P-controller and the predictive feed-forward both fight the same error at full strength risks double-correction/overshoot; backing off the reactive term as confidence rises hands authority to the predictor more deliberately than just adding its output on top. The integral term is always left untouched, so steady-state/DC error is still fought regardless of AI state.

Dark guiding (no fresh guide-camera frame that cycle) uses a simpler branch with no P/I blending:

```cpp
double total = aiGain * ai_out.confidence * aiResponse;
```

clamped to the max-pulse limit, and deliberately not checking the minimum-pulse threshold (matching GPG's existing dark-guiding behavior — dark-guiding pulses are intentionally small and shouldn't be filtered by the normal dead-band).

### 8.3 Confidence Computation

Confidence is computed per guider class, not by one shared function:

- `DirectDriveGuider::confidence()` is a fixed `0.95` (§6.3 — no online signal exists for a purely analytical model).
- `WormGearGuider` and `HarmonicGuider` both follow the same pattern: a rolling ~20-sample innovation RMS per axis, an EMA-tracked "typical RMS" baseline, `error_ratio = innovation_rms / typical_rms`, an SNR factor `clamp((snr-10)/20, 0, 1)`, a warmup ramp, and a Lorentzian prediction-quality term `1 / (1 + error_ratio²)`. Final: `confidence = clamp(warmup_factor * snr_factor * prediction_quality, 0, 1)` (§6.1, §6.2 give the exact warmup-ramp formula each guider uses).

---

## 9. Guide Settings Classification and Model Validity

The guide settings in `Options.h` / `kstars.kcfg` fall into three categories with respect to AI training and deployment. **Changing a critical setting without retraining is one of the most likely ways to silently degrade AI guiding performance.**

### 9.1 Settings Classification

#### Category A — Model-Invalidating (Must Be Logged + Locked)

These settings directly define the relationship between observed error, applied pulse, and residual drift that the model learns. Changing any of these after training means the model was trained on a fundamentally different system.

| Setting | Why It Invalidates the Model |
|---|---|
| `GuideExposure` | Frame rate sets the Nyquist frequency — what PE frequencies are observable. A model trained at 2s cadence is wrong for 1s cadence. Also changes the pixel drift per frame for all velocity terms. |
| `GuideBinning` | Binning changes the pixel scale (arcsec/px). Every feature in the input vector is at a different scale between 2x2 and 1x1. |
| `RAProportionalGain` / `DECProportionalGain` | Determines how large a correction pulse is issued for a given error. The residual drift the model learns is what escapes this gain — a different gain means a completely different residual pattern. This is a unit-less **0.0–1.0** control gain (`pulse_ms = arcsecDrift * gain * ms_per_arcsec`); `1.0` would attempt to fully correct the measured error in one pulse, and the UI (Guide Options) defaults to `0.75`. This is the "aggressiveness" confound that made the project's earlier GRU prototype hard to interpret. |
| `RAIntegralGain` / `DECIntegralGain` | If non-zero, the controller has memory that accumulates error over time. Default 0; any non-zero value invalidates a model trained at 0. |
| `RAMinimumPulseArcSec` / `DECMinimumPulseArcSec` | Sets the cutoff below which corrections are suppressed to zero. The model must see the same zero-suppression pattern it was trained on. |
| `RAMaximumPulseArcSec` / `DECMaximumPulseArcSec` | Hard clamp on pulse magnitude. Large corrections are capped differently at a different max. |
| `RAHysteresis` / `DECHysteresis` | If > 0, activates the hysteresis blending controller. Default 0; any non-zero value invalidates a model trained at 0. |
| `RAGuidePulseAlgorithm` / `DECGuidePulseAlgorithm` | The control algorithm active during sysid data collection determines what residual drift the model learns to predict (§9.2). |

#### Category B — Must Be Logged (Directional Axis Locks)

| Setting | Action Required |
|---|---|
| `RAGuideEnabled` / `DECGuideEnabled` | Must be `true` during sysid. Logged. |
| `NorthDECGuideEnabled` / `SouthDECGuideEnabled` | Logged; if `false` during training, the model never saw corrections in that direction. |
| `EastRAGuideEnabled` / `WestRAGuideEnabled` | The wizard enforces these enabled during sysid (§9.2). |

#### Category C — Log for Diagnostics Only (No Model Impact)

| Setting(s) | Reason Safe to Change |
|---|---|
| `GuideAlgorithm` (star detection: SEP, smart, etc.) | Affects centroiding accuracy, not the control law. `snr` already handles varying centroiding quality as a feature. |
| Dither settings | Dither frames are excluded from training by the error-code-aware preprocessor. |
| `GuideMaxDeltaRMS`, `GuideLostStarTimeout` | Safety abort thresholds; don't affect normal guiding behavior. |
| `GPGEnabled` and other `GPG*` settings | GPG is mutually exclusive with the AI guider at the algorithm-selection level. |
| `AIShadowMode` | Meta-setting for evaluation logging; doesn't affect model weights. |
| `GuideDelay`, `GuideSquareSize` | Timing/box-size, not the control law. |

### 9.2 Which Algorithm to Use During Sysid Data Collection

The Guide AI Assistant wizard **enforces** `Standard` (algorithm 0) for both RA and DEC axes during all sysid data collection, forcibly setting both before the first guide frame and restoring the original values when the wizard completes or is aborted.

| Algorithm | Why It Cannot Be Used for Sysid |
|---|---|
| **GPG** | Already predicts and removes RA PE. Training on GPG-corrected residual and deploying on Standard-corrected residual produces a model that actively fights the controller. |
| **Linear** | Maintains an internal slope estimate over recent frames — the pulse depends on error history, not just current error, making the pulse-to-residual relationship non-stationary and unlearnable from the logged features alone. |
| **Hysteresis** | Blends current error with the previous output (`output = gain × ((1-h)×error + h×last_output)`) — same history-dependence problem as Linear. |

There is no valid reason to collect sysid data with anything other than Standard — the AI is designed to augment the Standard proportional controller.

### 9.3 Model Validity Fingerprint

When a model is trained, a settings fingerprint is computed and stored alongside the weights JSON:

```json
{
  "model_fingerprint": {
    "guide_exposure_s": 2.0,
    "guide_binning": "2x2",
    "ra_proportional_gain": 0.75,
    "dec_proportional_gain": 0.75,
    "ra_integral_gain": 0.0,
    "dec_integral_gain": 0.0,
    "ra_min_pulse_arcsec": 0.2,
    "dec_min_pulse_arcsec": 0.2,
    "ra_max_pulse_arcsec": 25.0,
    "dec_max_pulse_arcsec": 25.0,
    "ra_hysteresis": 0.0,
    "dec_hysteresis": 0.0,
    "ra_pulse_algorithm": 0,
    "dec_pulse_algorithm": 0,
    "all_directions_enabled": true,
    "fingerprint_sha256": "a3f7c9..."
  }
}
```

At every session start, KStars computes the current settings fingerprint and compares it to the stored one. If it selecting an AI algorithm and the fingerprint doesn't match (or the weights file can't be loaded at all), guiding is refused outright with a notification, rather than silently falling back to the classical controller — a deliberate, conservative choice: a session where the user believes AI is active but it silently isn't is worse than one that refuses to start.

### 9.4 UI Locking Policy (Planned, Not Yet Implemented)

The intent is that when AI Guiding is enabled, Category A settings should be displayed as read-only in the Guide Options panel, with a tooltip explaining that the setting is locked while AI Guiding is active and must be changed via disabling AI Guiding, adjusting the setting, then retraining. This UI-level locking is not yet built — today, nothing prevents a user from changing a Category A setting while an AI algorithm is selected; the fingerprint check at session start (§9.3) is what actually catches the mismatch, just later (at guide start) rather than at the moment the setting is changed.

---

## 10. KStars Integration

### 10.1 Equipment Profile and Guide Options

- Mount type: auto-detected from the connected mount's device name via `mount_types.json` (§3.3), user-overridable in the wizard's Step 1.
- AI weights file: `Options::aIGuiderWeightsFile()`, set via the Guide Options "AI" tab or automatically after a successful wizard run.
- AI enable: there's no separate master "AI enabled" toggle — enabling AI **is** selecting the AI algorithm as `RAGuidePulseAlgorithm` and/or `DECGuidePulseAlgorithm` (the same per-axis combo box used to choose Standard/Hysteresis/Linear/GPG). Selecting AI on either axis with valid, fingerprint-matching weights loaded takes that axis through WARMUP into ACTIVE automatically.
- `AIPredictionGain` (`Options::aIPredictionGain()`, 0.0–1.0, UI default 0.5): the feed-forward gain multiplier in §8.2's blend formula.
- `AIProportionalBackoff` (bool): the optional P-gain-backoff mechanism, §8.2.
- `AIShadowMode` (bool): runs the AI model alongside standard guiding, logging predictions without applying them — see `EVALUATION.md` for the full shadow-mode evaluation workflow.
- `AIPIDAutoTune` (bool, default on): whether the wizard runs the PID Auto-Tune phase described in §4.3.

### 10.2 Guide State Machine

`AIGuideState` (`internalguide/gmath.h`) tracks five states:

```cpp
enum class AIGuideState { DISABLED, WARMUP, SHADOW, ACTIVE, FALLBACK };
```

- **DISABLED** — no weights loaded, or no AI algorithm selected and shadow mode off. Zero overhead.
- **WARMUP** — an AI algorithm is selected and weights are loaded; building history. Classic controller issues all pulses; guiding looks identical to non-AI guiding.
- **ACTIVE** — an AI algorithm is selected, warmup is past, and predictions are valid; the feed-forward blend (§8.2) is live.
- **FALLBACK** — was ACTIVE; confidence dropped below what's needed to trust the prediction. AI authority reduces toward zero; the classic controller carries the session. Re-engages automatically once confidence recovers — no user action needed.
- **SHADOW** — no AI algorithm selected on either axis, but `AIShadowMode` is on and weights are loaded. Full inference runs every frame and is logged, but never touches the dispatched pulse. This is the developer/QA evaluation path described fully in `EVALUATION.md`.

Transitions are handled in `cgmath::performProcessing()`: an invalid prediction moves an ACTIVE session to FALLBACK (or to WARMUP if it wasn't yet ACTIVE) unless the state is SHADOW, which never auto-promotes to ACTIVE on its own (SHADOW only ends when the user changes the algorithm selection).

### 10.3 AI Debug Log

Every guide frame, `cgmath` writes one row to a dedicated CSV — `<AppLocalDataLocation>/ai_debug_logs/ai_guider_<timestamp>.csv`, opened once per session — independent of the PHD2-format guide log (`guidelog.h`/`.cpp`, which remains exactly what it always was and carries no AI-specific fields). The debug CSV header:

```
t_session,dt,altitude_deg,azimuth_deg,parallactic_angle_deg,ra_error_arcsec,uncorrected_ra_delta_px,
dec_error_arcsec,uncorrected_dec_delta_px,conf,pred_ra_arcsec,physics_ra_arcsec,mlp_ra_arcsec,
pred_dec_arcsec,physics_dec_arcsec,mlp_dec_arcsec,ai_state,pe_statestring,
ra_algorithm,ra_prop_response_ms,ra_integral_response_ms,ra_ai_response_ms,ra_active_prop_gain,
ra_total_pulse_ms,ra_direction,ra_suppressed,
dec_algorithm,dec_prop_response_ms,dec_integral_response_ms,dec_ai_response_ms,dec_active_prop_gain,
dec_total_pulse_ms,dec_direction,dec_suppressed
```

This records the AI's prediction (split into `physics`/`mlp` components, §6.2) *and* the full per-axis blend breakdown for whichever algorithm actually ran that frame (`ra_algorithm`/`dec_algorithm` can be `AI | AI-Dark | GPG | GPG-Dark | Linear | Hysteresis | Standard`) — i.e. it records what was actually sent to the mount, not just the AI's isolated opinion, which is what makes counterfactual shadow-mode analysis possible (`EVALUATION.md`).

### 10.4 Deprecations

The experimental GRU prototype this feature superseded is not present in KStars — it was an earlier, standalone-repo-only exploration that established the evaluation approach (shadow-mode counterfactual analysis, confidence gating) but never shipped in-tree.

---

## 11. Evaluation Framework

### 11.1 Primary Metrics

All metrics computed on the post-warmup portion of a session.

| Metric | Definition | Target |
|---|---|---|
| RMS improvement vs. proportional-only | `(1 - RMS_AI / RMS_prop) * 100%` | See §11.3 per mount type |
| Safety score | Fraction of sessions where AI causes RMS degradation > 10% | < 1% of sessions |
| Warmup convergence time | Frames until confidence reaches 0.8 | Reasonably fast relative to session length — see each guider's `warmupFrames()` in §6 |

### 11.2 Shadow-Mode and Simulation Evaluation

`evaluate_shadow.py` (`kstars/ekos/guide/offlinetrainer/`) computes a counterfactual RMS comparison directly from the AI debug CSV (§10.3) for shadow-mode sessions — see `EVALUATION.md` for the full method, its accuracy caveats, and usage. `analyze_oscillation.py` in the same directory is the complementary tool for diagnosing oscillation/ringing once AI guiding is active, distinguishing feed-forward phase-mismatch from a pre-existing mount/PID characteristic.

### 11.3 Acceptance Criteria

| Mount Type | RA Improvement | DEC Improvement | Safety |
|---|---|---|---|
| WORM_GEAR | > +15% | > +5% | CF never worse than classic by > 5% |
| HARMONIC_DRIVE | > +8% combined | > +5% | CF never worse by > 5% |
| DIRECT_DRIVE | > +5% combined | > +5% | CF never worse by > 5% |

Don't make a go/no-go decision from a single session — at least 3 sessions on the same equipment profile is the practical minimum (seeing/balance varies session to session, and the model's phase estimate or Kalman state needs a couple of sessions to show it's stable).

---

## 12. Risks and Limitations

### 12.1 Closed-Loop Distribution Shift (All Models)

**Risk**: All models are trained on data generated by a different controller (standard proportional, gain-locked by PID Auto-Tune). When deployed with AI active, the pulses generated change the error dynamics, which changes what a correct prediction looks like.

**Mitigation**: Confidence-gated blending limits how much the AI can deviate from the proportional fallback. Low initial confidence means the AI starts conservatively and only increases authority as it proves itself frame-by-frame. Running PID Auto-Tune before any sysid data collection (§4.3) addresses a more fundamental version of the same problem one step earlier: a badly-tuned base gain would contaminate every subsequent measurement.

**Residual limitation**: The AI never fully exploits its predictive power because it's always blended with a reactive controller. An online learning loop would address this but needs careful stability analysis to avoid divergence — a research item, not near-term work.

### 12.2 Nyquist Aliasing for Harmonic Drive PE (HarmonicGuider)

**Risk**: If the primary harmonic drive PE period is < 4x the guide frame interval, the PE aliases and is indistinguishable from noise.

**Mitigation**: The Kalman filter gracefully degrades in this case — it falls back to tracking longer-period trends (polar drift, slow harmonic). Confidence automatically reduces AI authority when prediction accuracy is low. In practice, PE fundamentals on tested rigs run 288-865s, far above the Nyquist floor at any guide cadence in normal use — aliasing is a real concern in principle (part of why §4.5 recommends 0.5-1.0s exposure for harmonic drives) but hasn't been the limiting factor observed on hardware so far; elastic windup (§6.2.1) is the effect that's actually been unresolvable, not aliased PE.

### 12.3 Training Data Scarcity for Rare Mounts

**Risk**: Rare or custom mounts may have only 1-2 training sessions available. Small datasets risk overfitting.

**Mitigation**: Model parameter counts are kept small by design (~200 for WormGear, ~90 + 8 physical for Harmonic, 4 for DirectDrive). EkosLive cloud training (§5.5), if used, can provide a warm start.

### 12.4 Mount Type Misclassification

**Risk**: The lookup table may be incorrect for some mounts, or a user overrides to the wrong type. A WormGearGuider running on a harmonic drive mount will mismodel the spring dynamics and may over-correct.

**Mitigation**: Confidence detects poor prediction accuracy and reduces AI authority automatically. AI authority starts at 0 for any new equipment profile and ramps up only with demonstrated prediction accuracy.

### 12.5 Seeing-Dominated Sessions (All Models)

**Risk**: In poor seeing, RMS is dominated by atmospheric noise. AI feed-forward corrections become small relative to seeing noise and may occasionally worsen RMS slightly.

**Mitigation**: Confidence naturally reduces AI authority when SNR degrades. The Q-net/SNR factor in each guider's confidence computation is specifically there to detect this.

### 12.6 EkosLive Dependency Risk

**Risk**: If EkosLive is unavailable, cloud training fails.

**Mitigation**: EkosLive is entirely optional (§5.5); the local offline Python trainer works fully offline.

### 12.7 Thermal Drift of Mechanical Parameters

**Risk**: Spring constant kappa and release time constant tau for harmonic drives could in principle change with temperature.

**Mitigation**: Currently moot in practice since kappa calibrates to 0 on every rig tested (§6.2.1). If a future rig or dedicated reversal-test experiment ever resolves a meaningful kappa, seasonal retraining would be the mitigation.

### 12.8 DEC Refraction Fitting Degeneracy (DirectDriveGuider and WormGearGuider)

**Risk**: `k_ref_dec * sin(q) / cos²(alt)` and `d_polar` can be confused if all sysid free-drift sessions are collected near the meridian (`sin(q) ≈ 0`).

**Mitigation**: The DIRECT_DRIVE sysid protocol requires positions at different azimuths, not just altitudes (§4.4). The trainer checks parallactic-angle range and warns + falls back to `k_ref_dec = k_ref` (physically correct to first order) if coverage is insufficient.

### 12.9 Relationship to Existing Predictive Guiding (GPG), and Why This Architecture Is Sound

Running a classical controller tune-up first (PID Auto-Tune, §4.3), then adding a confidence-gated predictive feed-forward term on top, is standard hybrid feedback+feedforward control practice — not a novel or risky approach specific to this project. The general pattern is well established in the disturbance-rejection literature: the feed-forward term handles the predictable part of the disturbance; the feedback loop, correctly tuned since it was never asked to absorb systematic error the feed-forward layer removes, mops up whatever's left, including the genuinely unpredictable part.

KStars already ships an independent, peer-reviewed instance of the same general idea: `internalguide/MPI_IS_gaussian_process/`, the vendored Gaussian Process Guiding (GPG) library from Klenske, Zeilinger, Schölkopf & Hennig (Max Planck Institute for Intelligent Systems) — the same predictive-RA-PE approach behind PHD2's "Predictive PEC." That published work reports **~20% RMS reduction** from adding a Gaussian-process-based predictive term on top of an already-guiding telescope mount, using an online hyperparameter estimation scheme that explicitly needs enough data to cover the periodic structure before its predictions are trusted — the same warmup-before-trust pattern used here (§8.3's confidence ramp, the WARMUP state in §10.2). GPG is narrower in scope than the AI Guider (RA periodic error only, one mount-agnostic model, no offline per-mount-class training step), but demonstrates that the core idea — classical reactive control augmented by a predictive term for the repeatable part of the error, gated by how much history the predictor has actually seen — already works and ships in this codebase, for a closely related problem, from an independent source.

`RAGuidePulseAlgorithm`/`DECGuidePulseAlgorithm` treats `GPG` and the AI algorithms as mutually exclusive selections (§9.1): a user picks one predictive layer or the other per axis, not both simultaneously, since both target the same class of systematic RA error and would otherwise double-correct — the same reasoning behind `AIProportionalBackoff` (§8.2) existing to prevent the AI feed-forward term from fighting the classical P-term.

The acceptance criteria in §11.3 remain the actual empirical check on whether any specific trained model delivers a real improvement on real hardware — the architecture being sound in general does not exempt any specific model from that check. Confidence gating limits the downside if a specific model is wrong; it does not guarantee the model is right.

---

## 13. Future Work and Known Limitations

Concrete, scoped items not yet addressed, in rough priority order:

- **UI locking for Category A settings** (§9.4): Guide Options doesn't yet grey out exposure/gain/pulse settings while an AI algorithm is selected. The fingerprint check at session start (§9.3) catches a mismatch, just later than a locked UI would.
- **`hour_angle_deg` / `guide_optics_oag` population** (§7.2): tube flexure and differential guide-scope flexure are designed for but not yet modeled by any shipped guider, since `gmath.cpp` doesn't populate these two `GuideFrameData` fields yet. Both source values (hour angle from mount RA + LST, OAG-vs-guide-scope from the optical train config) are already available elsewhere in Ekos.
- **Third/fourth PE line tracking for HarmonicGuider** (§6.2): the trainer detects up to four PE lines but the 14-state Kalman filter only tracks the top two. Worth revisiting if a secondary/tertiary harmonic turns out to carry more energy than expected on some rig.
- **Elastic wind-up follow-up experiment** (§6.2.1): if ever revisited, a dedicated large/reversing DEC pulse pair (not part of the normal protocol) is the one experiment that would actually test detectability, rather than continuing to fit the existing guide-scale pulse data harder.
- **Online learning loop** (§12.1): training the AI on data it generated itself would address the residual closed-loop distribution-shift limitation, but needs careful stability analysis to avoid divergence.
- **Cross-mount transfer learning**: whether a model trained on one worm-gear mount improves performance on a different worm-gear mount of the same class — would need real deployment data across multiple mounts to evaluate meaningfully.
