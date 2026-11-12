/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ekos/capture/camerastate.h"
#include "ekos/capture/capturedeviceadaptor.h"
#include "ekos/capture/refocusstate.h"
#include "ekos/capture/sequencejob.h"
#include "kstarsdata.h"
#include "Options.h"

#include "../../testhelpers.h"

#include <QRegularExpression>
#include <QTest>

// Tests the in-sequence focus decisions of CameraState::startFocusIfRequired(), in particular that focusing runs with
// the filter of the capture job even if another module (e.g. Align) changed the filter since the last frame.
// CameraState only talks to the filter manager through signals, and learns about the filter wheel through
// setCurrentFilterPosition() and setFilterManagerState(), so the tests play the part of the filter manager.
class TestCameraState : public QObject
{
        Q_OBJECT

    private Q_SLOTS:
        void initTestCase();
        void cleanupTestCase();
        void init();
        void cleanup();

        void testFocusWaitsForJobFilter_data();
        void testFocusWaitsForJobFilter();
        void testFocusWithJobFilterInPlace();
        void testNoFilterChangeWithoutFilters_data();
        void testNoFilterChangeWithoutFilters();
        void testAdaptiveFocusKeepsFilter();
        void testNoFocusRequired();
        void testFocusFallsBackIfFilterUnchanged();
        void testNewJobClearsFilterRequest();

    private:
        enum Trigger
        {
            TRIGGER_TIME,
            TRIGGER_USER,
            TRIGGER_MERIDIAN_FLIP,
            TRIGGER_TIME_AND_MERIDIAN_FLIP,
            TRIGGER_HFR
        };

        static constexpr int lumPosition = 1;
        static constexpr int haPosition = 2;

        // Create a light frame job for the Ha filter, with Lum in place as Align leaves it
        void createState(int jobFilter = haPosition, int currentFilter = lumPosition);
        void setTrigger(Trigger trigger);
        void record();

        QSharedPointer<Ekos::CaptureDeviceAdaptor> m_Devices;
        QSharedPointer<Ekos::CameraState> m_State;
        QSharedPointer<Ekos::SequenceJob> m_Job;

        // Signals emitted by the camera state, in order
        QStringList m_Events;
        QList<int> m_FilterPositions;
        QList<Ekos::FilterManager::FilterPolicy> m_FilterPolicies;

        bool m_EnforceRefocusEveryN { false };
        uint m_RefocusEveryN { 0 };
        bool m_EnforceAutofocusOnTemperature { false };
        bool m_EnforceAutofocusHFR { false };
        bool m_FocusAdaptive { false };
};

Q_DECLARE_METATYPE(Ekos::FilterManager::FilterPolicy)

void TestCameraState::initTestCase()
{
    KTEST_BEGIN();
    QVERIFY(KStarsData::Create() != nullptr);

    m_EnforceRefocusEveryN = Options::enforceRefocusEveryN();
    m_RefocusEveryN = Options::refocusEveryN();
    m_EnforceAutofocusOnTemperature = Options::enforceAutofocusOnTemperature();
    m_EnforceAutofocusHFR = Options::enforceAutofocusHFR();
    m_FocusAdaptive = Options::focusAdaptive();
}

void TestCameraState::cleanupTestCase()
{
    Options::setEnforceRefocusEveryN(m_EnforceRefocusEveryN);
    Options::setRefocusEveryN(m_RefocusEveryN);
    Options::setEnforceAutofocusOnTemperature(m_EnforceAutofocusOnTemperature);
    Options::setEnforceAutofocusHFR(m_EnforceAutofocusHFR);
    Options::setFocusAdaptive(m_FocusAdaptive);
    KTEST_END();
}

void TestCameraState::init()
{
    // Only the refocus triggers selected by each test may fire
    Options::setEnforceRefocusEveryN(false);
    Options::setEnforceAutofocusOnTemperature(false);
    Options::setEnforceAutofocusHFR(false);
    Options::setFocusAdaptive(false);

    m_Events.clear();
    m_FilterPositions.clear();
    m_FilterPolicies.clear();
}

void TestCameraState::cleanup()
{
    m_Job.clear();
    m_State.clear();
    m_Devices.clear();
}

