# KStars UI Tests — `test_ekos_scheduler_ops`

This document describes every test function in
`Tests/kstars_ui/test_ekos_scheduler_ops.cpp` and explains what each one
verifies, the scenario it simulates, and the expected outcome.

For the lower-level scheduler **unit** tests (job setup, time estimation,
greedy evaluation — no simulated clock) see
[`Tests/scheduler/README.md`](../scheduler/README.md).

---

## Prerequisites

All tests in this file require:

- KStars fully initialised with solar-system data (checked in `initTestCase()`
  via `findByName("Sun")`).
- An active D-Bus session bus (mock Ekos modules are registered on it).

If solar-system data is absent, `initTestCase()` calls `QSKIP` and the entire
test class is skipped.

## Known issues / workarounds

### `QDateTime::setTimeSpec` warning flood (Qt ≥ 6.8)

Qt 6.8 deprecated `QDateTime::setTimeSpec(Qt::TimeZone)` and emits a runtime
warning every time it is called.  KStars uses this API internally via
`KStarsDateTime`, which is called on every simulated clock tick.  The result
is hundreds of identical `QWARN` lines that drown out the real test output.

**Fix:** `initTestCase()` installs a custom Qt message handler
(`suppressKnownWarnings`) that silently drops any `QtWarningMsg` containing
`"QDateTime::setTimeSpec: Pass a QTimeZone"`.  All other messages pass through
normally.  The underlying KStars source should eventually be updated to use
`QTimeZone` directly.

### Shutdown stall: `SHUTDOWN_PRE_QUEUE_RUNNING` never completes (Qt task queue)

The scheduler's shutdown state machine checks for a pre-shutdown task queue
file (`preShutdownScriptURL`).  In a real installation this file is populated
from `taskqueue/collections/observatory_shutdown.json`.  When the system
templates are present in the test home directory, the scheduler loads the queue
and enters `SHUTDOWN_PRE_QUEUE_RUNNING`, then tries to execute it — but the
queue executor requires a live Ekos profile, which does not exist in unit
tests.  The state machine stalls indefinitely and "Wait for Shutdown" times
out after 50 iterations.

