# AGENTS.md — Guidance for AI coding agents working on KStars

This file tells AI agents (Claude Code, Codex, Copilot, Cursor, etc.) how to work in this repository.
Human contributors should read [README.md](README.md) and [Tests/README.md](Tests/README.md); this file
summarises the parts that matter most and adds rules specific to agent-driven changes.

If anything here conflicts with an explicit instruction from the maintainer in the current session, follow the maintainer.

---

## 1. Project at a glance

KStars is a KDE desktop planetarium plus **Ekos**, a full astrophotography suite (capture, focus, guide, align,
mount, scheduler, observatory, analyze) that controls hardware through **INDI**. It is C++17 on Qt 6
(Qt 5 is being phased out) and KDE Frameworks 6, built with CMake/ECM.

| Path | Contents |
|---|---|
| `kstars/` | Main application sources |
| `kstars/ekos/` | Ekos modules (`capture`, `focus`, `guide`, `align`, `mount`, `scheduler`, `observatory`, `analyze`, `ekoslive`, `mcp`, …) |
| `kstars/indi/` | INDI client layer (devices, properties, drivers) |
| `kstars/fitsviewer/` | FITS viewer, image processing, stacking; `pipeline/` is the headless post-processing pipeline (see its `README.md`, which is authoritative) |
| `kstars/skyobjects/`, `kstars/skycomponents/`, `kstars/projections/`, `kstars/time/` | Sky model, catalogs, rendering, time |
| `kstars/auxiliary/` | Shared utilities (`dms`, `KSUtils`, paths, user DB, …) |
| `kstars/kstars.kcfg` | All persistent settings — accessed in code via the generated `Options::` class |
| `kstars/org.kde.kstars.*.xml` | D-Bus interfaces (public API used by scripts, the scheduler, and tests) |
| `kstars/doc/` | DocBook handbook |
| `datahandlers/` | Catalog database handling |
| `Tests/` | QtTest unit and UI tests (see §4) |
| `tools/` | Formatting, static-analysis, and helper scripts |
| `po/` | Translations — synced automatically by KDE scripty (`GIT_SILENT` commits). **Never edit by hand.** |

Mature code runs on real observatories, often unattended overnight. A regression can waste a night of
imaging or put equipment at risk (mount slews, dome/cap motion, cooling). Change carefully and conservatively.

---

## 2. Working rules

1. **Understand before changing.** Read the surrounding module and its callers. Ekos modules talk through
   signals/slots, D-Bus, and state machines (e.g. `CaptureModuleState`, `SchedulerModuleState`), so a local
   edit can have effects far away. Check for existing helpers in `kstars/auxiliary/` and the module's own
   utility classes before writing new ones.
2. **Keep changes minimal and focused.** One logical change per commit. Don't mix refactors or mass
   reformatting with behaviour changes.
3. **Work in the existing checkout.** Edit the live working tree the maintainer has open. Don't create git
   worktrees or branches unless asked.
4. **Don't commit, push, or open merge requests unless asked.** Leave changes as uncommitted working-tree edits by default.
   **Never open an MR/PR until the maintainer has confirmed they tested the change locally.** Pushing a branch
   to a personal fork when asked is fine. Opening the MR needs that confirmation first.
5. **Never run destructive git commands** (`reset --hard`, `checkout -- .`, `clean -fd`, force-push) without
   explicit approval. Run `git status` first.
6. **Don't touch shared or running infrastructure** (running KStars instances, INDI servers, PM2 services,
   system packages) without asking first.
7. **Report outcomes honestly.** If a build or test fails, or a step was skipped, say so and include the output.

---

## 3. Building

Use the existing CMake build directory (`build/`, Ninja, Debug) when there is one. Don't create new build
trees without a reason.

```bash
# Configure (only if build/ does not exist yet). Enable tests.
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_WITH_QT6=ON -DBUILD_TESTING=ON

# Build — never use all CPU cores; leave headroom (see below).
JOBS=$(( $(nproc) * 3 / 4 ))
cmake --build build -j"$JOBS"       # everything
ninja -C build -j"$JOBS" kstars     # just the app
ninja -C build -j"$JOBS" testfocus  # a single test binary
```

