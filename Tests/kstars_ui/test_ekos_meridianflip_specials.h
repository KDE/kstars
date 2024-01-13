/*
    KStars UI tests for meridian flip - special cases

    SPDX-FileCopyrightText: 2020, 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "test_ekos_meridianflip_base.h"

#if defined(HAVE_INDI)

class TestEkosMeridianFlipSpecials : public TestEkosMeridianFlipBase
{
    Q_OBJECT

public:
    explicit TestEkosMeridianFlipSpecials(QObject *parent = nullptr);
    explicit TestEkosMeridianFlipSpecials(QString guider, QObject *parent = nullptr);
    
private slots:

    /** @brief Test a meridian flip where a guiding deviation aborts a capture and subsequently a flip
     * is executed. It is expected that capturing is restarted after the flip has been executed successfully.
     * */
    void testCaptureGuidingDeviationMF();

    /** @brief Test data for @see testSchedulerGuidingDeviationMF() */
    void testCaptureGuidingDeviationMF_data();

    /** @brief Test if capturing waits after a MF until the recalibration has succeeded */
    void testCaptureGuidingRecalibrationMF();

    /** @brief Test data for @see testCaptureGuidingRecalibrationMF() */
    void testCaptureGuidingRecalibrationMF_data();

    /** @brief Test a meridian flip where the dithering counter after the last capture before the flip is already 0.
     * Since dithering after the meridian flip is postponed, this checks whether after dithering takes place after
     * the first capture after the meridian flip.
     * */
    void testCaptureDitheringDelayedAfterMF();

    /** @brief Test data for @see testCaptureDitheringDelayedAfterMF() */
    void testCaptureDitheringDelayedAfterMF_data();

    /** @brief Test pausing a capture sequence before the meridian flip takes place.
     * When capturing is paused, the meridian flip should take place but then nothing more should happen unless
     * the pause is finished. As soon as capture continues, all necessary preparations should take place before capturing
     * starts again.
     * */
    void testCaptureAlignGuidingPausedMF();

    /** @brief Test data for @see testCaptureAlignGuidingPausedMF() */
    void testCaptureAlignGuidingPausedMF_data();

    /** @brief Test call pausing a capture sequence as soon as a meridian flip has already been planned. */
    void testCaptureAlignGuidingPauseMFPlanned();

    /** @brief Test data for @see testCaptureAlignGuidingPauseMFPlanned() */
    void testCaptureAlignGuidingPauseMFPlanned_data();

    /** @brief Test if a meridian flip takes place while re-focusing and consequently
     * leads to abort the refocusing and refocusing is restarted after a successful meridian flip */
    void testAbortRefocusMF();

    /** @brief Test data for @see testCaptureRefocusMF() */
    void testAbortRefocusMF_data();

    /** @brief Simple scheduler test case with capturing */
    void testSchedulerCaptureMF();

    /** @brief Test data for @see testSchedulerCaptureMF() */
    void testSchedulerCaptureMF_data();

    /** @brief Test if a meridian flip within a scheduler job takes place while re-focusing and consequently
     * leads to abort the refocusing and the scheduler recovers afterwards */
    void testAbortSchedulerRefocusMF();

    /** @brief Test data for @see testAbortSchedulerRefocusMF() */
    void testAbortSchedulerRefocusMF_data();

    /** @brief Test the situation where the mount does not change its pier side and the meridian flip is
     * repeated after 4 minutes.
     * */
    void testSimpleRepeatedMF();

    /** @brief Test data for @see testSimpleRepeatedMF() */
    void testSimpleRepeatedMF_data();

    /** @brief Test if a re-alignment is aborted during a meridian flip and post flip actions are executed.
     */
    void testCaptureRealignMF();

    /** @brief Test data for @see testCaptureRealignMF() */
    void testCaptureRealignMF_data();

    /**
     * @brief Test if failed re-alignment after a meridian flip is retried and handled appropriately
     */
    void testCapturePostRealignmentFailedHandling();

    /** @brief Test data for @see testCapturePostRealignmentFailedHandling() */
    void testCapturePostRealignmentFailedHandling_data();

};

#endif // HAVE_INDI