void TestCameraState::createState(int jobFilter, int currentFilter)
{
    m_Devices.reset(new Ekos::CaptureDeviceAdaptor());
    m_State.reset(new Ekos::CameraState());

    m_Job.reset(new Ekos::SequenceJob(m_Devices, m_State, Ekos::SequenceJob::JOBTYPE_BATCH));
    m_Job->setFrameType(FRAME_LIGHT);
    if (jobFilter > 0)
        m_Job->setTargetFilter(jobFilter, jobFilter == haPosition ? "Ha" : "Lum");
    m_Job->setCoreProperty(Ekos::SequenceJob::SJ_Exposure, 300.0);
    m_Job->setCoreProperty(Ekos::SequenceJob::SJ_Count, 10);
    m_State->setActiveJob(m_Job);

    m_State->setCurrentFilterPosition(currentFilter, currentFilter == haPosition ? "Ha" : "Lum", "--");
    m_State->setFilterManagerState(Ekos::FILTER_IDLE);

    record();
}

void TestCameraState::setTrigger(Trigger trigger)
{
    auto refocus = m_State->getRefocusState();
    switch (trigger)
    {
        case TRIGGER_TIME:
        case TRIGGER_TIME_AND_MERIDIAN_FLIP:
            // Refocus every 0 minutes: the timer has always elapsed
            Options::setEnforceRefocusEveryN(true);
            Options::setRefocusEveryN(0);
            refocus->startRefocusTimer(true);
            refocus->setRefocusAfterMeridianFlip(trigger == TRIGGER_TIME_AND_MERIDIAN_FLIP);
            break;
        case TRIGGER_USER:
            refocus->setForceInSeqAF(true);
            break;
        case TRIGGER_MERIDIAN_FLIP:
            refocus->setRefocusAfterMeridianFlip(true);
            break;
        case TRIGGER_HFR:
            Options::setEnforceAutofocusHFR(true);
            refocus->setAutoFocusReady(true);
            refocus->setInSequenceFocusCounter(0);
            break;
    }
}

void TestCameraState::record()
{
    connect(m_State.data(), &Ekos::CameraState::newFilterPosition, this,
            [this](int position, Ekos::FilterManager::FilterPolicy policy)
    {
        m_Events << "filter";
        m_FilterPositions << position;
        m_FilterPolicies << policy;
    });
    connect(m_State.data(), &Ekos::CameraState::runAutoFocus, this, [this]()
    {
        m_Events << "autofocus";
    });
    connect(m_State.data(), &Ekos::CameraState::checkFocus, this, [this]()
    {
        m_Events << "hfrcheck";
    });
    connect(m_State.data(), &Ekos::CameraState::adaptiveFocus, this, [this]()
    {
        m_Events << "adaptive";
    });
}

void TestCameraState::testFocusWaitsForJobFilter_data()
{
    QTest::addColumn<int>("trigger");
    QTest::addColumn<QString>("focusEvent");
    QTest::addColumn<int>("policy");

    const int noAutofocus = Ekos::FilterManager::NO_AUTOFOCUS_POLICY;
    // Scheduler alignment check switched to the Align filter, then the refocus timer expired (the reported case)
    QTest::newRow("time elapsed") << int(TRIGGER_TIME) << "autofocus" << noAutofocus;
    QTest::newRow("user request") << int(TRIGGER_USER) << "autofocus" << noAutofocus;
    // Alignment after a meridian flip used a different filter
    QTest::newRow("after meridian flip") << int(TRIGGER_MERIDIAN_FLIP) << "autofocus" << noAutofocus;
    // The refocus timer takes precedence over the meridian flip as refocus reason
    QTest::newRow("time elapsed after meridian flip") << int(TRIGGER_TIME_AND_MERIDIAN_FLIP) << "autofocus" <<
            noAutofocus;
    // The HFR check keeps the autofocus on filter change that the job's filter change would request
    QTest::newRow("HFR check") << int(TRIGGER_HFR) << "hfrcheck" << int(Ekos::FilterManager::ALL_POLICIES);
}

