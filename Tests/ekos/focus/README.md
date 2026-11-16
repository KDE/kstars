# Focus Algorithm Unit Tests

This document describes the unit tests in `Tests/focus/`.  These tests cover
the **focus algorithm implementations** in `kstars/ekos/focus/` — specifically
`FocusAlgorithmInterface` and the concrete implementations created by
`MakeLinearFocuser()`.  No KStars window, no INDI connection, and no camera
hardware is required.

---

## Prerequisites

- `INDI_FOUND` + `CFITSIO_FOUND` (pulled in by the parent CMakeLists guard)
- No KStars data required

---

## Running the tests

```bash
# Run all focus tests:
./build/Tests/focus/testfocus -v2
./build/Tests/focus/testfocusstars -v2

# Run a single test function:
./build/Tests/focus/testfocus L1PHyperbolaTest -v2
```

---

## Test inventory

### `testfocus.cpp` — Linear focuser algorithm

#### `basicTest`

Runs the **Linear** focuser (`FOCUS_LINEAR`, quadratic curve fit) through a
complete simulated V-curve and exercises:

- Correct initial position calculation (respecting `maxTravel` and
  `maxPositionAllowed` limits)
- First pass: position decreases by `initialStepSize` as HFR improves
- Second pass: half step-size, terminates when HFR is within `focusTolerance`
  of the first-pass minimum
- **Retry logic:** when HFR worsens during the second pass the focuser samples
  the same position up to three times before deciding whether to continue or
  finish
- **Miss-tolerance path:** when the focuser misses the desired HFR it resets to
  a polynomial-fit position and retries; tests two and three resets are also
  verified
- **Max iterations exceeded:** after 30 steps the focuser reports failure
  (`solution() == -1`)

#### `restartTest`

Replays a real-world log sequence where the starting position was "too low" on
the V-curve.  Verifies that after several consecutive worsening HFR samples the
focuser correctly resets to a higher initial position rather than continuing
inward.

#### `L1PHyperbolaTest`

Runs the **Linear 1-Pass** focuser (`FOCUS_LINEAR1PASS`) with a hyperbola curve
fit (`FOCUS_HYPERBOLA`) through a synthetic 11-point data set.  Verifies:

- Correct initial position
- All 11 first-pass measurements are consumed without branching
- `solution()` is placed at the curve minimum
- `latestValue()`, `getMeasurements()`, `getPass1Measurements()`, and
  `getTextStatus()` return correct values
- A single additional measurement at the solution confirms `isDone()` and
  `doneReason() == "Solution found."`

#### `L1PParabolaTest`

Same structure as `L1PHyperbolaTest` but uses a parabola curve fit
(`FOCUS_PARABOLA`).  Verifies that `getTextStatus()` reports the correct curve
name.

#### `L1PQuadraticTest`

Same structure but uses a quadratic curve fit (`FOCUS_QUADRATIC`).  Verifies
that L1P with quadratic behaves identically to parabola for this symmetric
data set.

---

### `testfocusstars.cpp` — Focus star statistics

Tests the star HFR / FWHM measurement statistics used to evaluate focus quality
from a list of detected stars, including outlier rejection and weighted averages.

---

### `testfocusdiagnosis.cpp` — Autofocus failure rules

Tests `Ekos::FocusDiagnosis::diagnose()`, which turns `FocusFacts` into the
diagnosis report of a failed autofocus run, and `FocusDiagnosis::checkup()`, which
checks the setup when nothing failed. It is pure: no devices, no options, no widgets.

- **`testRules`** — one data row per rule: facts in, item ids and their order out.
  Devices (`camera_connect`, `focuser_connect`, `check_focuser`, ...), a frame without
  stars (raise the exposure, bin 2x2, then the sky and the cap; rings in the frame mean
  the focuser is far from focus), and the failures of the focus curve (maximum travel,
  step size, Focus Advisor, the seeing). A device that dropped out comes first.
- **`testItemDetails`** — costs, open targets and the values in the titles.
- **`testSuggestions`** — the exposure, step size and maximum travel the items set, and
  their limits: the largest single step and the focuser range.
- **`testDiagnosable`** — aborts, Focus Advisor reruns and runs that were optimised out
  get no diagnosis. A run that gave up after its retries ends with a forced abort: the
  reason of its failed attempts is reported instead (`reportedReason()`).
- **`testReasonKeys`** — every `AutofocusFailReason` has a unique key; every diagnosed
  reason has a title and at least one item.
- **`testInSequence`** — runs started by a capture sequence or the scheduler never start
  autofocus again from the diagnosis.
- **`testTriedAndDismissed`**, **`testEvidence`**, **`testCheckup`** — filtering, the
  evidence and the check-up.

How Focus feeds these rules is tested with the simulators in
`Tests/kstars_ui/test_ekos_focus.cpp`, `testFocusDiagnosis`: a run without stars gets one
report, raising the exposure from it changes the setting and starts autofocus again, and
stopping that run gets no report.
