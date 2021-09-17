/*
    KStars UI tests for meridian flip

    SPDX-FileCopyrightText: 2020 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "config-kstars.h"
#include "test_ekos_meridianflip_base.h"

#if defined(HAVE_INDI)


class TestEkosMeridianFlip : public TestEkosMeridianFlipBase
{
    Q_OBJECT

public:
    explicit TestEkosMeridianFlip(QObject *parent = nullptr);
    explicit TestEkosMeridianFlip(QString guider, QObject *parent = nullptr);

private slots:
    /** @brief Test a meridian flip without running capture sequence. */
    void testSimpleMF();

    /** @brief Test data for @see testSimpleMF() */
    void testSimpleMF_data();

    /** @brief Test the delay of a meridian flip without running a capture sequence */
    void testSimpleMFDelay();

    /** @brief Test data for @see testSimpleMFDelay() */
    void testSimpleMFDelay_data();

    /**
     * @brief Test a meridian flip with guiding running
     */
    void testGuidingMF();

    /** @brief Test data for @see testGuidingMF() */
    void testGuidingMF_data();

    /** @brief Test a meridian flip during a simple capture sequence */
    void testCaptureMF();

    /** @brief Test data for @see testCaptureMF() */
    void testCaptureMF_data();

    /** @brief Test a meridian flip during a simple capture sequence, where the capture is aborted during the waiting state */
    void testCaptureMFAbortWaiting();

    /** @brief Test data for @see testCaptureMFAbortWaiting() */
    void testCaptureMFAbortWaiting_data();

    /** @brief Test a meridian flip during a simple capture sequence, where the capture is aborted during the flip */
    void testCaptureMFAbortFlipping();

    /** @brief Test data for @see testCaptureMFAbortWaiting() */
    void testCaptureMFAbortFlipping_data();

    /** @brief Test a meridian flip during a simple capture sequence with guiding on */
    void testCaptureGuidingMF();

    /** @brief Test data for @see testCaptureMF() */
    void testCaptureGuidingMF_data();

    /** @brief Test a meridian flip during a simple capture sequence with alignment executed before */
    void testCaptureAlignMF();

    /** @brief Test data for @see testCaptureMF() */
    void testCaptureAlignMF_data();

    /** @brief Test a meridian flip during a simple capture sequence with guiding and alignment */
    void testCaptureAlignGuidingMF();

    /** @brief Test data for @see testCaptureMF() */
    void testCaptureAlignGuidingMF_data();

};

#endif // HAVE_INDI

