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
    explicit TestEkosCaptureWorkflow(QString guider, QObject *parent = nullptr);

protected:
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

    /**
     * @brief Test if after capture continues where it had been suspended by a
     * guiding deviation as soon as guiding is back below the deviation threshold
     */
    void testGuidingDeviationSuspendingCapture();

    /**
     * @brief Test if aborting a job suspended due to a guiding deviation
     * remains aborted when the guiding deviation is below the configured threshold.
     */
    void testGuidingDeviationAbortCapture();
};

#endif // HAVE_INDI
