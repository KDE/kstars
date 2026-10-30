/*
    SPDX-FileCopyrightText: 2026 KStars Team

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/*
 * Unit tests for the task queue's SET action: which property state counts as the device's answer to a SET
 * command, and how a command that the device refuses is retried.
 */

#include "ekos/scheduler/taskqueue/actions/setaction.h"
#include "auxiliary/kspaths.h"
#include "ekos/scheduler/taskqueue/actions/evaluateaction.h"
#include "ekos/scheduler/taskqueue/queue/queueexecutor.h"
#include "ekos/scheduler/taskqueue/queue/queueitem.h"
#include "ekos/scheduler/taskqueue/queue/queuemanager.h"
#include "ekos/scheduler/taskqueue/task.h"
#include "ekos/scheduler/taskqueue/templatemanager.h"
#include "indi/clientmanager.h"
#include "indi/deviceinfo.h"
#include "indi/driverinfo.h"
#include "indi/indilistener.h"
#include "indi/indistd.h"
#include "kstarsdata.h"

#include "../../testhelpers.h"

#include <indipropertyswitch.h>
#include <indipropertytext.h>
#include <parentdevice.h>

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QMetaEnum>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>

#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

using Ekos::SetAction;
using Ekos::TaskAction;

Q_DECLARE_METATYPE(IPState)
Q_DECLARE_METATYPE(ISState)
Q_DECLARE_METATYPE(SetAction::AckResult)

namespace
{
const QString MountName("Test Mount");
// The wait timeout of the park task the scheduler creates (QueueManager::addMountParkTask(), the template's default)
const int ParkWaitTimeout = 60;

// An INDI device that is not connected to an INDI server but reports itself as connected, so that SetAction sends
// properties to it. Sending goes through the real INDI client, which sets the property to Busy and has no server to
// write to. GenericDevice itself only becomes connected through a CONNECTION update, which needs the KStars window.
class ConnectedDevice : public ISD::GenericDevice
{
    public:
        using ISD::GenericDevice::GenericDevice;

        bool isConnected() const override
        {
            return true;
        }
};
}

class TestSetAction : public QObject
{
        Q_OBJECT

    private Q_SLOTS:
        void initTestCase();
        void cleanupTestCase();
        void init();
        void cleanup();

        void testEvaluateAnswer_data();
        void testEvaluateAnswer();
        void testAnswerClassification_data();
        void testAnswerClassification();
        void testRetryAfterRefusal();
        void testSelfBusyNotAccepted_data();
        void testSelfBusyNotAccepted();
        void testMountParkTemplate();
        void testSecondParkNotSentWhileParking_data();
        void testSecondParkNotSentWhileParking();
        void testRefusedFirstParkSendsSecondPark_data();
        void testRefusedFirstParkSendsSecondPark();
        void testQueueStopDuringRetryDelay_data();
        void testQueueStopDuringRetryDelay();

    private:
        SetAction *createParkAction(int retries);
        void deviceAnswers(IPState state);
        void createParkTask(std::unique_ptr<Ekos::Task> &task);
        Ekos::QueueItem *addParkQueueItem(Ekos::QueueManager &manager, int evaluateTimeout);
        void setParkSwitch(ISState park, ISState unpark, IPState state);

        ClientManager m_ClientManager;
        // Owns the device's properties: they are cleared when the last ParentDevice is gone
        std::unique_ptr<INDI::ParentDevice> m_BaseDevice;
        INDI::Property m_Park;
        QSharedPointer<ISD::GenericDevice> m_Device;
};

void TestSetAction::initTestCase()
{
    KTEST_BEGIN();
    // INDI devices follow the KStars clock
    QVERIFY(KStarsData::Create() != nullptr);
}

void TestSetAction::cleanupTestCase()
{
    KTEST_END();
}