- **Always set the job count explicitly, below the number of CPU cores.** A bare `ninja`/`make`/`cmake --build`
  uses every core, and some translation units (e.g. `fitsdata.cpp`, `fitsstack.cpp`) need a lot of memory each
  to compile. Running too many at once can exhaust RAM and trigger the OOM killer. Using about ¾ of the cores
  is a sensible default. Go lower on machines with limited RAM (roughly 2 GB per job). If the maintainer or the
  local agent memory/config gives a specific cap for this machine, use that instead.
- If `BUILD_TESTING` is `OFF` in `build/CMakeCache.txt`, reconfigure with `-DBUILD_TESTING=ON` before
  building tests: `cmake -B build -DBUILD_TESTING=ON`. An IDE sharing `build/` may reconfigure it with
  tests off whenever a `CMakeLists.txt` changes, even mid-build. After editing CMake files, re-check the
  cache, and if a test binary fails to execute, check that it is a valid executable before debugging it.
- For a fast check of one file, compile it with `-fsyntax-only`, using its command from
  `build/compile_commands.json`.
- A change must build with **no new warnings** in the files you touched.
- Optional dependencies (INDI, CFITSIO, StellarSolver, WCSLIB, libraw, …) are guarded by `HAVE_*` macros from
  `config-kstars.h`. Keep new code behind the same guards so builds without them still compile.

---

## 4. Tests — REQUIRED for every change

**Every behavioural change or bug fix must come with tests that are created, run, and validated before the
work counts as done.** "It compiles" is not validation.

### 4.1 What to test

| Change type | Minimum expectation |
|---|---|
| Bug fix | Add a regression test that **fails before the fix and passes after**. Confirm both by running it. |
| New feature / new logic | Unit tests covering the normal path, edge cases, and error handling. |
| Refactor (no behaviour change) | Existing tests covering the code must pass. Add characterisation tests first if coverage is thin. |
| UI-only / wiring change that can't reasonably be unit-tested | Explain why. Run the nearest related existing tests. Give the maintainer concrete manual test steps. |
| Docs / comments / build-only | Build succeeds. No tests needed. |

Put pure logic (calculations, parsers, state transitions, sequence/job handling) somewhere it can be tested
without a KStars window or an INDI connection. That usually makes the test much easier to write.

For scheduler behaviour over time (weather alerts, dawn, end-at times, wakeups), prefer the mock-module
tests in `Tests/kstars_ui/test_ekos_scheduler_ops`: they drive a simulated clock, so hours of scheduler time
run in seconds. Tests with the real INDI simulators run in real time and cannot wait for long timers.
`Tests/ekos/scheduler/README.md` documents the scheduler state machines.

### 4.2 Where tests go

Tests use **QtTest** and live under `Tests/`, in a folder that mirrors the source location:

| Test type | Location | Notes |
|---|---|---|
| Unit | `Tests/auxiliary`, `Tests/tools`, `Tests/skyobjects`, `Tests/fitsviewer`, `Tests/datahandlers`, `Tests/ekos/<module>` | No main window, no INDI. Prefer these. |
| UI / integration | `Tests/kstars_ui` | Launches full KStars, and often the INDI simulators. Needs a display/D-Bus. Slow. |
| Lite UI | `Tests/kstars_lite_ui` | KStars Lite (QML), Linux only. |

To add a test:
1. Create `test<classname>.cpp` (and `.h` if needed) next to similar tests. Copy the structure of an existing
   test in the same folder. Use `QTEST_GUILESS_MAIN` for non-GUI tests and data-driven `_data()` functions
   for tables of cases.
2. Add the test executable to that folder's `CMakeLists.txt`, following the pattern already used there:
   ```cmake
   ADD_EXECUTABLE( testthing testthing.cpp )
   TARGET_LINK_LIBRARIES( testthing ${TEST_LIBRARIES} )
   ADD_TEST( NAME ThingTest COMMAND testthing )
   SET_TESTS_PROPERTIES( ThingTest PROPERTIES LABELS "stable" )
   ```
   Keep the existing `ADD_TEST`/`LABELS` lines for consistency with the other tests (`stable`, `unstable`, `ui`,
   `no-xvfb`, `astrometry`). Put new directories inside the right dependency guard in `Tests/CMakeLists.txt`
   (e.g. `INDI_FOUND`, `CFITSIO_FOUND`).
