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
- **`testRunCheck`** — `checkRun()` on Linear 1 Pass runs recorded with the simulators
  (in focus at 36,700, in-focus HFR 1.62 px) and on the sky (2026-11-16, in-focus HFR
  0.907 px). A run never reaches focus (`no_minimum`) when it has no V and its smallest
  stars are at least `NO_MINIMUM_RATIO` (2) times the reference with a poor or failed curve
  fit, or `ONE_SIDED_RATIO` (2.5) times with a good fit and a significant slope toward an
  end that has not risen again. Where along the run the smallest stars are is left to the
  noise: on the sky they were a few points in from the end. A run through focus is never
  one, also with a poor fit or seeing twice as bad. Without a reference, or with fewer than
  5 points, nothing is flagged; outliers (donuts) are left out.
- **`testReferenceRuns`** — `isReferenceRun()`: only a run that went through focus becomes
  a reference (good fit, no slope toward an end, both ends at least `REFERENCE_RISE` times
  its smallest stars). On the sky a run that stopped 2,900 steps from focus became a
  reference 4 times too large and hid the next one.
- **`testReference`** — the in-focus HFR of a train: the median of its last 5 good runs with
  the same binning. Runs within 15 degrees of the run's altitude count as measured; without
  any, all count, scaled up for a lower altitude (airmass^0.6) but never down. An HFR does not
  convert between binnings: on the sky 0.95 px at 2x2 was 1.35 px at 1x1, not 1.9. On the same
  refractor the stars were as large at 75 degrees as at 31, where the model expected them 37%
  smaller: scaling down would bring good runs closer to failing.
  Runs older than 30 days are ignored. Focus keeps the runs, with their filter, as the
  `reference` metadata of its settings of the train (`OpticalTrainSettings::setMetadata()`).
- **`testNoMinimum`** — the `no_minimum` report: go to the last good focus of the filter
  when it is outside the run, otherwise the direction focus lies in, a larger step size and
  Focus Advisor. A curve failure far from the last good focus offers it as a possible cause.
- **`testSettingsChecks`** — Max Travel smaller than Linear 1 Pass needs (out steps × step
  size + AF Overscan), and backlash corrected twice (driver backlash and AF Overscan both set),
  in the check-up and as possible causes of a curve failure.

How Focus feeds these rules is tested with the simulators in
`Tests/kstars_ui/test_ekos_focus.cpp`, `testFocusDiagnosis`: a run without stars gets one
report, raising the exposure from it changes the setting and starts autofocus again, and
stopping that run gets no report.

`testAutofocusNeverReachesFocus` runs autofocus around focus first, which stores the in-focus
HFR of the train, then starts at 60,000 with steps of 200. That run fails as never reaching
focus (`FOCUS_FAIL_NO_MINIMUM`) instead of succeeding far from focus. Focus handles it like any
other failure and retries it; its report has the reason `no_minimum` and starts with `focus_goto_last_good`, and
applying it moves the focuser to the position of the good run. With the simulator's noise,
Linear 1 Pass sometimes walks toward focus instead until it runs out of steps
(`max_iterations`): also a failure, whose report also offers `focus_goto_last_good`. The test
accepts both; it never accepts a success.

`testTiltSettingsPerTrain` checks that the Tilt Correction Advisory's plate settings are kept as
the `tilt` metadata of the Focus settings of the train: saved at once, never among the Focus
settings or the options, and kept when Focus saves its settings.

**The thresholds are verified on one setup only**: a refractor with an ATR2600M (2x2) on
2026-11-16, and the simulators. They need field data from other optics before they are tuned:
seeing-limited long focal lengths, fast astrographs, and scopes with a central obstruction
(donuts). Never tune them to a single setup.

Focus logs `Autofocus run check: ...` after every Linear 1 Pass run with all the values the
rule uses (points, lowest HFR, reference, ratio, R2, slope t, one-sided, V, direction). Use
these lines from real-sky runs to check the thresholds.