void TestSetAction::init()
{
    // TELESCOPE_PARK as a mount driver defines it, the mount is unparked
    m_BaseDevice.reset(new INDI::ParentDevice(INDI::ParentDevice::Valid));
    m_BaseDevice->setDeviceName(MountName.toLatin1().constData());
    INDI::PropertySwitch park(2);
    park[0].fill("PARK", "Park(ed)", ISS_OFF);
    park[1].fill("UNPARK", "UnPark(ed)", ISS_ON);
    park.fill(MountName.toLatin1().constData(), "TELESCOPE_PARK", "Parking", "Main Control", IP_RW, ISR_1OFMANY, 60,
              IPS_IDLE);
    m_BaseDevice->registerProperty(park);
    m_Park = park;
    // Read by the device constructor
    INDI::PropertyText driverInfo(2);
    driverInfo[0].fill("DRIVER_VERSION", "Version", "1.0");
    driverInfo[1].fill("DRIVER_INTERFACE", "Interface", QByteArray::number(INDI::BaseDevice::TELESCOPE_INTERFACE).constData());
    driverInfo.fill(MountName.toLatin1().constData(), "DRIVER_INFO", "Driver Info", "General Info", IP_RO, 60, IPS_IDLE);
    m_BaseDevice->registerProperty(driverInfo);

    QSharedPointer<DriverInfo> driver(new DriverInfo(MountName));
    QSharedPointer<DeviceInfo> deviceInfo(new DeviceInfo(driver, *m_BaseDevice));
    m_Device.reset(new ConnectedDevice(deviceInfo, &m_ClientManager));
    QVERIFY(m_Device->getProperty("TELESCOPE_PARK").isValid());
    QVERIFY(m_Device->getDriverInterface() & INDI::BaseDevice::TELESCOPE_INTERFACE);

    // SetAction finds the device by name through INDIListener, which only creates devices for a running INDI client.
    // Its device list is the only way in without an INDI server.
    auto &devices = const_cast<QList<QSharedPointer<ISD::GenericDevice>> &>(INDIListener::devices());
    devices.append(m_Device);
    QSharedPointer<ISD::GenericDevice> found;
    QVERIFY(INDIListener::findDevice(MountName, found));
}

void TestSetAction::cleanup()
{
    INDIListener::Instance()->removeDevice(MountName);
    m_Device.clear();
    m_Park = INDI::Property();
    m_BaseDevice.reset();
}

SetAction *TestSetAction::createParkAction(int retries)
{
    auto action = new SetAction();
    action->setDevice(MountName);
    action->setProperty("TELESCOPE_PARK");
    action->setElement("PARK");
    action->setValue(true);
    action->setTimeout(30);
    action->setRetries(retries);
    return action;
}

// The driver's answer: an update of TELESCOPE_PARK, delivered the way INDIListener delivers it from the INDI client
void TestSetAction::deviceAnswers(IPState state)
{
    INDI::PropertySwitch park(m_Park);
    if (state == IPS_ALERT)
    {
        // A driver that refuses the park reports the mount as still unparked
        park.reset();
        park[1].setState(ISS_ON);
    }
    park.setState(state);
    INDIListener::Instance()->updateProperty(m_Park);
}

void TestSetAction::testEvaluateAnswer_data()
{
    QTest::addColumn<IPState>("state");
    QTest::addColumn<qint64>("msSinceSend");
    QTest::addColumn<SetAction::AckResult>("expected");

    const qint64 settle = SetAction::ACK_SETTLE_MS;

    // An update right after the send can read the Busy state that the INDI client set itself when sending
    QTest::newRow("BusyRightAfterSend") << IPS_BUSY << qint64(0) << SetAction::AckResult::Wait;
    QTest::newRow("BusyJustBeforeSettle") << IPS_BUSY << settle - 1 << SetAction::AckResult::Wait;
    // A Busy state that lasts for the settle time is the device's answer (e.g. "park in progress")
    QTest::newRow("BusyAtSettle") << IPS_BUSY << settle << SetAction::AckResult::Accepted;
    QTest::newRow("BusyLater") << IPS_BUSY << qint64(5000) << SetAction::AckResult::Accepted;
    // The send leaves the property at Busy, so Ok and Idle can only come from the device
    QTest::newRow("Ok") << IPS_OK << qint64(0) << SetAction::AckResult::Accepted;
    QTest::newRow("Idle") << IPS_IDLE << qint64(0) << SetAction::AckResult::Accepted;
    QTest::newRow("AlertRightAfterSend") << IPS_ALERT << qint64(0) << SetAction::AckResult::Refused;
    QTest::newRow("AlertLater") << IPS_ALERT << qint64(5000) << SetAction::AckResult::Refused;
}

void TestSetAction::testEvaluateAnswer()
{
    QFETCH(IPState, state);
    QFETCH(qint64, msSinceSend);
    QFETCH(SetAction::AckResult, expected);

    QCOMPARE(SetAction::evaluateAnswer(state, msSinceSend, SetAction::ACK_SETTLE_MS), expected);
}

