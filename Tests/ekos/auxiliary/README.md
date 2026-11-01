# Ekos Auxiliary Unit Tests

This document describes the unit tests in `Tests/ekos/auxiliary/`. The
`darkprocessor/` subdirectory tests the dark frame processing and defect map
subsystem, `fieldderotation/` tests Alt-Az field derotation, `solver/` tests the
plate solving defaults, `diagnosis/` tests the failure diagnosis types and runner,
and `imageanalysis/` tests the star shape analysis of a frame in `kstars/ekos/auxiliary/`.

---

## Subdirectories

| Subdirectory | What it tests |
|---|---|
| [`darkprocessor/`](darkprocessor/) | Dark frame defect detection and hot-pixel correction |
| [`fieldderotation/`](fieldderotation/) | `FieldDerotation`, the Alt-Az offset model in `RotatorUtils`, `ISD::Mount` mount type |
| [`solver/`](solver/) | `SolverUtils` threading and blind field width selection, migration of saved align profiles |
| [`diagnosis/`](diagnosis/) | `Diagnosis` report types and the `DiagnosisManager` runner |
| [`imageanalysis/`](imageanalysis/) | `ImageAnalysis`: good, too few stars, out of focus, trailed or double images |

---

## Prerequisites

- `darkprocessor/`: `INDI_FOUND` + `CFITSIO_FOUND` (parent CMakeLists guards).
  The test loads a FITS file from a relative path; run from the build root.
- `fieldderotation/`: `INDI_FOUND`. No devices or INDI server are needed.
- `solver/`: `StellarSolver_FOUND` + `CFITSIO_FOUND`. No index files or images
  are needed; the saved profile fixtures are found with `QFINDTESTDATA`.
- `diagnosis/`: always built. No devices, no window, no network.
- `imageanalysis/`: `CFITSIO_FOUND`. The rendered frames are written to a
  temporary directory; the real frames from `Tests/fitsviewer` and `Tests/kstars_ui` are found with
  `QFINDTESTDATA`.

---

## Running the tests

```bash
# From the repo build directory:
./build/Tests/ekos/auxiliary/darkprocessor/testdefects -v2
./build/Tests/ekos/auxiliary/darkprocessor/testsubtraction -v2
./build/Tests/ekos/auxiliary/solver/test_ekos_solverprofiles -v2
./build/Tests/ekos/auxiliary/diagnosis/testdiagnosis -v2
./build/Tests/ekos/auxiliary/diagnosis/testdiagnosismanager -v2
./build/Tests/ekos/auxiliary/imageanalysis/testimageanalysis -v2
QT_QPA_PLATFORM=offscreen ./build/Tests/ekos/auxiliary/fieldderotation/test_ekos_fieldderotation -v2
```

---

## Test inventory

### `darkprocessor/testdefects.cpp` — Hot pixel detection and correction

Tests `DefectMap` and `DarkProcessor::normalizeDefects()`.

**Scenario:**

1. Loads `hotpixels.fits` — a synthetic dark frame with known hot pixels.
2. Injects two additional artificial hot pixels at known positions (50,10) and
   (80,80) by writing `255` directly into the image buffer.
3. Scans the buffer for pixels above threshold 200; verifies that exactly
   5 hot pixels are found (3 original + 2 injected).
4. Calls `DefectMap::setDarkData()` to build the defect map from the dark frame.
5. Calls `DarkProcessor::normalizeDefects()` to correct the image.
6. Re-scans all previously-hot pixel positions and verifies that every one is
   now below 100 — confirming that the correction replaced hot pixels with
   interpolated values from their neighbours.

**Note:** The test is skipped (via `QSKIP`) if `hotpixels.fits` cannot be
found in the expected relative path, so it passes harmlessly in environments
where the FITS file is absent.

---

### `darkprocessor/testsubtraction.cpp` — Dark frame subtraction

Tests `DarkProcessor`'s master dark subtraction path — applying a scaled dark
master to a light frame and verifying that the result matches the expected
pixel values within tolerance.

---

### `diagnosis/testdiagnosis.cpp` — Diagnosis report types

Tests `kstars/ekos/auxiliary/diagnosis.h`: `order()` (tier, then blockers before
warnings, stable), the JSON field names and types sent to EkosLive and D-Bus, and the
log lines written to the module log.

---

### `diagnosis/testdiagnosismanager.cpp` — Diagnosis runner

Tests `DiagnosisManager` with a fake module and a fake action table:

- unknown items, stale report ids and non-auto items are refused;
- an auto item is applied once, added to the tried list and the module is retried;
  the next report of the same module and train carries the tried list;
- items that move hardware need a confirmation and never retry the module by themselves;
- one item at a time (`busy`), asynchronous completion, timeouts, and a completion that
  arrives before `apply()` returns;
- revert of a setting change, manual Done with `verified` and `verify_failed`,
  `fixed` on success, dismiss for the session and for good (`DiagnosisDismissedItems`);
- the client time offset and the rebuild of cached reports.

---

### `imageanalysis/testimageanalysis.cpp` — What is wrong with a frame

Tests `Ekos::ImageAnalysis` (`kstars/ekos/auxiliary/imageanalysis.h`), which tells from the star shapes
whether the frame itself is the problem, e.g. why a solve failed. It detects the stars with OpenCV and
measures the profile of the 30 brightest isolated, unsaturated stars from all their pixels, with no detection
threshold: the concentration R80/R20 (2.7 for a Gaussian star, about 2.0 for a defocused disc), the centre
against the brightest ring (donuts), and the second moments (elongation, direction). Frames with a known fault
are rendered to FITS and measured:

