# Ekos Auxiliary Unit Tests

This document describes the unit tests in `Tests/ekos/auxiliary/`. The
`darkprocessor/` subdirectory tests the dark frame processing and defect map
subsystem, and `solver/` tests the plate solving defaults in `kstars/ekos/auxiliary/`.

---

## Subdirectories

| Subdirectory | What it tests |
|---|---|
| [`darkprocessor/`](darkprocessor/) | Dark frame defect detection and hot-pixel correction |
| [`solver/`](solver/) | `SolverUtils` threading and blind field width selection, migration of saved align profiles |

---

## Prerequisites

- `darkprocessor/`: `INDI_FOUND` + `CFITSIO_FOUND` (parent CMakeLists guards).
  The test loads a FITS file from a relative path; run from the build root.
- `solver/`: `StellarSolver_FOUND` + `CFITSIO_FOUND`. No index files or images
  are needed; the saved profile fixtures are found with `QFINDTESTDATA`.

---

## Running the tests

```bash
# From the repo build directory:
./build/Tests/ekos/auxiliary/darkprocessor/testdefects -v2
./build/Tests/ekos/auxiliary/darkprocessor/testsubtraction -v2
./build/Tests/ekos/auxiliary/solver/test_ekos_solverprofiles -v2
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