// Property 3, answer classification: for any property state and any time since the send, evaluateAnswer() returns
// Refused for Alert, Accepted for Ok and Idle, and for Busy Wait below the settle time and Accepted from the settle
// time on. Every state at the times around the settle time, for the settle time of the action, for 0 (passed by the
// timeout, which takes a Busy state as the answer) and for a shorter one, so the rule is checked as a function of the
// settle time.
// Validates: Requirements 2.1, 2.2, 2.3
void TestSetAction::testAnswerClassification_data()
{
    QTest::addColumn<IPState>("state");
    QTest::addColumn<qint64>("msSinceSend");
    QTest::addColumn<qint64>("settleMs");
    QTest::addColumn<SetAction::AckResult>("expected");

    // IPState has no count or sentinel value, these are all its values (indiapi.h: IPS_IDLE = 0, IPS_OK, IPS_BUSY,
    // IPS_ALERT)
    const QList<QPair<IPState, QString>> states =
    {
        { IPS_IDLE, "Idle" },
        { IPS_OK, "Ok" },
        { IPS_BUSY, "Busy" },
        { IPS_ALERT, "Alert" },
    };

    // The property as stated above, not taken from the implementation
    auto expectedAnswer = [](IPState state, qint64 msSinceSend, qint64 settleMs)
    {
        if (state == IPS_ALERT)
            return SetAction::AckResult::Refused;
        if (state == IPS_OK || state == IPS_IDLE)
            return SetAction::AckResult::Accepted;
        return (msSinceSend >= settleMs) ? SetAction::AckResult::Accepted : SetAction::AckResult::Wait;
    };

    auto sortUnique = [](std::vector<qint64> &values)
    {
        std::sort(values.begin(), values.end());
        values.erase(std::unique(values.begin(), values.end()), values.end());
    };

    std::vector<qint64> settleValues = { SetAction::ACK_SETTLE_MS, 0, 500 };
    sortUnique(settleValues);

    for (const qint64 settle : settleValues)
    {
        std::vector<qint64> times = { 0, 1, 500, settle - 1000, settle - 1, settle, settle + 1, settle + 1000, 2 * settle,
                                      60000, std::numeric_limits<qint32>::max()
                                    };
        // No times before the send
        times.erase(std::remove_if(times.begin(), times.end(), [](qint64 ms)
        {
            return ms < 0;
        }), times.end());
        sortUnique(times);

        for (const auto &state : states)
            for (const qint64 ms : times)
                QTest::newRow(qPrintable(QString("%1/settle%2/%3ms").arg(state.second).arg(settle).arg(ms)))
                        << state.first << ms << settle << expectedAnswer(state.first, ms, settle);
    }
}

void TestSetAction::testAnswerClassification()
{
    QFETCH(IPState, state);
    QFETCH(qint64, msSinceSend);
    QFETCH(qint64, settleMs);
    QFETCH(SetAction::AckResult, expected);

    const SetAction::AckResult answer = SetAction::evaluateAnswer(state, msSinceSend, settleMs);
    QCOMPARE(answer, expected);
    // Depends on its arguments only: the same call gives the same answer
    QCOMPARE(SetAction::evaluateAnswer(state, msSinceSend, settleMs), answer);
}

// The device refuses every attempt, as EQMod refuses a park while the mount slews ("Can not park while slewing").
// The action sends again only after the retry delay, and fails after the last retry with an error that names the
// device, property and element.
void TestSetAction::testRetryAfterRefusal()
{
    QElapsedTimer clock;
    QList<qint64> sends, refusals;
    std::unique_ptr<SetAction> action(createParkAction(2));
    QSignalSpy progressSpy(action.get(), &TaskAction::progress);
    QSignalSpy completedSpy(action.get(), &TaskAction::completed);
    QSignalSpy failedSpy(action.get(), &TaskAction::failed);

    // started() is emitted right before each send, the device answers after the send
    connect(action.get(), &TaskAction::started, action.get(), [&]()
    {
        sends.append(clock.elapsed());
        QTimer::singleShot(0, action.get(), [&]()
        {
            refusals.append(clock.elapsed());
            deviceAnswers(IPS_ALERT);
        });
    });

    clock.start();
    QVERIFY(action->start());
    QTRY_COMPARE_WITH_TIMEOUT(failedSpy.count(), 1, 3 * SetAction::RETRY_DELAY_MS + 5000);

    // First attempt and two retries, each one refused
    QCOMPARE(sends.size(), 3);
    QCOMPARE(refusals.size(), 3);
    // Qt's default (coarse) timers may fire up to 5 % early
    const qint64 minDelay = SetAction::RETRY_DELAY_MS * 95 / 100;
    for (int i = 1; i < sends.size(); i++)
    {
        const qint64 delay = sends[i] - refusals[i - 1];
        QVERIFY2(delay >= minDelay, qPrintable(QString("retry %1 was sent %2 ms after the refusal").arg(i).arg(delay)));
    }

    QStringList messages;
    for (const auto &args : std::as_const(progressSpy))
        messages << args.at(0).toString();
    QVERIFY2(messages.contains("Property in Alert state, retry 1/2"), qPrintable(messages.join(" | ")));
    QVERIFY2(messages.contains("Property in Alert state, retry 2/2"), qPrintable(messages.join(" | ")));

    const QString error = failedSpy.at(0).at(0).toString();
    QVERIFY2(error.contains(QString("%1.TELESCOPE_PARK.PARK").arg(MountName)), qPrintable(error));
    QCOMPARE(action->errorMessage(), error);
    QCOMPARE(action->status(), TaskAction::FAILED);
    QCOMPARE(completedSpy.count(), 0);
}