3. Use the shared helpers in `Tests/shared/kstars_test_macros.h` (`KVERIFY_SUB`, `KWRAP_SUB`, `KTRY_*`) for
   bool-returning helper functions. Put test fixtures (FITS files, `.esq`/`.esl` sequences) next to the test and keep them small.
4. Tests must be deterministic and self-contained. No real network access (`KSTARS_TEST_NO_NETWORK=1`
   is honoured). No dependence on the user's profile: `Tests/testhelpers.h` redirects `HOME`/`XDG_*` to a temp
   dir, and UI tests must use `QStandardPaths::setTestModeEnabled(true)`. No fixed `sleep`s — use `QTRY_*` /
   `QSignalSpy::wait` with a timeout.
5. If you find an unrelated bug while writing tests, don't silently fix it. Document it with `QEXPECT_FAIL`
   and mention it in your report.

### 4.3 How to run and validate

Tests are run as **Qt Test** executables, directly. Don't use `ctest`. Each test binary lands in
`build/Tests/<subdirectory>/` and accepts the standard Qt Test command-line options.

```bash
# Build the test target(s) you touched (with a capped job count, see §3)
ninja -C build -j"$JOBS" testfocus

# Run the whole test binary
./build/Tests/ekos/focus/testfocus

# Verbose output (-v1 / -v2), or log signal emissions (-vs)
./build/Tests/ekos/focus/testfocus -v2

# Run a single test function, or a single data row of a data-driven test
./build/Tests/auxiliary/testksalmanac testDawnDusk -v2
./build/Tests/auxiliary/testksalmanac testDawnDusk:<data-tag>

# List the test functions and data tags in a binary
./build/Tests/ekos/focus/testfocus -functions
./build/Tests/ekos/focus/testfocus -datatags

# Machine-readable results (for reporting)
./build/Tests/ekos/focus/testfocus -o results.xml,xml -o -,txt
```

A binary passes only if it exits with code 0 and prints `Totals: N passed, 0 failed`. Check both. A
`QSKIP`ped function isn't a pass for the behaviour you changed.

Before calling a change done:
1. The new or updated tests pass.
2. For a bug fix, you have seen the regression test **fail without the fix** (stash or revert the fix
   temporarily, run it, then restore).
3. The existing Qt Test binaries for the module you touched still pass. For changes to shared code
   (`auxiliary/`, `skyobjects/`, `fitsviewer/`, `indi/`, Ekos state/sequence classes), run every non-UI
   test binary that depends on it (e.g. all binaries under `build/Tests/auxiliary/`, `build/Tests/ekos/`, …).
4. Run UI tests (`Tests/kstars_ui`) when you changed UI flows they cover and a display is available. If you
   can't run them, say so.
5. Your final report lists the exact commands you ran and their pass/fail results.

**Before blaming or fixing a failure:** confirm it is caused by your change. Temporarily restore the `HEAD`
versions of the files you changed, rebuild, and run the failing function; if it fails the same way, it is
pre-existing. Report it rather than fixing it silently.

**Test pitfalls learned the hard way:**
- Run test binaries from their own build directory (e.g. `build/Tests/kstars_ui`). Some tests find their
  fixtures by relative path and otherwise skip.
- Tests that create widgets (`QTEST_MAIN`) abort with "Authorization required, but no authorization protocol
  specified" when `DISPLAY` is set but the shell has no X authorization, which is common in agent and sandboxed
  shells. `Tests/testhelpers.h` only switches to offscreen when `DISPLAY` is empty, so set it explicitly for
  non-UI binaries: `QT_QPA_PLATFORM=offscreen ./testfoo`. (Full `kstars_ui` tests need a real display; see
  Troubleshooting below.)
- Check the test binary's own exit code. `./testfoo | tail` reports `tail`'s status, so an aborted test looks
  like a pass. Redirect to a log file, then check `$?` and the `Totals:` line.
- Ekos startup in UI tests kills any running `indiserver`. If a real one is running on the machine, run UI
  tests in an isolated environment (separate PID/network namespace or container), never against it.