| Frame | Verdict |
|---|---|
| 150 round, focused stars | `good`: the image is ruled out, look for other causes |
| no stars, 5 stars | `few_stars` |
| large Gaussian stars (sigma 5 px), faint focused stars | `good`: a bright core, whatever their size or brightness |
| defocused discs and donuts, small (HFR 3.5 px) and large (HFR 11 px) | `out_of_focus`; donuts also have a dark centre |
| straight 20 px and 60 px trails, curved trails | `trailed`, with the length and direction of the trails |
| every star doubled 9 px apart | `double_images` (a mechanical jump) |
| stars stretched towards the centre, like coma | `good`: elongated, but not parallel |
| `Tests/kstars_ui/Kocab.fits`, a real sky frame with undersampled stars | `good` |
| `Tests/fitsviewer/ngc4535-autofocus1..3.fits`, a real autofocus run | `good`: the first two are slightly out of focus, but their stars keep a bright core; only their size against a focused frame would tell |
| `Tests/fitsviewer/m47_sim_stars.fits` | `good` |

- **`testHotPixels`** — single hot pixels of an uncalibrated frame are not stars: an empty frame with 300 of
  them has too few stars, and they do not change the verdict of a good frame.
- **`testFewDiscsAmongHotPixels`** — a defocused colour frame with a few large, faint discs and many hot pixels
  spread by debayering, which look like small focused stars and outnumber the discs. The medians weigh each star
  by the square root of its flux, so the discs decide: `out_of_focus`. With plain medians it reads `good`.
- **`testColourFrame`** — a three channel frame is measured on the mean of its channels.
- **`testCroppedStarCount`** — the star count of the whole frame is estimated from the measured region.
- **`testTinyFrame`** — a region too small for a star and its background is not measured.
- **`testSiderealTrail`** — the trail of an untracked mount is measured as long as the sidereal drift.
- **`testMeasurementRegion`** — frames larger than 2048 x 2048 are measured in their centre, as Align does.

A 1024 x 768 frame takes 15 to 65 ms, the 2048 x 2048 centre of a 26 MP frame about 60 ms.

Set `IMAGE_ANALYSIS_FILES` to image files separated by `:` (FITS, JPEG, PNG) to print the verdict and the
measurements of each in `testExtraFiles`, with the time taken to load and to analyse it.

---

## Known gaps in `kstars/ekos/auxiliary/`

Most Ekos auxiliary classes have **no unit tests**.  The following are the most
valuable targets:

| Class | Source file | What to test |
|---|---|---|
| `FilterManager` | `filtermanager.cpp` | Filter sequence, offset application, autofocus triggers |
| `SolverUtils` | `solverutils.cpp` | Plate-solve result parsing and WCS extraction |
| `RotatorUtils` | `rotatorutils.cpp` | Position angle ↔ mechanical angle conversion |
| `OpticalTrainManager` | `opticaltrainmanager.cpp` | CRUD operations on the SQLite optical train store |
| `DarkLibrary` | `darklibrary.cpp` | Dark frame matching by temperature, exposure, and binning |
| `FilterOffsets` | `buildfilteroffsets.cpp` | Focus offset calculation from filter measurements |

---

### `fieldderotation/testfieldderotation.cpp` — Alt-Az field derotation

Tests `Ekos::FieldDerotation`, the Alt-Az offset model in `RotatorUtils`
(`setAltAz()`, `updateParallacticAngle()`, `updateOffset()`) and the
`TELESCOPE_MOUNT_TYPE` handling in `ISD::Mount`.

- The parallactic angle formula matches known values and, for a grid of
  latitudes, declinations and hour angles, the position angle of the zenith
  computed with the KStars coordinate transforms.
- For an Alt-Az mount the rotator offset follows the parallactic angle, a new
  calibration (plate solve) is taken at the current parallactic angle, and
  equatorial mounts are unaffected.
- Decision logic: no correction while slewing or while the rotator moves,
  errors below the threshold accumulate instead of being dropped, the error
  wraps at ±180°, large jumps are held instead of chased, a slew to a new target
  holds the new PA while a short re-centering slew keeps the held PA.
- Closed loop: a simulated Alt-Az mount and rotator (constant speed, shortest
  path) are run for up to one hour with 1 s updates for several sky positions
  (setting target, transit between zenith and pole, 5° from the zenith,
  southern hemisphere) and every combination of driver reversal and parity.
  The camera PA must stay within the threshold plus one update of field
  rotation, while the uncorrected field turns by several degrees. A second
  scenario adds a plate solve with re-centering every two minutes and checks
  that the error does not accumulate.

The simulated rig assumes the KStars rotator model (normal or reversed) has been
set up correctly, as Align does through its parity check. It does not prove the
direction of a particular rotator; that needs a test on a real Alt-Az mount.

---

### `solver/testsolverprofiles.cpp` — Plate solving defaults

Tests `SolverUtils::selectMultiAlgorithm()`, `SolverUtils::applyExpectedFieldWidth()`
and the align profile migration in `stellarsolverprofile.cpp`.

- With a scale or position hint a single solver thread is used, a blind solve
  uses parallel solvers on multiple scales, explicit choices are kept.
- Without a scale hint, the field width search is limited to 1/4x to 4x of
  the expected field width.
- There is one built-in align profile.
- `savedalignprofiles_searchradius.ini` and `savedalignprofiles_custom.ini` are
  profile files saved by KStars 3.6 - 3.8. Unmodified copies of built-in
  profiles are replaced by the current built-in profile, keeping the user's
  search radius; retired profiles are dropped and their users moved to the
  default profile; user profiles and modified built-in profiles are kept.
- `loadAlignOptionsProfiles()` rewrites the saved file and the selected
  profile once, and leaves them unchanged on the next load.
