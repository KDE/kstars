/*
    Tests for scheduler job state machine.

    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_sequencejobstate.h"

#include "Options.h"

TestSequenceJobState::TestSequenceJobState() : QObject() {}


void TestSequenceJobState::testPrepareLightFrames()
{
    QFETCH(bool, isPreview);
    QFETCH(bool, enforce_rotate);
    QFETCH(bool, enforce_temperature);
    QFETCH(bool, enforce_guiding);
    QFETCH(double, temperature_delta);
    QFETCH(double, angle_delta);

    double current_temp = 10.0;
    double current_angle = 10.0;
    double current_guide_dev = 100.0, target_guide_dev = 2.0;

    // initialize the test processor, but do not inform the state machine
    m_adapter->init(current_temp, current_angle, current_guide_dev);

    // set target values
    if (enforce_temperature)
        m_stateMachine->setTargetCCDTemperature(current_temp + temperature_delta);
    if (enforce_rotate)
        m_stateMachine->setTargetRotatorAngle(current_angle + angle_delta);
    if (enforce_guiding)
        m_stateMachine->setTargetStartGuiderDrift(target_guide_dev);

    // connect the processor
    connect(m_stateMachine, &Ekos::SequenceJobState::setRotatorAngle, m_adapter, &TestAdapter::setRotatorAngle);
    connect(m_stateMachine, &Ekos::SequenceJobState::setCCDTemperature, m_adapter, &TestAdapter::setCCDTemperature);
    connect(m_stateMachine, &Ekos::SequenceJobState::setCCDBatchMode, m_adapter, &TestAdapter::setCCDBatchMode);
    connect(m_adapter, &TestAdapter::newRotatorAngle, m_stateMachine, &Ekos::SequenceJobState::setCurrentRotatorPositionAngle);
    connect(m_adapter, &TestAdapter::newCCDTemperature, m_stateMachine, &Ekos::SequenceJobState::setCurrentCCDTemperature);

    // start the capture preparation
    m_stateMachine->prepareLightFrameCapture(enforce_temperature, isPreview);

    // The test adapter is simulates the behavior of real devices
    QTRY_VERIFY_WITH_TIMEOUT(m_adapter->isCapturePreparationComplete, 5000);
}

/* *********************************************************************************
 * Test data
 * ********************************************************************************* */

void TestSequenceJobState::testPrepareLightFrames_data()
{
    QTest::addColumn<bool>("isPreview");           /*!< preview capture? */
    QTest::addColumn<bool>("enforce_rotate");      /*!< enforce rotating? */
    QTest::addColumn<bool>("enforce_temperature"); /*!< enforce temperature? */
    QTest::addColumn<bool>("enforce_guiding");     /*!< enforce maximal initial guiding deviation? */
    QTest::addColumn<double>("temperature_delta"); /*!< difference between current and target temperature */
    QTest::addColumn<double>("angle_delta");     /*!< difference between current and target rotator angle */

    // iterate over all combinations
    for (bool preview :
            {
                false, true
            })
        for (bool rotate :
                {
                    false, true
                })
            for (bool temperature :
                    {
                        false, true
                    })
                for (bool guiding :
                        {
                            false, true
                        })
                    for (double delta_t :
                            {
                                -21.0, 0.0
                                })
                        for (double delta_r :
                    {
                        45.0, 0.0
                    })
    QTest::newRow(QString("preview=%4 enforce rotate=%1, temperature=%2, guiding=%3, delta_t=%5, delta_rot=%6")
                  .arg(rotate).arg(temperature).arg(guiding).arg(preview).arg(delta_t).arg(delta_r).toLocal8Bit())
            << preview << rotate << temperature << guiding << delta_t << delta_r;
}

/* *********************************************************************************
 * Test infrastructure
 * ********************************************************************************* */
void TestSequenceJobState::initTestCase()
{
    qDebug() << "initTestCase() started.";
}

void TestSequenceJobState::cleanupTestCase()
{
    qDebug() << "cleanupTestCase() started.";
}

void TestSequenceJobState::init()
{
    QSharedPointer<Ekos::CaptureModuleState> captureModuleState;
    captureModuleState.reset(new Ekos::CaptureModuleState());
    m_stateMachine = new Ekos::SequenceJobState(captureModuleState);
    // currently all tests are for light frames
    m_stateMachine->setFrameType(FRAME_LIGHT);
    QVERIFY(m_stateMachine->getStatus() == Ekos::JOB_IDLE);
    m_adapter = new TestAdapter();
    QVERIFY(m_adapter->isCapturePreparationComplete == false);
    // forward signals to the sequence job
    connect(m_adapter, &TestAdapter::prepareCapture, m_stateMachine, &Ekos::SequenceJobState::prepareLightFrameCapture);
    // react upon sequence job signals
    connect(m_stateMachine, &Ekos::SequenceJobState::prepareComplete, m_adapter, &TestAdapter::setCapturePreparationComplete);
    connect(m_stateMachine, &Ekos::SequenceJobState::readCurrentState, m_adapter, &TestAdapter::readCurrentState);

}

