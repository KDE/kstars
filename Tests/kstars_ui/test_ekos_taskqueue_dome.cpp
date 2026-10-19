/*  KStars task queue dome tests with the INDI simulators
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// These tests start Ekos with a dome simulator and run the default observatory startup
// and shutdown collections through the task queue. Dome shutter tasks declare
// DOME_SHUTTER as a required property, so they must run on a dome with a shutter and be
// skipped on one without. Each test must run in a process of its own, as each starts a
// different profile. CMake registers one test per function for this.

#include "../testhelpers.h"
#include "test_ekos_taskqueue_dome.h"

#if defined(HAVE_INDI)

#include "kstars_ui_tests.h"
#include "test_ekos.h"
#include "test_ekos_helper.h"
#include "test_ekos_simulator.h"
#include "ekos/scheduler/taskqueue/queue/queueexecutor.h"
#include "ekos/scheduler/taskqueue/queue/queuemanager.h"
#include "ekos/scheduler/taskqueue/task.h"
#include "indi/indilistener.h"
#include "kspaths.h"
#include "Options.h"

namespace
{

// Run an installed observatory collection and wait until the queue finishes
bool runCollection(const QString &file, Ekos::QueueManager &manager)
{
    const QString path = KSPaths::locate(QStandardPaths::AppDataLocation, "taskqueue/collections/" + file);
    KVERIFY2_SUB(!path.isEmpty(), qPrintable(QString("%1 not found in KStars data directory").arg(file)));
    KVERIFY_SUB(manager.loadQueue(path));

    Ekos::QueueExecutor executor(&manager);
    QSignalSpy completed(&executor, &Ekos::QueueExecutor::completed);
    QSignalSpy aborted(&executor, &Ekos::QueueExecutor::aborted);
    KVERIFY_SUB(executor.start());

    // Timeouts in the collections add up to a few minutes, the simulators take seconds
    KTRY_VERIFY_WITH_TIMEOUT_SUB(completed.count() > 0 || aborted.count() > 0, 180000);
    KVERIFY2_SUB(aborted.isEmpty(), qPrintable(QString("%1 aborted").arg(file)));
    return true;
}

// Status of the queue item created from a template, or -1 if there is none
int statusOf(const Ekos::QueueManager &manager, const QString &templateId)
{
    for (Ekos::QueueItem *item : manager.items())
    {
        if (item->task() && item->task()->templateId() == templateId)
            return item->status();
    }
    return -1;
}

bool switchIsOn(const QString &device, const char *property, const char *element)
{
    QSharedPointer<ISD::GenericDevice> genericDevice;
    if (!INDIListener::findDevice(device, genericDevice))
        return false;

    auto prop = genericDevice->getProperty(property);
    if (!prop.isValid() || prop.getType() != INDI_SWITCH)
        return false;

    auto sw = prop.getSwitch()->findWidgetByName(element);
    return sw && sw->getState() == ISS_ON;
}

}

// Start a profile with a camera and a dome only. Without a mount, the mount tasks of the
// collections are skipped, so KTRY_EKOS_START_PROFILE (which waits for the mount module)
// is not used.
#define KTRY_START_DOME_PROFILE(profile, dome) do { \
    TestEkosHelper helper; \
    helper.m_CCDDevice = "CCD Simulator"; \
    helper.m_DomeDevice = dome; \
    QVERIFY(helper.setupEkosProfile(profile, false)); \
    KTRY_EKOS_SELECT_PROFILE(profile); \
    KTRY_EKOS_CLICK(processINDIB); \
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->indiStatus() == Ekos::Success, 30000); \
    QSharedPointer<ISD::GenericDevice> started; \
    QTRY_VERIFY_WITH_TIMEOUT(INDIListener::findDevice(dome, started) && started->isConnected() && \
                             started->getProperty("DOME_PARK").isValid(), 30000); } while(false)

TestEkosTaskQueueDome::TestEkosTaskQueueDome(QObject *parent) : QObject(parent)
{
}

void TestEkosTaskQueueDome::init()
{
    // Keep the shutdown collection limited to its own tasks
    Options::setCameraWarmupEnabled(false);
}

void TestEkosTaskQueueDome::testDomeWithShutterSimulator()
{
    const QString dome = "Dome Simulator";

    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();
    KTRY_START_DOME_PROFILE("Simulators+Dome", dome);

    QSharedPointer<ISD::GenericDevice> device;
    QVERIFY(INDIListener::findDevice(dome, device));
    QTRY_VERIFY_WITH_TIMEOUT(device->getProperty("DOME_SHUTTER").isValid(), 10000);

    // Startup: unpark and open the shutter
    {
        Ekos::QueueManager manager;
        QVERIFY(runCollection("observatory_startup.json", manager));
        QCOMPARE(statusOf(manager, "dome_unpark"), static_cast<int>(Ekos::QueueItem::COMPLETED));
        QCOMPARE(statusOf(manager, "dome_shutter_open"), static_cast<int>(Ekos::QueueItem::COMPLETED));
        QVERIFY(switchIsOn(dome, "DOME_PARK", "UNPARK"));
        QVERIFY(switchIsOn(dome, "DOME_SHUTTER", "SHUTTER_OPEN"));
        QCOMPARE(device->getProperty("DOME_SHUTTER").getState(), IPS_OK);
    }

    // Shutdown: close the shutter and park
    {
        Ekos::QueueManager manager;
        QVERIFY(runCollection("observatory_shutdown.json", manager));
        QCOMPARE(statusOf(manager, "dome_shutter_close"), static_cast<int>(Ekos::QueueItem::COMPLETED));
        QCOMPARE(statusOf(manager, "dome_park"), static_cast<int>(Ekos::QueueItem::COMPLETED));
        QVERIFY(switchIsOn(dome, "DOME_SHUTTER", "SHUTTER_CLOSE"));
        QCOMPARE(device->getProperty("DOME_SHUTTER").getState(), IPS_OK);
        QVERIFY(switchIsOn(dome, "DOME_PARK", "PARK"));
    }

    KTRY_EKOS_STOP_SIMULATORS();
    KTRY_CLOSE_EKOS();
}

void TestEkosTaskQueueDome::testRollOffWithoutShutterSimulator()
{
    const QString dome = "RollOff Simulator";

    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();
    KTRY_START_DOME_PROFILE("Simulators+RollOff", dome);

    QSharedPointer<ISD::GenericDevice> device;
    QVERIFY(INDIListener::findDevice(dome, device));
    QVERIFY(!device->getProperty("DOME_SHUTTER").isValid());

    // Startup: the roof is unparked, the shutter task does not apply
    {
        Ekos::QueueManager manager;
        QVERIFY(runCollection("observatory_startup.json", manager));
        QCOMPARE(statusOf(manager, "dome_unpark"), static_cast<int>(Ekos::QueueItem::COMPLETED));
        QCOMPARE(statusOf(manager, "dome_shutter_open"), static_cast<int>(Ekos::QueueItem::SKIPPED));
        QVERIFY(switchIsOn(dome, "DOME_PARK", "UNPARK"));
    }

    // Shutdown: the shutter task does not apply, the roof is still parked
    {
        Ekos::QueueManager manager;
        QVERIFY(runCollection("observatory_shutdown.json", manager));
        QCOMPARE(statusOf(manager, "dome_shutter_close"), static_cast<int>(Ekos::QueueItem::SKIPPED));
        QCOMPARE(statusOf(manager, "dome_park"), static_cast<int>(Ekos::QueueItem::COMPLETED));
        QVERIFY(switchIsOn(dome, "DOME_PARK", "PARK"));
    }

    KTRY_EKOS_STOP_SIMULATORS();
    KTRY_CLOSE_EKOS();
}

QTEST_KSTARS_MAIN(TestEkosTaskQueueDome)

#endif // HAVE_INDI
