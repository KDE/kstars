/*
    Tests for scheduler job state machine.

    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_schedulerjobstatemachine.h"

#include "Options.h"

TestSchedulerJobStateMachine::TestSchedulerJobStateMachine() : QObject(){}


void TestSchedulerJobStateMachine::testFullParameterSet()
{
    QFETCH(bool, isPreview);
    QFETCH(bool, enforce_guiding);
    QFETCH(bool, enforce_rotate);
    QFETCH(bool, enforce_temperature);

    double current_temp = 10.0, target_temp = -10.0;
    double current_drift = 10.0, target_drift = 2;
    double current_angle = 10.0, target_angle = 50;
    // set current and target values
    if (enforce_temperature)
    {
        m_adapter->setCCDTemperature(current_temp);
        m_stateMachine->setTargetCCDTemperature(target_temp);
    }
    if (enforce_guiding)
    {
        m_adapter->setGuiderDrift(current_drift);
        m_stateMachine->setTargetStartGuiderDrift(target_drift);
    }
    if (enforce_rotate)
    {
        m_adapter->setRotatorAngle(current_angle);
        m_stateMachine->setTargetRotatorAngle(target_angle);
    }

    // start the capture preparation
    m_adapter->startCapturePreparation(FRAME_LIGHT, enforce_temperature, enforce_guiding, isPreview);
    QVERIFY(m_adapter->isCapturePreparationComplete == !(enforce_temperature | enforce_guiding | enforce_rotate));
    // now step by step set the values to the target value
    if (enforce_temperature)
        m_adapter->setCCDTemperature(target_temp + 0.5*Options::maxTemperatureDiff());
    QVERIFY(m_adapter->isCapturePreparationComplete == !(enforce_guiding | enforce_rotate));
    if (enforce_guiding)
        m_adapter->setGuiderDrift(1.5);
    QVERIFY(m_adapter->isCapturePreparationComplete == !enforce_rotate);
    if (enforce_rotate)
        m_adapter->setRotatorAngle(target_angle + 0.5*Options::astrometryRotatorThreshold()/60);
    QVERIFY(m_adapter->isCapturePreparationComplete == true);
}

void TestSchedulerJobStateMachine::testLazyInitialisation()
{
    QFETCH(bool, isPreview);
    QFETCH(bool, enforce_guiding);
    QFETCH(bool, enforce_rotate);
    QFETCH(bool, enforce_temperature);

    double current_temp = 10.0, target_temp = -10.0;
    double current_drift = 10.0, target_drift = 2;
    double current_angle = 10.0, target_angle = 50;

    // set target values
    if (enforce_temperature)
        m_stateMachine->setTargetCCDTemperature(target_temp);
    if (enforce_guiding)
        m_stateMachine->setTargetStartGuiderDrift(target_drift);
    if (enforce_rotate)
        m_stateMachine->setTargetRotatorAngle(target_angle);

    // start the capture preparation
    m_adapter->startCapturePreparation(FRAME_LIGHT, enforce_temperature, enforce_guiding, isPreview);

    // set current values
    m_adapter->setCCDTemperature(current_temp);
    m_adapter->setGuiderDrift(current_drift);
    m_adapter->setRotatorAngle(current_angle);

    // now step by step set the values to the target value
    QVERIFY(m_adapter->isCapturePreparationComplete == !(enforce_temperature | enforce_guiding | enforce_rotate));
    if (enforce_temperature)
        m_adapter->setCCDTemperature(target_temp + 0.5*Options::maxTemperatureDiff());
    QVERIFY(m_adapter->isCapturePreparationComplete == !(enforce_guiding | enforce_rotate));
    if (enforce_guiding)
        m_adapter->setGuiderDrift(1.5);
    QVERIFY(m_adapter->isCapturePreparationComplete == !enforce_rotate);
    if (enforce_rotate)
        m_adapter->setRotatorAngle(target_angle + 0.5*Options::astrometryRotatorThreshold()/60);
    QVERIFY(m_adapter->isCapturePreparationComplete == true);
}

void TestSchedulerJobStateMachine::testWithProcessor()
{
    TestProcessor *processor = new TestProcessor();

    double current_temp = 10.0, target_temp = -10.0;
    double current_drift = 10.0, target_drift = 2;
    double current_angle = 10.0, target_angle = 50;
    bool isPreview = processor->isPreview;

    // set current values
    m_adapter->setCCDTemperature(current_temp);
    m_adapter->setGuiderDrift(current_drift);
    m_adapter->setRotatorAngle(current_angle);

    // set target values
    m_stateMachine->setTargetCCDTemperature(target_temp);
    m_stateMachine->setTargetStartGuiderDrift(target_drift);
    m_stateMachine->setTargetRotatorAngle(target_angle);

    // connect the processor
    connect(m_stateMachine, &Ekos::SequenceJobStateMachine::setRotatorAngle, processor, &TestProcessor::setRotatorAngle);
    connect(m_stateMachine, &Ekos::SequenceJobStateMachine::setCCDTemperature, processor, &TestProcessor::setCCDTemperature);
    connect(m_stateMachine, &Ekos::SequenceJobStateMachine::setCCDBatchMode, processor, &TestProcessor::setCCDBatchMode);
    connect(processor, &TestProcessor::newRotatorAngle, m_stateMachine, &Ekos::SequenceJobStateMachine::setCurrentRotatorAngle);
    connect(processor, &TestProcessor::newCCDTemperature, m_stateMachine, &Ekos::SequenceJobStateMachine::setCurrentCCDTemperature);

    // start the capture preparation
    m_adapter->startCapturePreparation(FRAME_LIGHT, true, true, isPreview);
    QVERIFY(m_adapter->isCapturePreparationComplete == false);
    // now step by step set the values to the target value
    m_adapter->setGuiderDrift(1.5);
    QVERIFY(m_adapter->isCapturePreparationComplete == true);
    // verify if the batch mode has been set
    QVERIFY(processor->isPreview == isPreview);

    // disconnect the processor
    disconnect(m_stateMachine, nullptr, processor, nullptr);
    disconnect(processor, nullptr, m_stateMachine, nullptr);
}

/* *********************************************************************************
 * Test data
 * ********************************************************************************* */