- Starting Ekos locks the KStars clock to real time, and `SimClock::setUTC()` is ignored in that mode. This
  state leaks between test functions in the same process: keep INDI-simulator tests separate from
  simulated-clock tests, and register one CTest entry per function when simulator tests interfere.
- Time zones: the scheduler honours the UTC offset in ISO timestamps. Build local times with the offset
  the running scheduler uses (`KStarsData::Instance()->geo()->TZ()` after the test sets its location, which
  includes DST), not `GeoLocation::TZ()` of a freshly constructed object (standard time only).

**Troubleshooting:** UI tests need a display and a D-Bus session. Under `QT_QPA_PLATFORM=offscreen`, some
KWidgetsAddons versions crash in `KColorButton`/`KColorMimeData` on a null clipboard `QMimeData` during
KStars startup. If every `kstars_ui` test SIGSEGVs in `TestKStarsStartup::createInstanceTest()`, suspect that
library bug, not your change.

---

## 5. Code style and conventions

- **Formatting is enforced in CI** (`astyle-check` job) with astyle, pinned to the version in `.astyle-version`
  and configured by `.astylerc`: Allman braces, 4-space indent, indented switches/classes/modifiers, padded operators,
  max line length 124, LF line endings. Format only the files you changed:
  ```bash
  bash tools/run_astyle.sh path/to/changed.cpp path/to/changed.h
  bash tools/run_astyle.sh --check --changed-since origin/master   # verify, as CI does
  ```
  Don't reformat untouched files or code.
- **Match surrounding code**: naming (`m_` members, camelCase methods, `Ekos::` namespace for Ekos), comment
  density, and idioms. Prefer Qt types and containers (`QString`, `QList`, `QSharedPointer`, `QPointer`) where
  the surrounding code uses them.
- **License headers**: new files start with an SPDX header (`SPDX-FileCopyrightText` and
  `SPDX-License-Identifier: GPL-2.0-or-later`) like existing files.
- **User-visible strings** go through KI18n (`i18n()`, `i18nc()`, `i18np()`). Don't concatenate translated fragments.
  Use placeholders.
- **Logging**: use the module's `qCDebug/qCInfo/qCWarning(KSTARS_EKOS_<MODULE>)` categories (declared with
  `ecm_qt_declare_logging_category` in `kstars/CMakeLists.txt`), not `qDebug()`.
- **Settings**: add new persistent options to `kstars/kstars.kcfg` and access them via `Options::`. When a UI
  widget is bound to a setting, follow the module's existing sync pattern.
- **D-Bus interfaces** (`org.kde.kstars.*.xml`) are public API used by external scripts and the scheduler.
  Don't rename or remove methods, properties, or signals. Add new ones instead.
- **Threading**: never block the GUI thread with long I/O or image processing (use the existing
  `QtConcurrent`/worker patterns). Never touch widgets from worker threads.
- **Qt compatibility**: Qt 6 is primary. Where Qt 5/6 differ, use the existing `qtcompat.h`,
  `qtskipemptyparts.h`, etc. shims, not raw version checks scattered through the code.
- **Hardware safety**: code that moves or powers equipment (mount, dome, dust cap, flat panel, rotator,
  focuser, cooler) must handle failure, abort, and timeout paths explicitly. Never make it less safe by
  default. In the scheduler, any recovery path (after a weather/safety alert, a sleep or an error) must
  re-evaluate the jobs before running startup tasks that unpark or open equipment, and must still shut
  down when no job can run.

---

## 6. Documentation

- Update the relevant `README.md` when you change behaviour it documents, especially
  `kstars/fitsviewer/pipeline/README.md` (EkosLive `postprocess_*` commands and payloads) and
  `Tests/README.md` (test infrastructure).
- User-facing feature changes may need a handbook update in `kstars/doc/*.docbook` (see README.md, "Making Updates to the Manual").
- Write Doxygen comments for new public classes and methods, matching the style of the header you are editing.

---

## 7. Commits and merge requests (only when asked)

- Commit subject: short, imperative, sentence case, no trailing period. A module prefix is optional
  (`Capture: fix …`, `Fix …`, `Add …`). Explain the *why* in the body when it isn't obvious.
