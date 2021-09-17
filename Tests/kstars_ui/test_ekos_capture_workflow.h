/*
    KStars UI tests for alignment

    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "config-kstars.h"
#include "test_ekos.h"
#include "test_ekos_capture_helper.h"

#if defined(HAVE_INDI)

#include <QObject>

class TestEkosCaptureWorkflow : public QObject
{
    Q_OBJECT
public:
    explicit TestEkosCaptureWorkflow(QObject *parent = nullptr);

protected:
    // sequence of alignment states that are expected
    QQueue<Ekos::AlignState> expectedAlignStates;
    // sequence of telescope states that are expected
    QQueue<ISD::Telescope::Status> expectedTelescopeStates;
    // sequence of capture states that are expected
    QQueue<Ekos::CaptureState> expectedCaptureStates;
    // sequence of focus states that are expected
    QQueue<Ekos::FocusState> expectedFocusStates;

    // destination where images will be located
    QTemporaryDir *destination;
    QDir *imageLocation = nullptr;

protected slots:
    void initTestCase();
    void cleanupTestCase();

    void init();
    void cleanup();

    bool prepareTestCase();

private:
    // helper class
    TestEkosCaptureHelper *m_CaptureHelper = nullptr;

    QString target = "test";

    // CCD device
    QString m_CCDDevice = "CCD Simulator";

    // current scope status
    ISD::Telescope::Status m_TelescopeStatus { ISD::Telescope::MOUNT_IDLE };

    // current capture status
    Ekos::CaptureState m_CaptureStatus { Ekos::CAPTURE_IDLE };

    // current focus status
    Ekos::FocusState m_FocusStatus { Ekos::FOCUS_IDLE };

    /**
     * @brief Slot to track the mount status
     * @param status new mount state
     */
    void telescopeStatusChanged(ISD::Telescope::Status status);

    /**
     * @brief Slot to track the capture status
     * @param status new capture status
     */
    void captureStatusChanged(Ekos::CaptureState status);

    /**
     * @brief Slot to track the focus status
     * @param status new focus status
     */
    void focusStatusChanged(Ekos::FocusState status);

    /**
     * @brief slot to track captured images from the align process
     * @param view
     */
    void imageReceived(FITSView *view);

    /**
     * @brief Setup capturing
     * @return true iff preparation was successful
     */
    bool prepareCapture();

    /**
     * @brief Execute autofocus
     * @return true iff it succeeded
     */
    bool executeFocusing();

    /**
     * @brief Helper function translating simple QString input into QTest test data rows
     * @param exptime exposure time of the sequence
     * @param sequence filter and count as QString("<filter>:<count"), ... list
     */
    void prepareTestData(double exptime, QString sequence);

    // counter for images taken in a single test run
    int image_count;

    QDir *getImageLocation();


private slots:
    /** @brief Test if re-focusing is triggered after the configured delay. */
    void testCaptureRefocus();

    /** @brief Test data for @see testCaptureRefocus() */
    void testCaptureRefocus_data();

    /** @brief Test if re-focusing is aborted if capture is aborted. */
    void testCaptureRefocusAbort();

    /** @brief Test data for @see testCaptureRefocusAbort() */
    void testCaptureRefocusAbort_data();

    /** @brief Test whether a pre-capture script is executed before a capture is executed */
    void testPreCaptureScriptExecution();
};

#endif // HAVE_INDI