void TestSetAction::testSelfBusyNotAccepted_data()
{
    QTest::addColumn<IPState>("deviceAnswer");

    QTest::newRow("DeviceOk") << IPS_OK;
    QTest::newRow("DeviceIdle") << IPS_IDLE;
    QTest::newRow("DeviceBusy") << IPS_BUSY;
}

// Right after the send an update arrives that reads the client's own Busy state. When the device then answers, the
// action completes: at once for Ok or Idle, from the settle time on for Busy.
void TestSetAction::testSelfBusyNotAccepted()
{
    QFETCH(IPState, deviceAnswer);

    QElapsedTimer clock;
    IPState stateAfterSend = IPS_IDLE;
    std::unique_ptr<SetAction> action(createParkAction(0));
    QSignalSpy completedSpy(action.get(), &TaskAction::completed);
    QSignalSpy failedSpy(action.get(), &TaskAction::failed);

    // As in a queued update that the driver sent before the command and that is delivered after the send
    connect(action.get(), &TaskAction::started, action.get(), [&]()
    {
        QTimer::singleShot(0, action.get(), [&]()
        {
            stateAfterSend = m_Park.getState();
            INDIListener::Instance()->updateProperty(m_Park);
        });
    });

    clock.start();
    QVERIFY(action->start());
    // Neither this update nor the polling (every second) takes the client's own Busy for an answer
    QVERIFY(!completedSpy.wait(SetAction::ACK_SETTLE_MS * 3 / 4));
    QCOMPARE(stateAfterSend, IPS_BUSY);
    QCOMPARE(action->status(), TaskAction::RUNNING);

    deviceAnswers(deviceAnswer);
    if (deviceAnswer == IPS_BUSY)
    {
        // A Busy answer ("park in progress") counts once the settle time has passed, through the polling
        QTRY_COMPARE_WITH_TIMEOUT(completedSpy.count(), 1, SetAction::ACK_SETTLE_MS + 3000);
        QVERIFY2(clock.elapsed() >= SetAction::ACK_SETTLE_MS, qPrintable(QString::number(clock.elapsed())));
    }
    else
    {
        // Ok and Idle complete the action in the update itself
        QCOMPARE(completedSpy.count(), 1);
    }
    QCOMPARE(action->status(), TaskAction::COMPLETED);
    QCOMPARE(failedSpy.count(), 0);
}

namespace
{
template <typename T>
QString enumKey(T value)
{
    return QString::fromLatin1(QMetaEnum::fromType<T>().valueToKey(static_cast<int>(value)));
}

// One line per action of a park task, with everything the task's behaviour depends on
QString describeAction(TaskAction *action)
{
    QString what = action->typeString();
    if (auto set = qobject_cast<SetAction *>(action))
        what += QString(" %1.%2.%3 = %4 (%5), %6").arg(set->device(), set->property(), set->element(),
                set->value().toString(), QString::fromLatin1(set->value().typeName()),
                set->waitForCompletion() ? QString("wait") : QString("no wait"));
    else if (auto evaluate = qobject_cast<Ekos::EvaluateAction *>(action))
        what += QString(" %1.%2.%3 (%4) %5 %6").arg(evaluate->device(), evaluate->property(), evaluate->element(),
                enumKey(evaluate->propertyType()), enumKey(evaluate->condition()), evaluate->target().toString());

    return QString("%1, timeout %2 s, retries %3, on failure %4").arg(what).arg(action->timeout())
           .arg(action->retries()).arg(enumKey(action->failureAction()));
}
}

