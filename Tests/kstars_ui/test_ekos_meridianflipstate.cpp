/*
    Tests for meridian flip state machine.

    SPDX-FileCopyrightText: 2022 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_ekos_meridianflipstate.h"

#include "kstars.h"
#include "kstars_ui_tests.h"
#include "test_ekos_debug.h"
#include "kstarsdata.h"
#include "Options.h"
#include "skymapcomposite.h"
#include "indicom.h"




void TestEkosMeridianFlipState::testMeridianFlip()
{
    QFETCH(bool, enabled);
    QFETCH(bool, captureInterface);
    QFETCH(bool, upper);
    QFETCH(double, offset);
    const double base_offset = 15.0;
    // mount should be in tracking mode
    QVERIFY(m_MountStatus == ISD::Mount::MOUNT_TRACKING);
    // expect SLEW --> TRACK
    expectedMountStates.append(ISD::Mount::MOUNT_SLEWING);
    expectedMountStates.append(ISD::Mount::MOUNT_TRACKING);
    // find a target 10 sec before meridian
    SkyPoint target = findMFTestTarget(base_offset, upper);
    m_Mount->Slew(target);
    // expect finished slew to target close to the meridian
    QTRY_VERIFY_WITH_TIMEOUT(expectedMountStates.isEmpty(), 10000);
    QVERIFY(m_currentPosition.valid && m_currentPosition.position == target);
    QVERIFY2(m_currentPosition.ha.HoursHa() - (upper ? 0 : 12) < 0,
             QString("Current HA=%1").arg(m_currentPosition.ha.HoursHa()).toLatin1());
    QVERIFY(std::abs(m_currentPosition.ha.HoursHa() - (upper ? 0 : 12) + base_offset / 3600) * 3600 <=
            60.0); // less than 60 arcsec error
    QVERIFY(m_currentPosition.pierSide == (upper ? ISD::Mount::PIER_WEST : ISD::Mount::PIER_EAST));

    // expect RUNNING --> COMPLETED
    expectedMeridianFlipMountStates.append(Ekos::MeridianFlipState::MOUNT_FLIP_RUNNING);
    expectedMeridianFlipMountStates.append(Ekos::MeridianFlipState::MOUNT_FLIP_COMPLETED);
    // expect SLEW --> TRACK
    expectedMountStates.append(ISD::Mount::MOUNT_SLEWING);
    expectedMountStates.append(ISD::Mount::MOUNT_TRACKING);

    if (enabled)
    {
        // wait until the meridian flip is planned
        QTRY_VERIFY_WITH_TIMEOUT(m_MeridianFlipMountStatus == Ekos::MeridianFlipState::MOUNT_FLIP_PLANNED, 15000);
        // check if the time to the meridian flip is within the range
        QTRY_VERIFY_WITH_TIMEOUT(secs_to_mf >= 0, 5000);
        QVERIFY(std::abs(secs_to_mf - base_offset - offset * 4 * 60) <= 20.0);

        if (captureInterface)
        {
            // acknowledge as requested (TODO: this should be shifted to the state machine!)
            m_stateMachine->updateMeridianFlipStage(Ekos::MeridianFlipState::MF_REQUESTED);
            qCInfo(KSTARS_EKOS_TEST) << "Meridian flip requested.";
            // accept the flip
            m_stateMachine->updateMFMountState(Ekos::MeridianFlipState::MOUNT_FLIP_ACCEPTED);
            qCInfo(KSTARS_EKOS_TEST) << "Meridian flip accepted.";
        }

        // wait until the slew completes
        QTRY_VERIFY_WITH_TIMEOUT(expectedMountStates.isEmpty(), 10000);
        // meridian flip should be completed
        QTRY_VERIFY_WITH_TIMEOUT(expectedMeridianFlipMountStates.isEmpty(), 3 * Options::minFlipDuration());
        // mount is on the east side for upper, west for lower culmination
        QVERIFY(m_currentPosition.pierSide == (upper ? ISD::Mount::PIER_EAST : ISD::Mount::PIER_WEST));
    }
    else
    {
        // meridian flip not enabled
        QTest::qWait(10.0);
        // still no flip planned
        QVERIFY(m_MeridianFlipMountStatus == Ekos::MeridianFlipState::MOUNT_FLIP_NONE);
        QVERIFY(expectedMeridianFlipMountStates.size() == 2);
        QVERIFY(expectedMountStates.size() == 2);
    }
}

void TestEkosMeridianFlipState::testMeridianFlip_data()
{
#if QT_VERSION < QT_VERSION_CHECK(5,9,0)
    QSKIP("Bypassing fixture test on old Qt");
    Q_UNUSED(enabled)
    Q_UNUSED(captureInterface)
    Q_UNUSED(culmination)
    Q_UNUSED(offset)
#else
    QTest::addColumn<QString>("location");      /*!< geographical location? */
    QTest::addColumn<bool>("enabled");          /*!< meridian flip enabled? */
    QTest::addColumn<bool>("captureInterface"); /*!< capture interface present? */
    QTest::addColumn<bool>("upper");            /*!< upper culmination? */
    QTest::addColumn<double>("offset");         /*!< meridian flip offset (in degrees) */

    for (auto loc :
            {"Cape Town", "Greenwich"
            })
    {
        QTest::newRow(QString("loc=%1 enabled=no").arg(loc).toLatin1()) << loc << false << false << true << 0.0;
        QTest::newRow(QString("loc=%1 enabled=yes, capture=no").arg(loc).toLatin1()) << loc << true << false << true << 0.0;
        QTest::newRow(QString("loc=%1 enabled=yes, capture=yes, upper=yes").arg(loc).toLatin1()) << loc << true << true << true <<
                0.0;
        QTest::newRow(QString("loc=%1 enabled=yes, capture=yes, upper=no").arg(loc).toLatin1()) << loc << true << true << false <<
                0.0;
        QTest::newRow(QString("loc=%1 enabled=yes, capture=yes, upper=yes offset=15'").arg(loc).toLatin1()) << loc << true << true
                << true << 0.25;
        QTest::newRow(QString("loc=%1 enabled=yes, capture=yes, upper=no offset=15'").arg(loc).toLatin1()) << loc << true << true <<
                false << 0.25;
    }

