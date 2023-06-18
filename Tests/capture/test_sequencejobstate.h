/*
    Tests for scheduler job state machine.

    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ekos/capture/sequencejobstate.h"
#include <QTest>

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
    void testPrepareLightFrames();

    /**
     * @brief Test data for {@see testPrepareLightFrames()}
     */
    void testPrepareLightFrames_data();

private:
    // The state machine
    Ekos::SequenceJobState *m_stateMachine;
    // the test adapter simulating the EKOS environment
    TestAdapter *m_adapter;
    // forward signals to the sequence job
    void connectAdapter();
};

/* *********************************************************************************
 * Test adapter
 * ********************************************************************************* */

class TestAdapter : public QObject
{
    Q_OBJECT
public:
    explicit TestAdapter();

    double m_ccdtemperature = 0.0, m_rotatorangle = 0.0, m_guiding_dev;
    double m_targetccdtemp, m_targetrotatorangle, m_target_guiding_dev;
    bool m_isguideractive, m_ispreview;

    QTimer *temperatureTimer, *rotatorTimer, *guidingTimer;

    // initialize the values
    void init(double temp, double angle, double guiding_limit);

    // set CCD to preview mode
    void setCCDBatchMode(bool m_preview);
    // set the current CCD temperature
    void setCCDTemperature(double value);
    // set the current camera rotator position
    void setRotatorAngle(double *value);

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
    // update the current camera rotator position
    void newRotatorAngle(double value, IPState state);
    // update to the current guiding deviation
    void newGuiderDrift(double deviation_rms);
};
