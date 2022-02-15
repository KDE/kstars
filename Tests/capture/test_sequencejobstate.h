/*
    Tests for scheduler job state machine.

    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ekos/capture/sequencejobstate.h"
#include <QtTest/QtTest>

class TestAdapter;

class TestSequenceJobState : public QObject
{
    Q_OBJECT
public:
    explicit TestSequenceJobState();

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

    /**
     * @brief Test if guider deactivation will prevent completing the preparation.
     */
    void testGuiderDeactivation();

private:
    // The state machine
    Ekos::SequenceJobState *m_stateMachine;
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

    double m_ccdtemperature, m_guiderdrift, m_rotatorangle;
    bool m_isguideractive;

    // initialize the values
    void init(double temp, double drift, double angle, bool guideractive = true);

    // set CCD to preview mode
    void setCCDBatchMode(bool m_preview);
    // set the current CCD temperature
    void setCCDTemperature(double value);
    // update whether guiding is active
    void setGuiderActive(bool active);
    // set the current guiding deviation
    void setGuiderDrift(double value);
    // set the current camera rotator position
    void setRotatorAngle(double value, IPState state);

    // flag if capture preparation is completed
    bool isCapturePreparationComplete = false;

public slots:
    /**
     * @brief Slot that reads the requested device state and publishes the corresponding event
     * @param state device state that needs to be read directly
     */
    void readCurrentState(Ekos::CaptureState state);

    /**
     * @brief Slot that marks the capture preparation as completed.
     */
    void setCapturePreparationComplete() { isCapturePreparationComplete = true; }

signals:
    // signals to be forwarded to the state machine
    // trigger capture preparation steps
    void prepareCapture(CCDFrameType frameType, bool enforceCCDTemp, bool enforceStartGuiderDrift, bool isPreview);
    // update the current CCD temperature
    void newCCDTemperature(double value);
    // update the current guider state
    void newGuiderState(bool active);
    // update the current guiding deviation
    void newGuiderDrift(double deviation_rms);
    // update the current camera rotator position
    void newRotatorAngle(double value, IPState state);

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
    void setRotatorAngle(double *value) { emit newRotatorAngle(*value, IPS_OK); }

    bool isPreview = true;
    void enableCCDBatchMode(bool enable) { isPreview = !enable; }

signals:
    // update the current CCD temperature
    void newCCDTemperature(double value);
    // update the current camera rotator position
    void newRotatorAngle(double value, IPState state);
    // set CCD to preview mode
    void setCCDBatchMode(bool m_preview);

};