#endif
}

void TestEkosMeridianFlipState::connectAdaptor()
{
    m_Mount = new MountSimulator();

    // connect to the mount process position changes
    connect(m_Mount, &MountSimulator::newCoords, this,
            &TestEkosMeridianFlipState::updateTelescopeCoord, Qt::UniqueConnection);
    // connect to the mount process status changes
    connect(m_Mount, &MountSimulator::newStatus, this,
            &TestEkosMeridianFlipState::mountStatusChanged, Qt::UniqueConnection);
    // connect to the state machine to receive meridian flip status changes
    connect(m_stateMachine, &Ekos::MeridianFlipState::newMountMFStatus, this,
            &TestEkosMeridianFlipState::meridianFlipMountStatusChanged, Qt::UniqueConnection);
    // publish the meridian flip mount status
    connect(m_stateMachine, &Ekos::MeridianFlipState::newMeridianFlipMountStatusText, [ = ](QString statustext)
    {
        qCInfo(KSTARS_EKOS_TEST) << statustext;
        if (secs_to_mf < 0 && statustext.startsWith("Meridian flip in"))
            secs_to_mf = m_Helper.secondsToMF(statustext);
    });

    // connect the state machine to the mount simulator
    connect(m_Mount, &MountSimulator::newStatus, m_stateMachine, &Ekos::MeridianFlipState::setMountStatus,
            Qt::UniqueConnection);
    connect(m_Mount, &MountSimulator::newCoords, m_stateMachine, &Ekos::MeridianFlipState::updateTelescopeCoord,
            Qt::UniqueConnection);
    connect(m_stateMachine, &Ekos::MeridianFlipState::slewTelescope, m_Mount, &MountSimulator::Slew, Qt::UniqueConnection);

    // initialize home position
    SkyPoint *pos = KStars::Instance()->data()->skyComposite()->findByName("Kocab");
    m_Mount->init(*pos);

    // activate the mount in the state machine
    m_stateMachine->setMountConnected(true);
}