// The park task as the scheduler creates it: QueueManager::createTaskFromTemplate() instantiates the built-in
// mount_park template from the template manager without a device, and the queue executor then assigns the mount
void TestSetAction::createParkTask(std::unique_ptr<Ekos::Task> &task)
{
    Ekos::TemplateManager *manager = Ekos::TemplateManager::Instance();
    // Nothing else in this test uses the singleton: release it, so that each test loads the templates again
    struct Release
    {
        ~Release()
        {
            Ekos::TemplateManager::release();
        }
    } release;

    QVERIFY(manager->initialize());
    Ekos::TaskTemplate *tmpl = manager->getTemplate("mount_park");
    QVERIFY(tmpl != nullptr);
    QVERIFY(tmpl->isSystemTemplate());

    QMap<QString, QVariant> parameters;
    parameters["wait_timeout"] = ParkWaitTimeout;
    task.reset(new Ekos::Task());
    QVERIFY2(task->instantiateFromTemplate(tmpl, QString(), parameters), qPrintable(task->errorMessage()));
    task->setDevice(MountName);
}

// TELESCOPE_PARK as the client knows it after the driver's last update
void TestSetAction::setParkSwitch(ISState park, ISState unpark, IPState state)
{
    INDI::PropertySwitch property(m_Park);
    property[0].setState(park);
    property[1].setState(unpark);
    property.setState(state);
}

// The built-in mount_park template, loaded as the scheduler loads it: park, wait once for the mount to report parked,
// send the park again (skipped while the PARK switch is on, see testSecondParkNotSentWhileParking), wait again. Only
// the last wait fails the task: a refused first park goes on to the wait and the second park
// (see testRefusedFirstParkSendsSecondPark).
// Validates: Requirements 3.1, 3.4
void TestSetAction::testMountParkTemplate()
{
    // The template manager reads the template of this source tree (installTestData() links the source data
    // directory into the test's data location), not an installed copy
    const QString systemDir = KSPaths::locate(QStandardPaths::AppDataLocation, "taskqueue/templates/system",
                              QStandardPaths::LocateDirectory);
    const QString loadedFile = QFileInfo(QDir(systemDir).filePath("mount.json")).canonicalFilePath();
    const QString sourceFile = QFileInfo(QDir(KTest::kstarsDataDir()).filePath("taskqueue/templates/system/mount.json"))
                               .canonicalFilePath();
    qDebug() << "System templates:" << systemDir << "- mount.json:" << loadedFile;
    QVERIFY(!sourceFile.isEmpty());
    QCOMPARE(loadedFile, sourceFile);

    std::unique_ptr<Ekos::Task> task;
    createParkTask(task);
    if (QTest::currentTestFailed())
        return;

    const QString setPark = QString("SET %1.TELESCOPE_PARK.PARK = true (bool), wait, timeout %2 s, retries 2, "
                                    "on failure %3").arg(MountName).arg(ParkWaitTimeout);
    const QString waitParked = QString("EVALUATE %1.TELESCOPE_PARK.STATE (STATE) EQUALS OK, timeout %2 s, retries %3, "
                                       "on failure %4").arg(MountName).arg(ParkWaitTimeout);
    const QStringList expected =
    {
        // If the mount refuses the park (e.g. still slewing) after all retries, go on to the wait and the second park
        setPark.arg("CONTINUE"),
        // A single wait. If the mount didn't report parked, go on and send the park again
        waitParked.arg(0).arg("CONTINUE"),
        setPark.arg("ABORT_QUEUE"),
        waitParked.arg(2).arg("ABORT_QUEUE"),
    };

    QStringList actual;
    for (TaskAction *action : task->actions())
        actual << describeAction(action);
    qDebug() << "mount_park actions:" << actual;

    QCOMPARE(actual.size(), expected.size());
    for (int i = 0; i < expected.size(); i++)
        QVERIFY2(actual[i] == expected[i], qPrintable(QString("action %1: %2, expected: %3").arg(i + 1)
                 .arg(actual[i], expected[i])));
}

void TestSetAction::testSecondParkNotSentWhileParking_data()
{
    QTest::addColumn<ISState>("parkSwitch");
    QTest::addColumn<ISState>("unparkSwitch");
    QTest::addColumn<IPState>("state");

    // TELESCOPE_PARK is one of many: PARK on (parking or parked), UNPARK on (unparked, or a park was refused), or
    // both off (a park was cancelled or aborted)
    struct Switches
    {
        QString name;
        ISState park;
        ISState unpark;
    };
    const QList<Switches> switches =
    {
        { "ParkOn", ISS_ON, ISS_OFF },
        { "UnparkOn", ISS_OFF, ISS_ON },
        { "AllOff", ISS_OFF, ISS_OFF },
    };
    // All values of IPState (indiapi.h)
    const QList<QPair<IPState, QString>> states =
    {
        { IPS_IDLE, "Idle" },
        { IPS_OK, "Ok" },
        { IPS_BUSY, "Busy" },
        { IPS_ALERT, "Alert" },
    };

    for (const auto &switchState : switches)
        for (const auto &state : states)
            QTest::newRow(qPrintable(QString("%1/%2").arg(switchState.name, state.second)))
                    << switchState.park << switchState.unpark << state.first;
}

