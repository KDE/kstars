/*
    Helper class of KStars UI capture tests

    Copyright (C) 2020
    Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "config-kstars.h"
#include "test_ekos_debug.h"
#include "test_ekos_helper.h"
#include "test_ekos_simulator.h"
#include "test_ekos_capture.h"

#include "ekos/profileeditor.h"

#include <QObject>

#pragma once



class TestEkosCaptureHelper : public TestEkosHelper
{

public:

    explicit TestEkosCaptureHelper();

    /**
     * @brief Initialization ahead of executing the test cases.
     */
    void initTestCase() override;

    /**
     * @brief Cleanup after test cases have been executed.
     */
    void cleanupTestCase() override;

    /**
     * @brief Helper function for start of capturing
     * @param checkCapturing set to true if check of capturing should be included
     */
    bool startCapturing(bool checkCapturing = true);

    /**
     * @brief Helper function to stop capturing
     */
    bool stopCapturing();


    // sequence of capture states that are expected
    QQueue<Ekos::CaptureState> expectedCaptureStates;

    /**
     * @brief Slot to track the capture status
     * @param status new capture status
     */
    void captureStatusChanged(Ekos::CaptureState status);

    // current capture status
    Ekos::CaptureState m_CaptureStatus;

    /**
     * @brief Retrieve the current capture status.
     */
    inline Ekos::CaptureState getCaptureStatus() {return m_CaptureStatus;}
};
