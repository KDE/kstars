/*
    KStars UI tests for meridian flip - special cases

    Copyright (C) 2020
    Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
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

    /** @brief Test the situation where the mount does not change its pier side and the meridian flip is
     * repeated after 4 minutes.
     * */
    void testSimpleRepeatedMF();

    /** @brief Test data for @see testRepeatedMF() */
    void testRepeatedMF_data();
};

#endif // HAVE_INDI
