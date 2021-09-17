/*
    KStars UI tests for verifying correct counting of the capture module

    SPDX-FileCopyrightText: 2020 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QQueue>

#include "config-kstars.h"
#include "test_ekos_debug.h"

#if defined(HAVE_INDI)
#include "ekos/ekos.h"
#include "ekos/capture/sequencejob.h"
#include "test_ekos_capture_helper.h"

class TestEkosCaptureCount : public QObject
{
    Q_OBJECT
public:
    explicit TestEkosCaptureCount(QObject *parent = nullptr);

protected:

    /**
     * @brief Setup capturing
     * @return true iff preparation was successful
     */
    bool prepareCapture();

    /**
     * @brief Setup capturing for tests with the scheduler
     * @param completionCondition completion condition for the scheduler
     * @return true iff preparation was successful
     */
    bool prepareScheduledCapture(SchedulerJob::CompletionCondition completionCondition);

    /**
     * @brief Prepare the scheduler for the test.
     * @param sequenceFile File name of the capture sequence file
     * @param sequence filter and count as QString("<filter>:<count"), ... list
     * @param capturedFramesMap mapping from filter to existing frames per filter as QString("<filter>:<count"), ... list
     * @param completionCondition completion condition for the scheduler
     * @param iterations number of iterations to be executed (only relevant if completionCondition == FINISH_REPEAT)
     * @param rememberJobProgress should the scheduler use the option "Remember job progress"
     * @param exptime exposure time (identical for all frames)
     * @return true iff preparation was successful
     */
    bool setupScheduler(QString sequenceFile, QString sequence, QString capturedFramesMap, SchedulerJob::CompletionCondition completionCondition,
                        int iterations, bool rememberJobProgress, double exptime);

    /**
     * @brief Verify the counts that the scheduler displays in the job table
     * @param sequence filter and count as QString("<filter>:<count"), ... list
     * @param capturedFramesMap mapping from filter to existing frames per filter as QString("<filter>:<count"), ... list
     * @param completionCondition completion condition for the scheduler
     * @param iterations number of iterations to be executed (only relevant if completionCondition == FINISH_REPEAT)
     * @param rememberJobProgress should the scheduler use the option "Remember job progress"
     * @param exptime exposure time (identical for all frames)
     * @return true iff the displayed counts match the specification
     */
    bool verifySchedulerCounting(QString sequence, QString capturedFramesMap, SchedulerJob::CompletionCondition completionCondition,
                                 int iterations, bool rememberJobProgress, double exptime);

    /**
     * @brief Execute capturing
     * @return true iff exactly the expected frames have been taken
     */
    bool executeCapturing();

    /**
     * @brief Helper function translating simple QString input into QTest test data rows
     * @param exptime exposure time of the sequence
     * @param sequence filter and count as QString("<filter>:<count"), ... list
     * @param capturedFramesMap mapping from filter to existing frames per filter as QString("<filter>:<count"), ... list
     * @param expectedFrames expected number of frames per filter as QString("<filter>:<count"), ... list
     * @param iterations number of iterations the capture sequence should be repeated
     */
    void prepareTestData(double exptime, QString sequence, QString capturedFramesMap, QString expectedFrames, int iterations = 1);

    // mapping between image signature and number of images expected to be captured for this signature
    QMap<QString, int> m_expectedImages;
    /**
     * @brief Register that a new image has been captured
     */
    void captureComplete(const QString &filename, double exposureSeconds, const QString &filter, double hfr);

    // sequence of scheduler states that are expected
    QQueue<Ekos::SchedulerState> expectedSchedulerStates;
    /**
     * @brief Slot to receive a new scheduler state
     * @param status new capture status
     */
    void schedulerStateChanged(Ekos::SchedulerState status);

    // current scheduler status
    Ekos::SchedulerState m_SchedulerStatus;

    /**
     * @brief Retrieve the current capture status.
     */
    inline Ekos::SchedulerState getSchedulerStatus() {return m_SchedulerStatus;}


protected slots:
    void initTestCase();
    void cleanupTestCase();

    void init();
    void cleanup();

private:
    // current capture status
    Ekos::CaptureState m_CaptureStatus;

    // helper class
    TestEkosCaptureHelper *m_CaptureHelper = nullptr;

    QString target = "test";

    /**
     * @brief Fill the map of frames that have already been captured
     * @param expectedFrames comma separated list of <filter>:<count>
     * @return true if everything was successful
     */
    bool fillCapturedFramesMap(QString capturedFramesMap);

    /**
     * @brief Determine the total count from a comma separated sequence of <filter>:<count>
     * @return sum of <count>
     */
    int totalCount(QString sequence);

    /**
     * @brief Determine the map filter --> counts from a comma separated sequence of <filter>:<count>
     * @param sequence
     * @return
     */
    QMap<QString, uint16_t> framesMap(QString sequence);

    /**
     * @brief Fill the map of frames that are expected to be captured
     * @param expectedFrames comma separated list of <filter>:<count>
     * @return true if everything was successful
     */
    bool setExpectedFrames(QString expectedFrames);


    /**
     * @brief Check if the expected number of frames are captured
     * @return true if yes
     */
    bool checkCapturedFrames();

private slots:
    /**
     * @brief Test whether the capture module produces exactly the diff between the capture frames map and the defined frame counts.
     */
    void testCaptureWithCaptureFramesMap();

    /** @brief Test data for @see testCaptureWithCaptureFramesMap() */
    void testCaptureWithCaptureFramesMap_data();

    /**
     * @brief Test of appropriate captures controlled by the scheduler for fixed
     *        number of iterations.
     */
    void testSchedulerCapture();

    /** @brief Test data for @see testSchedulerCapture() */
    void testSchedulerCapture_data();

    /**
     * @brief Test of appropriate capturing for the scheduler using the
     *        "Repeat until terminated" option.
     */
    void testSchedulerCaptureInfiteLooping();

    /** @brief Test data for @see testSchedulerCaptureInfiteLooping() */
    void testSchedulerCaptureInfiteLooping_data();
};

#endif // HAVE_INDI