void TestEkosMeridianFlipState::disconnectAdaptor()
{
    m_Mount->shutdown();
    m_stateMachine->setMountConnected(false);
    disconnect(m_Mount, nullptr, nullptr, nullptr);
    disconnect(m_stateMachine, &Ekos::MeridianFlipState::newMountMFStatus, this,
               &TestEkosMeridianFlipState::meridianFlipMountStatusChanged);

    m_Mount->deleteLater();
}

SkyPoint TestEkosMeridianFlipState::findMFTestTarget(int secsToMF, bool upper)
{
    // translate seconds into fractions of degrees
    double delta = static_cast<double>(secsToMF) / 240; // 240 = 86400 / 360

    // determine the hemisphere
    const bool northern = (KStarsData::Instance()->geo()->lat()->Degrees() > 0);

    // Azimuth position close to meridian
    double az = range360(((upper != northern) ? 0.0 : 180.0) + (northern ? -delta : delta));
    // we assume that all locations have a longitude > 10 deg
    double alt = 10;

    // Define the target
    SkyPoint target;
    target.setAlt(alt);
    target.setAz(az);

    // calculate local sideral time, converted to degrees, and observer's latitude
    const dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());
    const dms lat(KStarsData::Instance()->geo()->lat()->Degrees());
    // calculate JNow RA/DEC
    target.HorizontalToEquatorial(&lst, &lat);

    return target;
}

void TestEkosMeridianFlipState::updateTelescopeCoord(const SkyPoint &position, ISD::Mount::PierSide pierSide, const dms &ha)
{
    m_currentPosition.position = position;
    m_currentPosition.pierSide = pierSide;
    m_currentPosition.ha       = ha;
    m_currentPosition.valid    = true;
    qCDebug(KSTARS_EKOS_TEST) << "RA="
                              << m_currentPosition.position.ra().toHMSString()
                              << ", DEC="
                              << m_currentPosition.position.dec().toDMSString()
                              << ", ha=" << ha.HoursHa()
                              << ", "
                              << ISD::Mount::pierSideStateString(m_currentPosition.pierSide);
}

void TestEkosMeridianFlipState::mountStatusChanged(ISD::Mount::Status status)
{
    m_MountStatus = status;
    // check if the new state is the next one expected, then remove it from the stack
    if (!expectedMountStates.isEmpty() && expectedMountStates.head() == status)
        expectedMountStates.dequeue();
}

void TestEkosMeridianFlipState::meridianFlipMountStatusChanged(Ekos::MeridianFlipState::MeridianFlipMountState status)
{
    m_MeridianFlipMountStatus = status;
    // check if the new state is the next one expected, then remove it from the stack
    if (!expectedMeridianFlipMountStates.isEmpty() && expectedMeridianFlipMountStates.head() == status)
        expectedMeridianFlipMountStates.dequeue();
}

/* *********************************************************************************
 * Test infrastructure
 * ********************************************************************************* */
void TestEkosMeridianFlipState::init()
{
    // set the location
    QFETCH(QString, location);
    GeoLocation * const geo = KStars::Instance()->data()->locationNamed(location);
    QVERIFY(geo != nullptr);
    KStars::Instance()->data()->setLocation(*geo);
    // initialize the helper
    m_Helper.init();
    // clear the time to flip
    secs_to_mf = -1;
    // clear the meridian flip state
    m_MeridianFlipMountStatus = Ekos::MeridianFlipState::MOUNT_FLIP_NONE;
    // clear queues
    expectedMeridianFlipMountStates.clear();
    expectedMountStates.clear();

    // run 10x as fast
    KStarsData::Instance()->clock()->setClockScale(10.0);
    // start the clock
    KStarsData::Instance()->clock()->start();
    // initialize the state machine
    m_stateMachine = new Ekos::MeridianFlipState();
    // enable the meridian flip
    QFETCH(bool, enabled);
    m_stateMachine->setEnabled(enabled);
    // simulate the capture interface through the test case
    QFETCH(bool, captureInterface);
    m_stateMachine->setHasCaptureInterface(captureInterface);
    // set the meridian flip offset
    QFETCH(double, offset);
    m_stateMachine->setOffset(offset);
    // clear current position
    m_currentPosition.valid = false;
    // set delay of 5s to ignore too early tracking
    Options::setMinFlipDuration(10);
    // connect the test adaptor to the test case
    connectAdaptor();
}