**Fix:** `init()` explicitly clears the pre-shutdown script URL after creating
the `Scheduler` instance:
```cpp
scheduler->process()->moduleState()->setPreShutdownScriptURL(QUrl());
```
This ensures the shutdown code takes the fast path ("No pre-shutdown tasks,
proceeding to stop Ekos/INDI") for all tests that exercise the full shutdown
sequence (`testSimpleJob`, `testTimeZone`, `testArtificialHorizonConstraints`).

---

## Running a single test

Qt Test accepts the test function name as a command-line argument, so you can
run any individual test without waiting for the whole suite:

```bash
./build/Tests/kstars_ui/test_ekos_scheduler_ops testBasics -v2
./build/Tests/kstars_ui/test_ekos_scheduler_ops testSimpleJob -v2
```

All per-test launch configurations are also available in
`.vscode/launch.json` under names like
`Debug test_ekos_scheduler_ops::testBasics`.

---

## Test inventory

### `testBasics`

**What it tests**

1. **D-Bus interface discovery** — Before any mock Ekos modules are registered
   on the session bus, all five scheduler D-Bus interfaces (Focus, Mount,
   Capture, Align, Guide) must be `null`.  After `addModule()` and a brief Qt
   event-loop spin, every interface must become non-null, proving the scheduler
   correctly discovers each module via D-Bus asynchronous notifications.

2. **D-Bus method call round-trip** — `Focus::resetFrame` is called via the
   scheduler's proxy.  The mock object's in-process flag `focuser->isReset` must
   flip to `true`, confirming the call completes end-to-end.

3. **Scheduler idle-shutdown state machine** — With no jobs loaded the scheduler
   must boot and stop cleanly:

   ```
   RUN_WAKEUP → [iter 1] → RUN_SCHEDULER → [iter 2, no jobs] → RUN_NOTHING
   ```

   The sleep interval returned from the second iteration must be **1000 ms**
   (not 0), confirming the scheduler does not busy-spin on an empty job list.

---

### `testSimpleJob`

**Location:** Silicon Valley, CA (UTC−8)  
**Target:** Altair  
**Start time:** 2021-06-13 22:00 UTC (3 pm local)

A full end-to-end simulated observation:

1. Scheduler starts, Altair is well below the 30° altitude constraint.
2. Scheduler sleeps until Altair clears 30° (~11:35 pm local → 06:35 UTC 6/14).
3. Ekos and INDI start; mount slews, plate-solves, focuses, starts guiding.
4. Capture completes → guider aborts → full shutdown → `RUN_NOTHING`.

**Checks:** correct wakeup time; correct slew coordinates for Altair.

---

### `testTimeZone`

Same scenario as `testSimpleJob` but run from **New York City (UTC−5)**.

Altair crosses 30° around the same local time, but ~3 hours earlier UTC
(~03:26 UTC on 6/14).  This test ensures the scheduler's time-zone handling is
correct and results do not accidentally depend on the Silicon Valley timezone.

---

### `testDawnShutdown`

**Location:** Silicon Valley, CA  
**Target:** Altair  
**Start time:** 2021-06-14 10:10 UTC (3:10 am local)

This test exercises the **park-and-sleep** (non-preemptive) shutdown path.
When the next observable window is far away but preemptive shutdown is **off**,
the scheduler should only park the mount and set a wakeup timer — it must *not*
stop Ekos or disconnect INDI.

**Checks:** job start time; interrupt time near dawn; mount parked and scheduler
in `RUN_WAKEUP` state; **Ekos state == `EKOS_READY` and INDI state ==
`INDI_READY`** (confirming Ekos/INDI were not stopped during the sleep period);
correct restart time the following evening; mount unparked on restart.

---

### `testPreemptiveShutdown`

Verifies the **preemptive shutdown + automatic wakeup** cycle with
`Options::preemptiveShutdown(true)` and a 2-hour threshold.

**Checks:** `RUN_WAKEUP` state entered with preemptive flag set; stored wakeup
time is valid; flag cleared after wakeup; Ekos/INDI successfully restarted.

---

### `testPreemptiveShutdownTimerSwitchOnQueueComplete`

**Regression test** for a bug where Ekos was **never stopped** during a
preemptive shutdown when a pre-shutdown task queue was configured.

#### Root cause

When `shouldSchedulerSleep()` decided to do a preemptive shutdown it called
`enablePreemptiveShutdown()` → `setupNextIteration(RUN_WAKEUP)`.  When the
pre-shutdown queue completed, `queueExecutionCompleted()` set
`shutdownState = SHUTDOWN_STOPPING_EKOS` but **did not change the iteration
timer**.  Because `runSchedulerIteration()` only dispatches to
`checkShutdownState()` when the timer is `RUN_SHUTDOWN`, the
`SHUTDOWN_STOPPING_EKOS` state was never processed — Ekos remained running for
the entire sleep period.

#### Fix

`queueExecutionCompleted()` now calls `setupNextIteration(RUN_SHUTDOWN)`
after setting `SHUTDOWN_STOPPING_EKOS`.

**Assertions:**
1. `timerState() == RUN_SHUTDOWN` after `queueExecutionCompleted()`
2. `shutdownState() == SHUTDOWN_STOPPING_EKOS`
3. After one `runSchedulerIteration()`: `schedulerState() == SCHEDULER_IDLE`

---

### `testTwilightStartup`

**Data-driven:** two rows — San Francisco/Rasalhague and Melbourne/Arcturus.

Verifies that the scheduler waits for **astronomical twilight** before
starting a job when the twilight constraint is active.

**Checks:** job starts within ±5 minutes of expected post-twilight time.

---

### `testArtificialHorizonConstraints`

Four sub-scenarios:

1. Raised wakeup time due to 40° horizon at azimuths 100–120°
2. Disabled enforcement restores original wakeup time
3. Empty horizon also restores original wakeup time
4. Job interrupted by horizon at 175–200° / 70° altitude

---

### `testGreedySchedulerRun`

Full two-target simulation (Altair + Deneb) across one night with artificial
horizon.  The greedy scheduler interleaves them by altitude constraints across
four slots.  **Checks:** each slot starts within ±10 minutes of predicted time.

---

### `testRememberJobProgress`

**Data-driven:** six rows.  When `rememberJobProgress = true` the scheduler
skips pre-existing FITS files.  **Checks:** partially-done job → SCHEDULED;
fully-done job → COMPLETE.

---

### `testGreedy`

Pure schedule-prediction unit test (no simulated clock).  Tests FINISH_*
and START_* condition combinations and greedy vs. non-greedy scheduling.

---

### `testMaxMoonAltitude`

Verifies that Dubhe is only scheduled during the window when the Moon is
below 40°.

---

### `testGreedyStartAt`

Two jobs with explicit `START_AT` conditions; verifies both start within
±10 minutes of configured times.

---

### `testGreedyStartAtFinishAt`

Two jobs with `START_AT` + `FINISH_AT` conditions and contiguous windows (1am → 2am, 2am → 3am),
like mosaic tiles. Verifies both are in the plan and that each runs from its start time until its
finish time (bug 528291).

---

### `testGroups`

Three jobs share `group1`; greedy scheduler interleaves them round-robin:
`J1 → J2 → J3 → J2 → J3 → ...`

---

### `testGreedyAborts`

Aborted jobs are delayed by `errorDelay` (1 hour) before rescheduling.
Also verifies `checkJob()` live preemption after the delay expires.

---

### `testArtificialCeiling`

A floor + ceiling horizon band; target only observable between the two bands.
Verifies alternating observable windows across 3 simulated days.

---

### `testSettingAltitudeBug`

Regression: `checkJob()` must not abort a **running** job near its altitude
cutoff.

---

### `testEstimateTimeBug`

Regression: with `rememberJobProgress` and pre-existing files, the predicted
first slot for NGC 2359 must be ~45 minutes (remaining frames only), not a
full session.

---

### `testGreedyMessier`

Stress test with Messier 1–40 catalogue.  Three variants:

1. No horizon, min alt 0° → 39 entries over 2 nights
2. No horizon, min alt 30° → 15 entries
3. Large artificial horizon, min alt 30° → slightly different 15-entry schedule

Skipped if `.esl`/`.esq` fixture files are not found.

---

## Shared helpers

| Helper | Purpose |
|--------|---------|
| `startup()` | Load jobs, start Ekos/INDI, slew, focus, align, guide |
| `slewAndRun()` | Verify job start time; optionally verify interruption time |
| `parkAndSleep()` | Assert mount parks, scheduler enters `RUN_WAKEUP`, Ekos/INDI remain running |
| `wakeupAndRestart()` | Assert scheduler wakes at expected time and mount unparks |
| `startModules()` | Simulate mount tracking, focus, plate-solve, and guiding |
| `iterateScheduler()` | Advance simulated clock and run scheduler iterations until a predicate is true |
| `loadGreedySchedule()` | Write `.esl`/`.esq` files and load them into the scheduler |
| `checkLastSlew()` | Verify most recent mount slew matched expected target coordinates |

---

## Three scheduler sleep paths

```
shouldSchedulerSleep() decision tree
├── PreemptiveShutdown=ON  AND  gap > PreemptiveShutdownTime (default 2 h)
│     → full observatory shutdown + optional Ekos/INDI stop
│       stop() called → timerState = RUN_WAKEUP
│
├── gap > leadTime (5 min)  AND  can park  (preemptive OFF or gap < threshold)
│     → park mount, Ekos/INDI remain running
│       timerState = RUN_WAKEUP
│
└── gap > leadTime, no parking
      → scheduler sleeps, nothing parked
        timerState = RUN_WAKEUP
```