void TestSequenceJobState::cleanup()
{
    disconnect(m_adapter, nullptr, m_stateMachine, nullptr);
    disconnect(m_stateMachine, nullptr, m_adapter, nullptr);
    delete m_adapter;
    delete m_stateMachine;
    m_adapter = nullptr;
    m_stateMachine = nullptr;
}



TestAdapter::TestAdapter()
{
    // simulate a CCD cooler that stepwise reduces the temperature to a target value
    temperatureTimer = new QTimer(this);
    temperatureTimer->setInterval(200);
    temperatureTimer->setSingleShot(false);
    connect(temperatureTimer, &QTimer::timeout, this, [this]()
    {
        if (std::abs(m_ccdtemperature - m_targetccdtemp) > std::numeric_limits<double>::epsilon())
        {
            // decrease gap by 1 deg or less
            double delta = std::min(1.0, std::abs(m_ccdtemperature - m_targetccdtemp));
            m_ccdtemperature += (m_targetccdtemp > m_ccdtemperature) ? delta : -delta;
            // publish new value
            emit newCCDTemperature(m_ccdtemperature);
        }
        else
            // finish if temperature has been reached
            temperatureTimer->stop();
    });
    // simulate a rotator that stepwise rotates to the target value
    rotatorTimer = new QTimer(this);
    rotatorTimer->setInterval(200);
    rotatorTimer->setSingleShot(false);
    connect(rotatorTimer, &QTimer::timeout, this, [this]()
    {
        if (std::abs(m_rotatorangle - m_targetrotatorangle) > std::numeric_limits<double>::epsilon())
        {
            // decrease gap by 10 deg or less
            double delta = std::min(10.0, std::abs(m_rotatorangle - m_targetrotatorangle));
            m_rotatorangle += (m_rotatorangle < m_targetrotatorangle) ? delta : -delta;
            // publish new value
            emit newRotatorAngle(m_rotatorangle, std::abs(m_rotatorangle - m_targetrotatorangle) > 1.0 ? IPS_BUSY : IPS_OK);
        }
        else
            // finish if target angle has been reached
            rotatorTimer->stop();
    });
    // simulate guiding bringing the deviation stepwise below the threshold
    guidingTimer = new QTimer(this);
    guidingTimer->setInterval(200);
    guidingTimer->setSingleShot(false);
    connect(guidingTimer, &QTimer::timeout, this, [this]()
    {
        if (m_guiding_dev > m_target_guiding_dev)
        {
            m_guiding_dev /= 2;
            // publish new value
            emit newGuiderDrift(m_guiding_dev);
        }
        else
            // finish if target angle has been reached
            guidingTimer->stop();
    });
}

void TestAdapter::init(double temp, double angle, double guiding_limit)
{
    m_ccdtemperature = temp;
    m_rotatorangle   = angle;
    m_guiding_dev    = guiding_limit;
}

void TestAdapter::setCCDBatchMode(bool m_preview)
{
    m_ispreview = m_preview;
}

void TestAdapter::setCCDTemperature(double value)
{
    // simulate behaviour of the INDI server
    emit newCCDTemperature(value);
    // set the new target
    m_targetccdtemp = value;
    // start timer if not already running
    if (! temperatureTimer->isActive())
        temperatureTimer->start();
}

void TestAdapter::setRotatorAngle(double value)
{
    // set the new target
    m_targetrotatorangle = value;
    // start timer if not already running
    if (! rotatorTimer->isActive())
        rotatorTimer->start();
}

void TestAdapter::readCurrentState(Ekos::CaptureState state)
{
    // signal the current device value
    switch (state)
    {
        case Ekos::CAPTURE_SETTING_TEMPERATURE:
            emit newCCDTemperature(m_ccdtemperature);
            break;
        case Ekos::CAPTURE_SETTING_ROTATOR:
            emit newRotatorAngle(m_rotatorangle, IPS_OK);
            break;
        default:
            // do nothing
            break;
    }
}

QTEST_GUILESS_MAIN(TestSequenceJobState)