- Never create `GIT_SILENT` commits. They are reserved for automated translation syncs.
- The project is hosted at <https://invent.kde.org/education/kstars> (GitLab). MR descriptions should include
  a summary, the changes per file, notes, and **how to test**, including the automated tests you added or ran.
- Again: **don't open an MR until the maintainer confirms local testing passed.**

### KDE Bugzilla

Bugs are tracked at <https://bugs.kde.org> (product `kstars`). Public bugs can be read through the REST API
without a key:

```bash
curl -s https://bugs.kde.org/rest/bug/<id>              # bug fields
curl -s https://bugs.kde.org/rest/bug/<id>/comment      # comments
curl -s https://bugs.kde.org/rest/bug/<id>/attachment   # attachments (base64 in "data")
```

Commenting, changing status, or attaching files needs an API key (Bugzilla → Preferences → API Keys). Keep it
in a private file outside the repository, never in the tree, commits, or agent memory:

```bash
mkdir -p ~/.config/bugzilla && chmod 700 ~/.config/bugzilla
printf '%s' 'YOUR_KEY' > ~/.config/bugzilla/kde-api-key && chmod 600 ~/.config/bugzilla/kde-api-key
```

bugs.kde.org runs Bugzilla 5.0, which ignores the `X-BUGZILLA-API-KEY` header (and has no `/rest/whoami`).
Pass the key as the `api_key` parameter instead. Let curl read it from the file so it never appears on the command
line or in shell history. For example, to check that the key works:

```bash
curl -sG --data-urlencode "api_key@$HOME/.config/bugzilla/kde-api-key" \
     --data-urlencode "login=<your-bugzilla-email>" https://bugs.kde.org/rest/valid_login   # {"result":true}
```

For `POST`/`PUT` requests, put `api_key` in the JSON body instead.

Anything written to Bugzilla is public. Agents must get the maintainer's approval of the exact text before
posting a comment or changing a bug.

### KDE Webcrash (Sentry)

Crash reports from DrKonqi land in KDE's self-hosted Sentry at <https://crash-reports.kde.org> (organization
`kde`, project `kstars`). Bugzilla bugs can link to a Sentry issue through their `cf_sentryurl` field. The API needs
an auth token (User settings → Personal Tokens). Read-only scopes (`org:read`, `project:read`, `event:read`) are
enough to find crashes and read stack traces. Store it like the Bugzilla key:

```bash
mkdir -p ~/.config/sentry && chmod 700 ~/.config/sentry
printf '%s' 'YOUR_TOKEN' > ~/.config/sentry/kde-auth-token && chmod 600 ~/.config/sentry/kde-auth-token
```

Sentry takes the token as a bearer header. Feed the header to curl from a file so it stays off the command line:

```bash
H() { printf 'Authorization: Bearer %s' "$(cat ~/.config/sentry/kde-auth-token)"; }
curl -s -H @<(H) https://crash-reports.kde.org/api/0/                        # "auth" shows the token's scopes
curl -s -H @<(H) "https://crash-reports.kde.org/api/0/projects/kde/kstars/issues/?query=is:unresolved&statsPeriod=14d"
curl -s -H @<(H) https://crash-reports.kde.org/api/0/issues/<issue-id>/events/latest/   # stack trace
```

Crash events can contain users' paths, hostnames and device names. Don't copy them into commits, MRs or public
bug comments. Changing an issue (resolve, assign, merge) needs the maintainer's approval, like Bugzilla edits.

---

## 8. Checklist before reporting a task as complete

- [ ] The change is minimal, focused, and follows existing patterns.
- [ ] It builds (with a capped job count) and adds no new warnings.
- [ ] Tests were added or updated, **run**, and pass. For a bug fix, the regression test was shown to fail
      without the fix.
- [ ] The existing related Qt Test binaries (all non-UI binaries that depend on shared code) still pass.
- [ ] Changed files are astyle-formatted (`tools/run_astyle.sh --check` is clean for them).
- [ ] New strings are i18n'd, new settings are in `kstars.kcfg`, and D-Bus API compatibility is kept.
- [ ] Docs and READMEs are updated where behaviour changed.
- [ ] The final report lists the changed files, the test commands run with their results, anything skipped
      or unverified, and manual test steps for the maintainer.