void TestSchedulerJobStateMachine::testFullParameterSet_data()
{
    QTest::addColumn<bool>("isPreview");           /*!< preview capture? */
    QTest::addColumn<bool>("enforce_guiding");     /*!< enforce guiding? */
    QTest::addColumn<bool>("enforce_rotate");      /*!< enforce rotating? */
    QTest::addColumn<bool>("enforce_temperature"); /*!< enforce temperature? */

    // iterate over all combinations
    for (bool preview : {true, false})
        for (bool guide : {true, false})
            for (bool rotate : {true, false})
                for (bool temperature : {true, false})
                    QTest::newRow(QString("preview=%4 enforce guide=%1, rotate=%2, temperature=%3").arg(guide).arg(rotate).arg(temperature).arg(preview).toLocal8Bit())
                            << preview << guide << rotate << temperature;
}

void TestSchedulerJobStateMachine::testLazyInitialisation_data()
{
    testFullParameterSet_data();
}

/* *********************************************************************************
 * Test infrastructure
 * ********************************************************************************* */
void TestSchedulerJobStateMachine::initTestCase()
{
    qDebug() << "initTestCase() started.";
}

void TestSchedulerJobStateMachine::cleanupTestCase()
{
    qDebug() << "cleanupTestCase() started.";
}

void TestSchedulerJobStateMachine::init()
{
    m_stateMachine = new Ekos::SequenceJobStateMachine();
    QVERIFY(m_stateMachine->getStatus() == Ekos::JOB_IDLE);
    m_adapter = new TestAdapter();
    QVERIFY(m_adapter->isCapturePreparationComplete == false);
    // forward signals to the sequence job
    connect(m_adapter, &TestAdapter::prepareCapture, m_stateMachine, &Ekos::SequenceJobStateMachine::prepareCapture);
    connect(m_adapter, &TestAdapter::newGuiderDrift, m_stateMachine, &Ekos::SequenceJobStateMachine::setCurrentGuiderDrift);
    connect(m_adapter, &TestAdapter::newRotatorAngle, m_stateMachine, &Ekos::SequenceJobStateMachine::setCurrentRotatorAngle);
    connect(m_adapter, &TestAdapter::newCCDTemperature, m_stateMachine, &Ekos::SequenceJobStateMachine::setCurrentCCDTemperature);
    // react upon sequence job signals
    connect(m_stateMachine, &Ekos::SequenceJobStateMachine::prepareComplete, m_adapter, &TestAdapter::setCapturePreparationComplete);

}

void TestSchedulerJobStateMachine::cleanup()
{
    disconnect(m_adapter, nullptr, m_stateMachine, nullptr);
    disconnect(m_stateMachine, nullptr,m_adapter , nullptr);
    delete m_adapter;
    delete m_stateMachine;
    m_adapter = nullptr;
    m_stateMachine = nullptr;
}

QTEST_GUILESS_MAIN(TestSchedulerJobStateMachine)
