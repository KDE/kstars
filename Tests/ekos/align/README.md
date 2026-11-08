# Polar Alignment Unit Tests

This document describes the unit tests in `Tests/polaralign/`.  These tests
cover the **polar alignment math** in `kstars/ekos/align/` — specifically
`PolarAlign` and `PoleAxis`.  No KStars window, no plate-solving, and no mount
hardware is required.

---

## Prerequisites

- Always built (no additional CMake guards beyond the CFITSIO+INDI block in the
  parent CMakeLists)
- The test for `test_polaralign` copies `ngc4535-autofocus1.fits` from the
  `fitsviewer/` fixture directory at build time

---

## Running the tests

```bash
./build/Tests/polaralign/test_poleaxis -v2
./build/Tests/polaralign/test_polaralign -v2

# Run a single test function:
./build/Tests/polaralign/test_polaralign testPAA -v2
```

---

## Test inventory

### `test_poleaxis.cpp` — Pole axis geometry

Tests the `PoleAxis` class which computes the mount's actual pole axis direction
from three star observations taken at different hour angles.

Key scenarios:

- **Perfect polar alignment** — all three observations consistent with the
  celestial pole; computed axis matches the true pole exactly.
- **Known misalignment** — a deliberate altitude/azimuth offset is injected;
  the computed axis must match the injected offset within tolerance.
- **Numerical stability** — tests with stars close together in hour angle
  and with targets near the meridian.
- **`calculateRotationMatrix()`** — verifies that the 3D rotation matrix that
  maps the mount axis to the pole is orthogonal and has determinant +1.

---

### `test_polaralign.cpp` — Polar Alignment Assistant

Tests the `PolarAlign` class, the higher-level algorithm that drives the
three-point polar alignment procedure in the Ekos Align module.

Key scenarios:

- **`testPAA`** — loads `ngc4535-autofocus1.fits` as a synthetic star field,
  injects three simulated plate-solve results at different mount positions
  (corresponding to three rotation steps), and verifies that `PolarAlign`
  correctly computes the polar alignment error (altitude and azimuth components)
  and the correction vector.
- **Correction move calculation** — given a computed pole offset, verifies that
  `calculateAdjustment()` returns the correct alt-az knob-turn direction and
  magnitude.
- **Refresh after correction** — simulates re-solving after the user moves the
  alt-az adjusters and checks that the updated error estimate decreases.

---

### `testindexfiles.cpp` — Astrometry index files

Tests `Ekos::IndexFiles` (`kstars/ekos/align/indexfiles.h`), the pure helpers the Index
Files page and the Align diagnosis share. No network and no KStars window.

- **`testSizeRoleMatchesIndexPage`** — compares `sizeRole()` with the loop the Index Files
  page used before, for fields of view from 0.5′ to 3000′.
- **`testRequiredIndexSizes`** — required sizes for a few fields of view.
- **`testInstalledIndexSeries`** — complete, partial and duplicate series in temporary folders.
- **`testInstallName`** — the series the diagnosis installs: 52nn LITE up to size 06, 42nn above.
- **`testEstimatedDownloadBytes`** — the download estimate shown to the user (about 151 MB for 4207).

---

### `testaligndiagnosis.cpp` — Align failure rules

Tests `Ekos::AlignDiagnosis::diagnose()`, which turns `AlignFacts` into a diagnosis report, and
`Ekos::AlignDiagnosis::checkup()`, which checks the setup when nothing failed.
It is pure: no devices, no options, no widgets.

- **`testRules`** — one data row per rule of the Align catalog: facts in, item ids and their
  order out. Items go from the most likely cause to the least likely (`likely`, `possible`,
  `check`); within the same likelihood blockers come before warnings, then hardware,
  configuration, data and tuning, then the cheapest fix. Few stars in a short exposure come
  before a mount clock that is off. A mount or camera that disconnected is offered for the
  failures further down the line too, e.g. a solve or slew failure. Every required
  index size must be installed: an 18 x 14 arcmin field with only 5205 and 5206 still needs 5203
  and 5204. Out of focus stars are flat discs or rings (`check_focus`); large stars with a bright
  core are focused and not a cause.
- **`testItemDetails`** — costs, retry flags, open targets and the index download estimate.
- **`testTriedAndDismissed`**, **`testFewStarsAfterTries`** — items already tried or dismissed
  are not offered again; the next item in line is.
- **`testRuledOut`**, **`testClock`**, **`testEvidence`** — the ruled out and not checked lines
  (an out of focus frame is not ruled out, and the item says whether the stars are discs or rings),
  the clock comparison and the evidence. The KStars clock is compared with the client device
  (EkosLive), or with this computer when there is none; the client time is never applied. The
  mount clock is the time the mount last reported in `TIME_UTC` against when that report arrived,
  never against the current time: most drivers report their time once, when KStars sends it.
- **`testReasonKeys`** — every `AlignFailReason` has a unique key, a title and at least one item;
  the enum values that go out on D-Bus and EkosLive do not move.
- **`testLikelihood`** — which items are likely (the failure reason or the measured frame names
  them, missing index files, few stars) and which are only possible (clocks, solver profile,
  reducer) or advice (a narrow field).
- **`testCheckup`**, **`testCheckupItems`** — the check-up: devices, optics, index files, clocks,
  reducer and field of view always; exposure, binning and solver profile only when Align has not
  solved with the current settings. Its items never retry Align by themselves, and a setup that is
  fine gets no items and the lines of what was checked.

---

The star shapes of the failed frame are measured with `Ekos::ImageAnalysis`, tested in
`Tests/ekos/auxiliary/imageanalysis/`.

The Align code that records the reasons and runs the fixes is tested with the INDI
simulators in `Tests/kstars_ui/test_ekos_align_diagnosis.cpp`.

```bash
./build/Tests/ekos/align/testindexfiles -v2
./build/Tests/ekos/align/testaligndiagnosis -v2
```