// Property 4, no park cancel: for any state of the PARK switch, the park task sends PARK only while the switch is
// off, so it never turns a park in progress into a cancel (an INDI mount driver takes PARK during parking as a
// cancel). Every switch combination a driver reports, with every property state, for each SET action of the park
// task as the scheduler creates it: action 3, the second park, and action 1.
// Validates: Requirements 3.1, 3.4
void TestSetAction::testSecondParkNotSentWhileParking()
{
    QFETCH(ISState, parkSwitch);
    QFETCH(ISState, unparkSwitch);
    QFETCH(IPState, state);

    // The property as stated above, not taken from the implementation
    const bool expectSend = (parkSwitch == ISS_OFF);

    std::unique_ptr<Ekos::Task> task;
    createParkTask(task);
    if (QTest::currentTestFailed())
        return;

    int checked = 0;
    for (int i = 0; i < task->actionCount(); i++)
    {
        auto action = qobject_cast<SetAction *>(task->actions().at(i));
        if (action == nullptr)
            continue;
        checked++;
        const QString context = QString("action %1").arg(i + 1);
        QVERIFY2(action->property() == "TELESCOPE_PARK" && action->element() == "PARK" && action->value().toBool(),
                 qPrintable(context + ": " + describeAction(action)));

        setParkSwitch(parkSwitch, unparkSwitch, state);
        QSignalSpy startedSpy(action, &TaskAction::started);
        QSignalSpy completedSpy(action, &TaskAction::completed);
        QSignalSpy progressSpy(action, &TaskAction::progress);
        QVERIFY2(action->start(), qPrintable(context));

        INDI::PropertySwitch park(m_Park);
        const QString result = QString("%1: started %2, completed %3, skipped %4, status %5, PARK %6, UNPARK %7, "
                                       "state %8").arg(context).arg(startedSpy.count()).arg(completedSpy.count())
                               .arg(action->wasSkipped()).arg(action->statusString()).arg(park[0].getState())
                               .arg(park[1].getState()).arg(park.getState());
        if (expectSend)
        {
            // Sent: started() comes right before the send, and the client sets PARK on and Busy when it sends
            QVERIFY2(startedSpy.count() == 1 && completedSpy.isEmpty(), qPrintable(result));
            QVERIFY2(!action->wasSkipped() && action->status() == TaskAction::RUNNING, qPrintable(result));
            QVERIFY2(park[0].getState() == ISS_ON && park[1].getState() == ISS_OFF && park.getState() == IPS_BUSY,
                     qPrintable(result));
            // No device answers: stop waiting
            action->abort();
        }
        else
        {
            // Skipped: completed at once, nothing sent, the property stays as the driver reported it
            QVERIFY2(startedSpy.isEmpty() && completedSpy.count() == 1, qPrintable(result));
            QVERIFY2(action->wasSkipped() && action->status() == TaskAction::COMPLETED, qPrintable(result));
            QVERIFY2(park[0].getState() == parkSwitch && park[1].getState() == unparkSwitch && park.getState() == state,
                     qPrintable(result));
            QStringList messages;
            for (const auto &args : std::as_const(progressSpy))
                messages << args.at(0).toString();
            QVERIFY2(messages.filter("already at target value, skipping").size() == 1, qPrintable(messages.join(" | ")));
        }
    }
    QVERIFY(checked > 0);
}

// The park task of createParkTask() as a queue item that the queue executor runs without an Ekos profile. The
// executor asks the Ekos manager for a running profile only if a task has a device or a template with device
// interfaces, so the item's task has neither, and each action keeps the mount's name. The waits time out after
// evaluateTimeout seconds instead of the template's, to keep the test short. Returns nullptr on failure.
Ekos::QueueItem *TestSetAction::addParkQueueItem(Ekos::QueueManager &manager, int evaluateTimeout)
{
    std::unique_ptr<Ekos::Task> templateTask;
    createParkTask(templateTask);
    if (QTest::currentTestFailed() || !templateTask)
        return nullptr;

    QJsonObject json = templateTask->toJson();
    json["template_id"] = QString();
    json["device"] = QString();
    QJsonArray actions = json["actions"].toArray();
    for (int i = 0; i < actions.size(); i++)
    {
        QJsonObject action = actions[i].toObject();
        if (action["type"].toString() == "EVALUATE")
            action["timeout"] = evaluateTimeout;
        actions[i] = action;
    }
    json["actions"] = actions;

    auto task = new Ekos::Task();
    if (!task->loadFromJson(json))
    {
        delete task;
        return nullptr;
    }
    // The item owns the task, the manager the item
    auto item = new Ekos::QueueItem(task);
    manager.addItem(item);
    return item;
}

