# KStars Test Suite

This document is the entry point for the KStars test infrastructure.  It
describes the directory layout, how to build and run the tests, what external
libraries each group of tests needs, and where the known coverage gaps are.

---

## Table of Contents

1. [Test taxonomy](#1-test-taxonomy)
2. [Build prerequisites](#2-build-prerequisites)
3. [Building the tests](#3-building-the-tests)
4. [Running the tests](#4-running-the-tests)
5. [Python test runner script](#5-python-test-runner-script)
6. [Environment variables](#6-environment-variables)
7. [Directory map and coverage](#7-directory-map-and-coverage)
8. [Known coverage gaps](#8-known-coverage-gaps)

---

## 1. Test taxonomy

| Type | Description | Directories |
|------|-------------|-------------|
| **Unit** | Test a single class or algorithm in isolation; no KStars window, no INDI connection | `auxiliary`, `tools`, `skyobjects`, `fitsviewer`, `ekos/focus`, `ekos/guide`, `ekos/align`, `ekos/scheduler`, `ekos/capture`, `ekos/mount`, `datahandlers`, `ekos/auxiliary` |
| **Integration / UI** | Launch the full KStars application and interact with it via Qt signals or D-Bus mock modules; requires a display (or Xvfb) | `kstars_ui` |
| **Lite UI** | Same idea but for the KStars Lite (QML) variant; Linux-only | `kstars_lite_ui` |

CTest labels:

| Label | Meaning |
|-------|---------|
| `stable` | Passes reliably in CI without external dependencies |
| `unstable` | Known to be flaky; excluded from the default CI run |
| `ui` | Requires a running display and a D-Bus session bus |
| `no-xvfb` | Must run on a real X display (not Xvfb) |
| `astrometry` | Requires astrometry.net index files |

---

## 2. Build prerequisites

The top-level `Tests/CMakeLists.txt` enables subdirectories conditionally:

| Subdirectory | Required CMake condition |
|---|---|
| `auxiliary`, `tools`, `skyobjects`, `datahandlers` | Always built |
| `fitsviewer` | `CFITSIO_FOUND` |
| `ekos/auxiliary`, `ekos/observatory`, `ekos/analyze` | Always built (self-guarded internally) |
| `ekos/mount`, `ekos/capture` | Always built (self-guarded with `INDI_FOUND`) |
| `ekos/scheduler`, `ekos/focus`, `ekos/align` | `INDI_FOUND` + `CFITSIO_FOUND` |
| `ekos/guide` | `INDI_FOUND` + `CFITSIO_FOUND` + not Windows |
| `kstars_ui` | `UNIX && NOT APPLE && CFITSIO_FOUND` |
| `kstars_lite_ui` | `UNIX && NOT APPLE && CFITSIO_FOUND && BUILD_KSTARS_LITE` |
| `skyobjects/test_skypoint` | `NOVA_FOUND` |
| `fitsviewer/testfitsdata` | `StellarSolver_FOUND` (within CFITSIO block) |

Minimum packages for a useful test build on a typical Linux system:
```
libindi-dev    libcfitsio-dev    libnova-dev    stellarsolver
```

---

## 3. Building the tests

```bash
# Configure (from the repo root):
cmake -B build -DBUILD_TESTING=ON

# Build all tests:
cmake --build build --target all -j$(nproc)
```

Test binaries are placed under `build/Tests/<subdirectory>/`.

---

## 4. Running the tests

### Run the full suite
```bash
ctest --test-dir build/Tests --output-on-failure
```

### Run only stable tests (suitable for CI)
```bash
ctest --test-dir build/Tests -L stable --output-on-failure
```

### Run only unit tests (exclude UI tests)
```bash
ctest --test-dir build/Tests -LE ui --output-on-failure
```

### Run a specific test binary directly (recommended for development)
```bash
# Run all tests in the binary with verbose output:
./build/Tests/ekos/focus/testfocus -v2

# Run a single test function:
./build/Tests/auxiliary/testksalmanac testDawnDusk -v2

# Run a single function in the scheduler unit suite:
./build/Tests/ekos/scheduler/testschedulerunit -v2

# Run a single function in the scheduler ops (UI) suite:
./build/Tests/kstars_ui/test_ekos_scheduler_ops testSimpleJob -v2

# Scheduler weather tests with the INDI simulators: run one function per process
# (CTest registers each function as its own test):
./build/Tests/kstars_ui/test_ekos_scheduler_weather testWeatherSoftShutdownSimulator -v2
```

Per-test VS Code launch configurations are available in `.vscode/launch.json`.

---

## 5. Python test runner script

`Tests/run_tests.py` is a self-contained Python 3 script that wraps `ctest`
and the Qt Test binaries to provide a **developer-friendly** experience:

- Auto-discovers all CTest-registered test binaries (no manual list).
- Saves per-test **XML** and **stderr log** files to a timestamped directory.
- Parses the Qt Test XML output and shows the exact **test function**, **source
  file:line**, and **failure message** for every assertion that failed.
- Prints a colourised summary with failures first.
- Writes a plain-text `summary.txt` alongside the logs (useful in CI).
- Returns exit code `0` only when all selected tests pass.

### Prerequisites

Python 3.8+ and a configured CMake build with `BUILD_TESTING=ON`.
No additional pip packages are required — only the Python standard library.

### Quick start

```bash
# From the repository root:

# Run the full suite (also builds first by default):
python3 Tests/run_tests.py

# Skip the build step (use existing binaries):
python3 Tests/run_tests.py --no-build

# List all discovered tests without running them:
python3 Tests/run_tests.py --no-build --list
```

### Filtering tests

```bash
# Run only tests whose name contains "scheduler" (case-insensitive):
python3 Tests/run_tests.py --no-build -k scheduler

# Run only tests tagged with the 'stable' CTest label:
python3 Tests/run_tests.py --no-build -L stable

# Combine: stable tests that don't require a display:
python3 Tests/run_tests.py --no-build -L stable --exclude-label ui

# Run tests with multiple label filters (OR logic):
python3 Tests/run_tests.py --no-build -L stable -L unstable
```

### Parallel execution

```bash
# Run 4 tests at the same time:
python3 Tests/run_tests.py --no-build -j 4

# Note: --verbose forces single-threaded mode (output would interleave)
python3 Tests/run_tests.py --no-build --verbose -k focus
```

### Custom paths and timeout

```bash
# Custom build directory:
python3 Tests/run_tests.py --build-dir /tmp/kstars-build

# Custom log output directory:
python3 Tests/run_tests.py --no-build --log-dir /tmp/my-logs

# Per-test timeout (default is 120 s):
python3 Tests/run_tests.py --no-build --timeout 60
```

### Full CLI reference

| Option | Default | Description |
|---|---|---|
| `-k PATTERN` | — | Run only tests whose name contains `PATTERN` (substring, case-insensitive) |
| `-L LABEL` | — | Include only tests that carry this CTest label; may be repeated |
| `--exclude-label LABEL` | — | Exclude tests with this label; may be repeated |
| `-j N` | `1` | Run `N` tests in parallel |
| `--timeout N` | `120` | Per-test timeout in seconds |
| `--build-dir DIR` | `../build` | CMake build directory |
| `--log-dir DIR` | `./test-logs-<stamp>/` | Directory for log files |
| `--no-build` | off | Skip the `cmake --build` step |
| `--verbose` | off | Stream test output live to the terminal (forces `-j 1`) |
| `--list` | off | List discovered tests and exit without running them |

### Log directory layout

```
test-logs-20260403-212736/
├── summary.txt               ← plain-text copy of the summary (no ANSI codes)
├── CSVParserTest.xml         ← raw Qt Test XML output (stdout)
├── CSVParserTest.log         ← KDE/Qt diagnostic messages (stderr)
├── SchedulerunitTest.xml
├── SchedulerunitTest.log
└── …
```

### Reading the summary

```
────────────────────────────────────────────────────────────────────────
  KStars Test Suite — Summary
────────────────────────────────────────────────────────────────────────
  ❌ FAIL  SchedulerunitTest          0.1s  6p 3f 0s  [stable]
         ↳ loadSequenceQueueTest
           at Tests/ekos/scheduler/testschedulerunit.cpp:344
           'Ekos::SchedulerUtils::loadSequenceQueue(...)' returned FALSE.
         ↳ estimateJobTimeTest
           at Tests/ekos/scheduler/testschedulerunit.cpp:381
           'Ekos::SchedulerUtils::estimateJobTime(...)' returned FALSE.
  ✅ PASS  DMSTest                    0.0s  11p 0f 0s  [stable]
  …
────────────────────────────────────────────────────────────────────────
  27 passed  |  3 failed  |  1.9s total
────────────────────────────────────────────────────────────────────────
```

Column legend: `Np` = N test functions passed · `Nf` = failed · `Ns` = skipped.
The `↳` lines appear only for failing tests and pinpoint the exact assertion.

---

## 6. Environment variables

These are set automatically by `Tests/testhelpers.h` via the `KTest::setupTestEnvironment()` function that runs before `main()`.  You can also set them manually to influence test behaviour.

| Variable | Purpose |
|---|---|
| `KSTARS_TEST_SOURCEDIR` | Path to the repository root; used to locate `kstars/data/` at runtime.  Set by CMake via `add_compile_definitions`. |
| `KSTARS_TEST_DATADIR` | Override the KStars data directory independently of the source tree. |
| `KSTARS_TEST_NO_NETWORK=1` | Block all outbound network connections (loopback is always allowed).  Useful in sandboxed environments. |
| `KSTARS_TEST_ENABLE_NETWORK=1` | Explicitly allow outbound connections even when `KSTARS_TEST_NO_NETWORK` would otherwise be set by CI. |
| `HOME` / `XDG_DATA_HOME` / `XDG_CONFIG_HOME` | Redirected to a per-test-run temp dir (`/tmp/kstars-tests-<pid>/`) so tests never touch the real user profile. |

---

## 7. Directory map and coverage

> Legend: ✅ Covered — ⚠️ Partial — ❌ Not tested — 🔲 Stub (needs tests)

| Directory | Source subsystem | Coverage | README |
|---|---|---|---|
| [`auxiliary/`](auxiliary/README.md) | `kstars/auxiliary/` core utilities | ✅ | ✅ |
| [`datahandlers/`](datahandlers/README.md) | `kstars/catalogsdb/` + `datahandlers/` | ✅ | ✅ |
| [`fitsviewer/`](fitsviewer/README.md) | `kstars/fitsviewer/` | ✅ | ✅ |
| [`projections/`](projections/README.md) | `kstars/projections/` | 🔲 | ✅ |
| [`skycomponents/`](skycomponents/README.md) | `kstars/skycomponents/` | 🔲 | ✅ |
| [`skyobjects/`](skyobjects/README.md) | `kstars/skyobjects/` | ⚠️ | ✅ |
| [`time/`](time/README.md) | `kstars/time/` | 🔲 | ✅ |
| [`tools/`](tools/README.md) | `kstars/tools/` math helpers | ⚠️ | ✅ |
| **`ekos/`** | **All Ekos modules** | | |
| [`ekos/align/`](ekos/align/README.md) | `kstars/ekos/align/` (polar align, pole axis) | ✅ | ✅ |
| [`ekos/analyze/`](ekos/analyze/README.md) | `kstars/ekos/analyze/` | 🔲 | ✅ |
| [`ekos/auxiliary/`](ekos/auxiliary/README.md) | `kstars/ekos/auxiliary/` dark processor | ⚠️ | ✅ |
| [`ekos/capture/`](ekos/capture/README.md) | `kstars/ekos/capture/` | ⚠️ | ✅ |
| [`ekos/focus/`](ekos/focus/README.md) | `kstars/ekos/focus/` algorithms | ✅ | ✅ |
| [`ekos/guide/`](ekos/guide/README.md) | `kstars/ekos/guide/internalguide/` | ✅ | ✅ |
| [`ekos/mount/`](ekos/mount/README.md) | `kstars/ekos/mount/` meridian flip state | ✅ | ✅ |
| [`ekos/observatory/`](ekos/observatory/README.md) | `kstars/ekos/observatory/` | 🔲 | ✅ |
| [`ekos/scheduler/`](ekos/scheduler/README.md) | `kstars/ekos/scheduler/` (unit) | ✅ | ✅ |
| [`kstars_ui/`](kstars_ui/README.md) | Full Ekos UI integration (scheduler, capture, focus, guide, align, mount, meridian flip) | ✅ | ✅ |
| [`kstars_lite_ui/`](kstars_lite_ui/README.md) | KStars Lite UI smoke tests | ⚠️ | ✅ |

---

## 8. Known coverage gaps

The following KStars subsystems have **no automated tests** of any kind.
Contributions are welcome — see the stub `README.md` in each directory marked
🔲 above for guidance on what to add.

### High-priority gaps (testable math / logic, no hardware required)

| Subsystem | Source path | What to test |
|---|---|---|
| Sky projections | `kstars/projections/` | Round-trip pixel↔sky conversions for all 6 projector types |
| `KStarsDateTime` / `SimClock` / `TimeZoneRule` | `kstars/time/` | JD/calendar conversions, clock tick, DST rules |
| Solar system objects | `kstars/skyobjects/` | `KSSun`, `KSMoon`, `KSPlanet` position/phase calculations |
| `KSParser` (binary data reader) | `datahandlers/ksparser.cpp` | Reading star catalogue binary files |
| `RobustStatistics` | `kstars/auxiliary/robuststatistics.cpp` | Median, IQR, Q-estimator — used by focus and guide |
| Sky components | `kstars/skycomponents/` | `ArtificialHorizonComponent`, `StarComponent` loading |
| Observatory model | `kstars/ekos/observatory/` | Dome, weather model state machines |

### Medium-priority gaps (require display or more infrastructure)

| Subsystem | Source path | Notes |
|---|---|---|
| Ekos Analyze | `kstars/ekos/analyze/` | Session log parsing can be tested without a display |
| FilterManager | `kstars/ekos/auxiliary/filtermanager.cpp` | Can be unit-tested with a mock filter device |
| SolverUtils / RotatorUtils | `kstars/ekos/auxiliary/` | Math helpers testable in isolation |
| Optical Train Manager | `kstars/ekos/auxiliary/opticaltrainmanager.cpp` | SQLite-backed; testable with in-memory DB |
| Most of `kstars/tools/` | Conjunctions, eclipses, observing list, star hopper, imaging planner | Astronomical math is testable; UI wiring is not |

### Low-priority / infrastructure-dependent gaps

| Subsystem | Reason |
|---|---|
| HiPS rendering (`kstars/hips/`) | Requires display + network access to tile servers |
| Terrain rendering (`kstars/terrain/`) | Requires display and OpenGL |
| INDI device layer (`kstars/indi/`) | Requires live INDI server (covered indirectly by `kstars_ui` tests) |
| EkosLive (`kstars/ekos/ekoslive/`) | Requires live EkosLive cloud server |
| OAL observing log (`kstars/oal/`) | Simple data-transfer classes; low risk |

---

## Shared test infrastructure

Full documentation: [`shared/README.md`](shared/README.md).

### `Tests/shared/kstars_test_macros.h` — Generic Qt Test macros

Provides portable `KVERIFY_SUB`, `KWRAP_SUB`, `KTRY_VERIFY_WITH_TIMEOUT_SUB`,
`KTRY_GADGET`, `KTRY_CLICK`, `KTRY_SET_CHECKBOX`, `KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT`
and 20+ related macros.  **No INDI, no CFITSIO, no display required.**
Available to every test binary (the include path is set globally in
`Tests/CMakeLists.txt`).

New test files should `#include "kstars_test_macros.h"` instead of copying
macro definitions locally.

### `Tests/testhelpers.h` — Environment isolation

Header-only helpers shared by all test binaries:

| Symbol | Purpose |
|---|---|
| `KTest::setupTestEnvironment()` | Redirects `HOME`, `XDG_*`, `TMPDIR` to an isolated per-run temp tree; configures optional network blocking |
| `KTest::installTestData()` | Symlinks or copies `kstars/data/` into the isolated XDG home so `KStarsData` can initialise |
| `KTest::rootDir()` | Returns `/tmp/kstars-tests-<pid>/` |
| `KTest::copyRecursively()` | Recursive directory copy used by `installTestData()` |
| `KTEST_BEGIN()` | Macro: set app name, enable test mode, clean XDG dirs, install data |
| `KTEST_END()` | Macro: clean up XDG dirs after the test run |

### Ekos integration helpers (in `kstars_ui/`, documented in `shared/README.md`)

| File | What it provides |
|---|---|
| `kstars_ui/mockmodules.h` | D-Bus mock objects for all 5 Ekos modules |
| `kstars_ui/test_ekos_helper.h` | `TestEkosHelper` class + INDI-specific macros |
| `kstars_ui/test_ekos_capture_helper.h` | Capture test helpers |
| `kstars_ui/test_ekos_scheduler_helper.h` | Scheduler test helpers |
