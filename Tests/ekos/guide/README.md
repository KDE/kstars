# Internal Guide Unit Tests

This document describes the unit tests in `Tests/internalguide/`.  These tests
cover the internal guide algorithm classes in `kstars/ekos/guide/internalguide/`
and the `Calibration` helper.  No KStars window, no PHD2, and no guide camera
hardware is required.

---

## Prerequisites

- `INDI_FOUND` + `CFITSIO_FOUND` (the parent CMakeLists guards this directory)
- Not built on Windows (`if (NOT WIN32)` in CMakeLists)

---

## Running the tests

```bash
./build/Tests/internalguide/testguidestars -v2
./build/Tests/internalguide/teststarcorrespondence -v2
./build/Tests/internalguide/testcalibrationprocess -v2
```

---

## Test inventory

### `testguidestars.cpp` — Multi-star guiding and calibration

#### `basicTest`

Tests the `GuideStars` class end-to-end:

- **`setCalibration()` and arc-second conversion** — verifies that
  `convertToArcseconds()` produces the correct arcseconds-per-pixel value for
  a given focal length, pixel size, and binning.
- **`computeStarDrift()`** — checks the RA/DEC drift calculation at four
  calibration angles (0°, 90°, 180°, 270°) and confirms that the rotation
  matrix correctly maps pixel deltas to RA/DEC arcsecond deltas.
- **`selectGuideStar()`** — verifies that the highest-scoring star is chosen,
  that border proximity disqualifies a candidate, and that a close neighbour
  (distance < threshold) also disqualifies a candidate.
- **`getDrift()`** — with five reference stars set directly, checks that the
  median multi-star drift is computed correctly, that a single outlier star is
  excluded when its drift is > 2 arcsec from the guide star, and that the
  `SkyBackground` SNR threshold is respected.
- **`findMinDistance()`** — verifies the nearest-neighbour distance calculation
  for a three-star list.

#### `calibrationTest`

Tests the `Calibration` class API:

- Pixel ↔ arcsecond conversion with and without binning changes at runtime
  (`setBinningUsed()`)
- RA/DEC pulse rate storage and retrieval
- `rotateToRaDec()` at 0°, 90°, 180°, 270°
- Serialise / restore round-trip (`serialize()` / `restore()`)
- Pier-side flip: restoring on east vs. west pier inverts the rotation angle
  correctly
- `updateRAPulse()` — RA rate correction for declination offset during a session
- `calculate1D()` and `calculate2D()` — single-axis and two-axis calibration
  from measured pixel displacements and pulse durations
- Edge cases: null/NaN declination, unknown pier side

#### `testFindGuideStar`

Development/debugging scaffold for `GuideStars::findGuideStar()` using real
guide FITS files.  The test body is `#if 0`-gated and is skipped in normal
runs.  Activate it locally by removing the `#if 0` guard and providing the
required FITS files.

---

### `teststarcorrespondence.cpp` — Star correspondence across frames

Tests the `StarCorrespondence` class which matches stars between guide frames
when the guide star is lost or the field rotates.  Covers:

- Building a reference star list
- Finding the best match transformation (translation + optional rotation)
- Handling partial star sets (some stars absent in a given frame)
- Robustness to outlier stars

---

### `testcalibrationprocess.cpp` — Calibration process state machine

Tests the `CalibrationProcess` class which drives the RA/DEC pulse-movement
sequence used to calibrate the guider.  Covers the state transitions:
idle → measure RA → measure DEC → compute → done / failed.

- **`testFailures`** — a simulated mount moves the star a fixed step for each
  pulse, except in one broken direction.  The calibration fails and says why
  (`getFailReason()`): RA or DEC drift too short when an axis does not move out,
  RA or DEC no return when it does not come back, with the pulse and the drift
  of that moment.  The log line names the same failure: a DEC axis that does
  not move out used to be logged as one that does not come back.
- **`testFirstStepStuck`** — regression test: a star that does not move on the
  first RA pulse gets a doubled pulse next, as on later steps.  The first step
  compared the star's y position with 0 instead of its start.

---

### `testguidediagnosis.cpp` — Guide failure rules

Tests `Ekos::GuideDiagnosis::diagnose()`, which turns `GuideFacts` into the
diagnosis report of a calibration, guiding or dither failure, and
`GuideDiagnosis::checkup()`.  It is pure: no devices, no options, no widgets.

- **`testRules`** — one data row per rule: facts in, item ids and their order
  out.  Calibration failures point at the mount first (balance, clutch, power,
  cables, backlash, the ST4 cable when the pulses go through the camera), the
  settings second.  PHD2 gets the mount checks and its own reason, but no
  internal guider settings.  No item moves the mount by itself: "Done, try
  again" restarts calibration with guide pulses only.
- **`testItemDetails`**, **`testSuggestions`** — the pulse and drift in the
  texts, and the limits of the values the items set.
- **`testReasonKeys`**, **`testEvidence`**, **`testCheckup`** — stable keys, the
  evidence, and the check-up, including a guide rate that the last calibration
  measured too low.

How Guide feeds these rules is tested with the simulators in
`Tests/kstars_ui/test_ekos_guide_diagnosis.cpp`: a Telescope Simulator whose RA
guide rate is 0 fails calibration with "RA drift too short" (the reason is in
the log before the diagnosis), raising the pulse from the report calibrates
again, and Stop gets no report.  `testCalibrationWithDecNorthOnly` calibrates
with DEC guiding limited to north: the direction switches limit guiding
corrections only, calibration pulses in all four directions.  It hung before.