void TestSetAction::testRefusedFirstParkSendsSecondPark_data()
{
    // The mount accepts the second park, or refuses that one too
    QTest::addColumn<bool>("secondParkAccepted");

    QTest::newRow("SecondParkAccepted") << true;
    QTest::newRow("AllParksRefused") << false;
}

// The mount refuses the first park and all its retries, as EQMod does while a long slew runs ("Can not park while
// slewing"). The park task must go on to the wait and send the second park, and the queue completes when the mount
// accepts that one. If the first park failed the queue, the second park would never be sent and the tasks after the
// park (dome, dust cap) would be skipped. A park that never succeeds must still fail the queue, so that the
// scheduler reports the failed shutdown.
// Validates: Requirement 3.1
void TestSetAction::testRefusedFirstParkSendsSecondPark()
{
    QFETCH(bool, secondParkAccepted);

    // The queue executor creates the template manager singleton
    struct Release
    {
        ~Release()
        {
            Ekos::TemplateManager::release();
        }
    } release;

    Ekos::QueueManager manager;
    Ekos::QueueItem *item = addParkQueueItem(manager, 1);
    if (QTest::currentTestFailed())
        return;
    QVERIFY(item != nullptr);
    const QList<TaskAction *> actions = item->task()->actions();
    QCOMPARE(actions.size(), 4);
    auto firstPark = qobject_cast<SetAction *>(actions.at(0));
    auto secondPark = qobject_cast<SetAction *>(actions.at(2));
    QVERIFY(firstPark != nullptr && secondPark != nullptr);

    // The mount refuses every attempt of the first park, and accepts or refuses the second one
    int firstParkSends = 0, secondParkSends = 0;
    connect(firstPark, &TaskAction::started, this, [&]()
    {
        firstParkSends++;
        QTimer::singleShot(0, this, [this]()
        {
            deviceAnswers(IPS_ALERT);
        });
    });
    const IPState secondParkAnswer = secondParkAccepted ? IPS_OK : IPS_ALERT;
    connect(secondPark, &TaskAction::started, this, [&]()
    {
        secondParkSends++;
        QTimer::singleShot(0, this, [this, secondParkAnswer]()
        {
            deviceAnswers(secondParkAnswer);
        });
    });

    Ekos::QueueExecutor executor(&manager);
    QSignalSpy completedSpy(&executor, &Ekos::QueueExecutor::completed);
    QSignalSpy failedSpy(&executor, &Ekos::QueueExecutor::itemFailed);
    QSignalSpy abortedSpy(&executor, &Ekos::QueueExecutor::aborted);
    QVERIFY(executor.start());

    // Three refused attempts with the retry delay in between, the first wait times out after 1 s, the second park
    // (three more refused attempts if the mount refuses that one too)
    QTRY_VERIFY_WITH_TIMEOUT(!completedSpy.isEmpty() || !abortedSpy.isEmpty(), 6 * SetAction::RETRY_DELAY_MS + 20000);
    QStringList errors;
    for (const auto &args : std::as_const(failedSpy))
        errors << args.at(1).toString();
    QCOMPARE(firstParkSends, 3);
    QCOMPARE(firstPark->status(), TaskAction::FAILED);
    INDI::PropertySwitch park(m_Park);

    if (secondParkAccepted)
    {
        QVERIFY2(failedSpy.isEmpty() && abortedSpy.isEmpty(),
                 qPrintable(QString("The queue failed: %1").arg(errors.join(" | "))));
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(secondParkSends, 1);
        QCOMPARE(secondPark->status(), TaskAction::COMPLETED);
        QCOMPARE(item->status(), Ekos::QueueItem::COMPLETED);
        QVERIFY(park[0].getState() == ISS_ON && park[1].getState() == ISS_OFF && park.getState() == IPS_OK);
    }
    else
    {
        // The second park fails the task and the queue, with the mount's refusal as the error
        QVERIFY2(failedSpy.count() == 1 && abortedSpy.count() == 1 && completedSpy.isEmpty(),
                 qPrintable(QString("failed %1, aborted %2, completed %3").arg(failedSpy.count())
                            .arg(abortedSpy.count()).arg(completedSpy.count())));
        QVERIFY2(errors.at(0).contains("was refused by the device"), qPrintable(errors.at(0)));
        QCOMPARE(secondParkSends, 3);
        QCOMPARE(secondPark->status(), TaskAction::FAILED);
        QCOMPARE(item->status(), Ekos::QueueItem::FAILED);
        QVERIFY(park[0].getState() == ISS_OFF && park[1].getState() == ISS_ON && park.getState() == IPS_ALERT);
    }
}