// Reproduces the report that an in-sequence autofocus right after an alignment runs on the Align filter
// (e.g. Lum) instead of the filter of the capture job (e.g. Ha). The job's filter is only set in
// SequenceJobState::initCapture() when the next exposure starts, after the in-sequence focus checks, so
// startFocusIfRequired() has to set the job's filter first and wait until the filter manager is done.
void TestCameraState::testFocusWaitsForJobFilter()
{
    QFETCH(int, trigger);
    QFETCH(QString, focusEvent);
    QFETCH(int, policy);

    createState();
    setTrigger(static_cast<Trigger>(trigger));

    // The job filter is requested, focusing waits
    QVERIFY(m_State->startFocusIfRequired());
    QCOMPARE(m_Events, QStringList({"filter"}));
    QCOMPARE(m_FilterPositions, QList<int>({haPosition}));
    QCOMPARE(int(m_FilterPolicies.first()), policy);
    QVERIFY(m_State->getCaptureState() != Ekos::CAPTURE_FOCUSING);

    // The filter wheel is moving: keep waiting without a new request
    m_State->setFilterManagerState(Ekos::FILTER_CHANGE);
    QVERIFY(m_State->startFocusIfRequired());
    QCOMPARE(m_Events, QStringList({"filter"}));

    // The filter is in place but the focus offset is still being applied: keep waiting
    m_State->setCurrentFilterPosition(haPosition, "Ha", "--");
    m_State->setFilterManagerState(Ekos::FILTER_OFFSET);
    QVERIFY(m_State->startFocusIfRequired());
    QCOMPARE(m_Events, QStringList({"filter"}));

    // The filter manager is done: focusing starts with the job filter
    m_State->setFilterManagerState(Ekos::FILTER_IDLE);
    QVERIFY(m_State->startFocusIfRequired());
    QCOMPARE(m_Events, QStringList({"filter", focusEvent}));
    QCOMPARE(m_State->getCaptureState(), Ekos::CAPTURE_FOCUSING);
}

// Focusing in the middle of a job, with its filter in place, starts right away as before
void TestCameraState::testFocusWithJobFilterInPlace()
{
    createState(haPosition, haPosition);
    setTrigger(TRIGGER_TIME);

    QVERIFY(m_State->startFocusIfRequired());
    QCOMPARE(m_Events, QStringList({"autofocus"}));
}

void TestCameraState::testNoFilterChangeWithoutFilters_data()
{
    QTest::addColumn<int>("jobFilter");
    QTest::addColumn<int>("currentFilter");

    QTest::newRow("no filter wheel") << haPosition << -1;
    QTest::newRow("job without filter") << int(Ekos::INVALID_VALUE) << lumPosition;
}

void TestCameraState::testNoFilterChangeWithoutFilters()
{
    QFETCH(int, jobFilter);
    QFETCH(int, currentFilter);

    createState(jobFilter, currentFilter);
    setTrigger(TRIGGER_TIME);

    QVERIFY(m_State->startFocusIfRequired());
    QCOMPARE(m_Events, QStringList({"autofocus"}));
}

// Adaptive focus only moves the focuser and runs before most frames. It keeps running with the filter in place.
void TestCameraState::testAdaptiveFocusKeepsFilter()
{
    createState();
    Options::setFocusAdaptive(true);

    QVERIFY(m_State->startFocusIfRequired());
    QCOMPARE(m_Events, QStringList({"adaptive"}));
}

// Without a refocus reason the filter is left to the exposure preparation, as before
void TestCameraState::testNoFocusRequired()
{
    createState();

    QVERIFY(!m_State->startFocusIfRequired());
    QVERIFY(m_Events.isEmpty());
}

// If the filter manager does not carry out the request (e.g. no filter wheel in the device adaptor), focusing
// starts with the current filter on the next check instead of waiting until the capture operations time out
void TestCameraState::testFocusFallsBackIfFilterUnchanged()
{
    createState();
    setTrigger(TRIGGER_TIME);

    QVERIFY(m_State->startFocusIfRequired());
    QCOMPARE(m_Events, QStringList({"filter"}));

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("requested for focusing, but the filter is"));
    QVERIFY(m_State->startFocusIfRequired());
    QCOMPARE(m_Events, QStringList({"filter", "autofocus"}));
}

// A pending request does not survive the job, e.g. when a filter failure aborted the capture and left the
// filter manager state at FILTER_CHANGE. The restarted job requests its filter again instead of waiting.
void TestCameraState::testNewJobClearsFilterRequest()
{
    createState();
    setTrigger(TRIGGER_TIME);

    QVERIFY(m_State->startFocusIfRequired());
    m_State->setFilterManagerState(Ekos::FILTER_CHANGE);

    // capture aborted and started again
    m_State->setActiveJob(nullptr);
    m_State->setActiveJob(m_Job);

    QVERIFY(m_State->startFocusIfRequired());
    QCOMPARE(m_Events, QStringList({"filter", "filter"}));
    QCOMPARE(m_FilterPositions, QList<int>({haPosition, haPosition}));
}

QTEST_MAIN(TestCameraState)

#include "test_camerastate.moc"
