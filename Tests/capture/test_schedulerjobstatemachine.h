/*
    Tests for scheduler job state machine.

    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ekos/capture/sequencejobstatemachine.h"
#include <QtTest/QtTest>

class TestAdapter;

class TestSchedulerJobStateMachine : public QObject
{
    Q_OBJECT
public:
    explicit TestSchedulerJobStateMachine();

protected slots:
    /**
     * @brief Initialization at beginning of the run of the entire test suite.
     */
    void initTestCase();
    /**
     * @brief Cleanup after the entire test suite has finished.
     */
    void cleanupTestCase();
    /**
     * @brief Initialisation before each single test case.
     */
    void init();
    /**
     * @brief Cleanup after the a single test case has finished.
     */
    void cleanup();

private slots:
    /**
     * @brief Test case with temperature, rotator and guiding
     */
    void testFullParameterSet();

    /**
     * @brief Test data for {@see testFullParameterSet()}
     */
    void testFullParameterSet_data();

    /**
     * @brief Test case with first temperature, rotator and guiding values
     *        are set AFTER capture preparation is triggered.
     */
    void testLazyInitialisation();

    /**
     * @brief Test data for {@see testLazyInitialisation()}
     */
    void testLazyInitialisation_data();

    /**
     * @brief Test case using the {@see TestProcessor} as loop setting target
     *        values for temperature and rotator angle.
     */
    void testWithProcessor();

private:
    // The state machine
    Ekos::SequenceJobStateMachine *m_stateMachine;
    // the test adapter simulating the EKOS environment
    TestAdapter *m_adapter;

};

/* *********************************************************************************
 * Test adapter
 * ********************************************************************************* */

class TestAdapter : public QObject
{
    Q_OBJECT
public:
    explicit TestAdapter() {};

    /**
     * @brief Trigger all peparation actions before a capture may be started.
     * @param frameType frame type for which the preparation should be done
     * @param enforceCCDTemp flag if the CCD temperature should be set to the target value.
     * @param enforceStartGuiderDrift flag if the guider drift needs to be taken into account
     * @param isPreview flag if the captures are in the preview mode
     */
    void startCapturePreparation(CCDFrameType frameType, bool enforceCCDTemp, bool enforceStartGuiderDrift, bool isPreview)
    {
        emit prepareCapture(frameType, enforceCCDTemp, enforceStartGuiderDrift, isPreview);
    }

    // set CCD to preview mode
    void setCCDBatchMode(bool m_preview);
    // set the current CCD temperature
    void setCCDTemperature(double value) { emit newCCDTemperature(value); }
    // set the current guiding deviation
    void setGuiderDrift(double value) { emit newGuiderDrift(value); }
    // set the current camera rotator position
    void setRotatorAngle(double value) { emit newRotatorAngle(value); }

    // flag if capture preparation is completed
    bool isCapturePreparationComplete = false;

public slots:
    void setCapturePreparationComplete() { isCapturePreparationComplete = true; }

signals:
    // signals to be forwarded to the state machine
    // trigger capture preparation steps
    void prepareCapture(CCDFrameType frameType, bool enforceCCDTemp, bool enforceStartGuiderDrift, bool isPreview);
    // update the current CCD temperature
    void newCCDTemperature(double value);
    // update the current guiding deviation
    void newGuiderDrift(double deviation_rms);
    // update the current camera rotator position
    void newRotatorAngle(double value);

};

/* *********************************************************************************
 * Test processor
 * ********************************************************************************* */

class TestProcessor : public QObject
{
    Q_OBJECT
public:
    explicit TestProcessor() {};

    void setCCDTemperature(double value) { emit newCCDTemperature(value); }
    void setRotatorAngle(double *value) { emit newRotatorAngle(*value); }

    bool isPreview = true;
    void enableCCDBatchMode(bool enable) { isPreview = !enable; }

signals:
    // update the current CCD temperature
    void newCCDTemperature(double value);
    // update the current camera rotator position
    void newRotatorAngle(double value);
    // set CCD to preview mode
    void setCCDBatchMode(bool m_preview);

};