void TestSetAction::testQueueStopDuringRetryDelay_data()
{
    // Stop the queue with QueueExecutor::stop() (the queue viewer's Stop button), or abort it
    QTest::addColumn<bool>("abortQueue");

    QTest::newRow("Stop") << false;
    QTest::newRow("Abort") << true;
}

// The mount refuses the first park, and the SET action waits the retry delay before it sends the park again. The
// queue is stopped or aborted during that delay: the running action must be aborted with it, so that no park is
// sent after the queue was given up.
void TestSetAction::testQueueStopDuringRetryDelay()
{
    QFETCH(bool, abortQueue);

    // The queue executor creates the template manager singleton
    struct Release
    {
        ~Release()
        {
            Ekos::TemplateManager::release();
        }
    } release;

    Ekos::QueueManager manager;
    Ekos::QueueItem *item = addParkQueueItem(manager, 1);
    if (QTest::currentTestFailed())
        return;
    QVERIFY(item != nullptr);
    const QList<TaskAction *> actions = item->task()->actions();
    QCOMPARE(actions.size(), 4);
    auto firstPark = qobject_cast<SetAction *>(actions.at(0));
    QVERIFY(firstPark != nullptr);

    // Every SET of the task is a park: count all sends. The mount refuses the first one.
    QList<QSharedPointer<QSignalSpy>> startedSpies;
    for (TaskAction *action : actions)
        startedSpies << QSharedPointer<QSignalSpy>(new QSignalSpy(action, &TaskAction::started));
    auto sends = [&]()
    {
        int count = 0;
        for (const auto &spy : std::as_const(startedSpies))
            count += spy->count();
        return count;
    };
    bool refused = false;
    connect(firstPark, &TaskAction::started, this, [&]()
    {
        QTimer::singleShot(0, this, [&]()
        {
            deviceAnswers(IPS_ALERT);
            refused = true;
        });
    });

    Ekos::QueueExecutor executor(&manager);
    QSignalSpy stoppedSpy(&executor, &Ekos::QueueExecutor::stopped);
    QSignalSpy abortedSpy(&executor, &Ekos::QueueExecutor::aborted);
    QSignalSpy failedSpy(&executor, &Ekos::QueueExecutor::itemFailed);
    QSignalSpy completedSpy(&executor, &Ekos::QueueExecutor::completed);
    QVERIFY(executor.start());
    QTRY_VERIFY_WITH_TIMEOUT(refused, 5000);
    QCOMPARE(sends(), 1);
    // Waiting for the retry
    QCOMPARE(firstPark->status(), TaskAction::RUNNING);

    if (abortQueue)
        executor.abort();
    else
        executor.stop();
    QVERIFY(!executor.isRunning());
    QCOMPARE(stoppedSpy.count(), 1);
    QCOMPARE(abortedSpy.count(), abortQueue ? 1 : 0);

    // Twice the retry delay: nothing is sent, the mount stays as it reported itself after the refusal
    QVERIFY2(!startedSpies.first()->wait(2 * SetAction::RETRY_DELAY_MS),
             "The park was sent again after the queue was stopped");
    QCOMPARE(sends(), 1);
    INDI::PropertySwitch park(m_Park);
    QVERIFY(park[0].getState() == ISS_OFF && park[1].getState() == ISS_ON && park.getState() == IPS_ALERT);
    // The action was aborted with the queue, and its abort didn't fail the item
    QCOMPARE(firstPark->status(), TaskAction::ABORTED);
    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(completedSpy.count(), 0);
    QCOMPARE(stoppedSpy.count(), 1);
    QCOMPARE(abortedSpy.count(), abortQueue ? 1 : 0);
}

// Not QTEST_GUILESS_MAIN: KStarsData, which the INDI devices need, creates pixmaps for its color scheme
QTEST_MAIN(TestSetAction)

#include "testsetaction.moc"