void TestEkosMeridianFlipState::cleanup()
{
    disconnectAdaptor();
    delete m_stateMachine;
}

void TestEkosMeridianFlipState::initTestCase()
{

}

void TestEkosMeridianFlipState::cleanupTestCase()
{

}

/* *********************************************************************************
 * Test adapter
 * ********************************************************************************* */



void MountSimulator::updatePosition()
{
    if (m_status == ISD::Mount::MOUNT_SLEWING && KStarsData::Instance()->clock()->utc() > slewFinishedTime)
    {
        // slew finished
        m_position = m_targetPosition;
        m_pierside = m_targetPierside;
        emit newCoords(m_position, m_pierside, calcHA(m_position));
        setStatus(ISD::Mount::MOUNT_TRACKING);
        qCInfo(KSTARS_EKOS_TEST) << "Mount tracking.";
    }

    // in any case, report the current position
    emit newCoords(m_position, m_pierside, calcHA(m_position));
}

void MountSimulator::init(const SkyPoint &position)
{
    m_position = position;
    // regularly report the position and recalculated HA
    connect(KStarsData::Instance()->clock(), &SimClock::timeAdvanced, this, &MountSimulator::updatePosition);
    // set state to tracking
    setStatus(ISD::Mount::MOUNT_TRACKING);
}

void MountSimulator::shutdown()
{
    disconnect(KStarsData::Instance()->clock(), &SimClock::timeAdvanced, this, &MountSimulator::updatePosition);
}

void MountSimulator::Slew(const SkyPoint &position)
{
    // start slewing
    setStatus(ISD::Mount::MOUNT_SLEWING);
    qCInfo(KSTARS_EKOS_TEST) << "Mount slewing...";
    m_targetPosition = position;
    m_targetPierside = calcPierSide(position);
    // calculate an additional delay if pier side needs to be changed
    double delay = (m_pierside == ISD::Mount::PIER_UNKNOWN
                    || m_pierside == m_targetPierside) ? 0. : 2. * Options::minFlipDuration();
    QTest::qWait(100);
    // set time when slew should be finished and tracking should start
    slewFinishedTime = KStarsData::Instance()->clock()->utc().addSecs(delay);
    // slewing, pier side change and changing back to tracking will be handled by #updatePosition()
}

void MountSimulator::Sync(const SkyPoint &position)
{
    m_position = position;
    // set pier side only if unknown
    if (m_pierside == ISD::Mount::PIER_UNKNOWN)
        m_pierside = calcPierSide(position);

    emit newCoords(m_position, m_pierside, calcHA(position));
    setStatus(ISD::Mount::MOUNT_TRACKING);
}

void MountSimulator::setStatus(ISD::Mount::Status value)
{
    if (m_status != value)
        emit newStatus(value);

    m_status = value;
}

dms MountSimulator::calcHA(const SkyPoint &pos)
{
    dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());
    return dms(lst.Degrees() - pos.ra().Degrees());
}

ISD::Mount::PierSide MountSimulator::calcPierSide(const SkyPoint &pos)
{
    double ha = calcHA(pos).HoursHa(); // -12 <= ha <= 12
    if (ha <= 0)
        return ISD::Mount::PIER_WEST;
    else
        return ISD::Mount::PIER_EAST;
}

QTEST_KSTARS_MAIN(TestEkosMeridianFlipState)
