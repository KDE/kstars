/*
    KStars UI tests for meridian flip

    SPDX-FileCopyrightText: 2020 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "config-kstars.h"
#include "test_ekos_debug.h"

#if defined(HAVE_INDI)

#include <QObject>
#include <QPushButton>
#include <QQuickItem>
#include <QtTest>
#include <QQueue>

#include "ekos/align/align.h"
#include "ekos/guide/guide.h"
#include "ekos/mount/mount.h"
#include "ekos/profileeditor.h"

#include "test_ekos_simulator.h"
#include "test_ekos_capture.h"
#include "test_ekos_capture_helper.h"

class TestEkosMeridianFlipBase : public QObject
{
    Q_OBJECT

public:
    explicit TestEkosMeridianFlipBase(QObject *parent = nullptr);
    explicit TestEkosMeridianFlipBase(QString guider, QObject *parent = nullptr);

protected:
    /**
     * @brief Start a test EKOS profile.
     */
    bool startEkosProfile();

    /**
     * @brief Shutdown the current test EKOS profile.
     */
    bool shutdownEkosProfile();

    /**
     * @brief Enable the meridian flip on the mount tab
     * @param delay seconds delay of the meridian flip
     */
    bool enableMeridianFlip(double delay);

    /**
     * @brief Position the mount so that the meridian will be reached in certain timeframe
     * @param secsToMF seconds until the meridian will be crossed
     * @param fast use sync for fast positioning
     */
    bool positionMountForMF(int secsToMF, bool fast = true);

    /**
     * @brief Check if meridian flip runs and completes
     * @param startDelay upper limit for expected time in seconds that are expected the flip to start
     */
    bool checkMFExecuted(int startDelay);

    /**
     * @brief Determine the number of seconds until the meridian flip should take place by reading
     *        the displayed meridian flip status.
     * @return
     */
    int secondsToMF();

    /**
     * @brief Helper function that reads capture sequence test data, creates entries in the capture module,
     *        executes upfront focusing if necessary and positions the mount close to the meridian.
     * @param secsToMF seconds until the meridian will be crossed
     * @param initialFocus execute upfront focusing
     * @param guideDeviation select "Abort if Guide Deviation"
     */
    bool prepareCaptureTestcase(int secsToMF, bool initialFocus, bool guideDeviation);

    /**
     * @brief Prepare the scheduler with a single based upon the capture sequences filled
     *        by @see prepareCaptureTestcase(int,bool,bool,bool)
     * @param secsToMF seconds until the meridian will be crossed
     * @param useFocus use focusing for the scheduler job
     * @param completionCondition completion condition for the scheduler
     * @param iterations number of iterations to be executed (only relevant if completionCondition == FINISH_REPEAT)
     * @return true iff preparation was successful
     */
    bool prepareSchedulerTestcase(int secsToMF, bool useFocus, SchedulerJob::CompletionCondition completionCondition, int iterations);

    /**
     * @brief Prepare test data iterating over all combination of parameters.
     * @param exptime exposure time of the test frames
     * @param locationList variants of locations
     * @param culminationList variants of upper or lower culmination (upper = true)
     * @param filterList variants of filter parameter tests
     * @param focusList variants with/without focus tests
     * @param autofocusList variants with/without HFR autofocus tests
     * @param guideList variants with/without guiding tests
     * @param ditherList variants with/without dithering tests
     */
    void prepareTestData(double exptime, QList<QString> locationList, QList<bool> culminationList, QList<QString> filterList,
                         QList<bool> focusList, QList<bool> autofocusList, QList<bool> guideList, QList<bool> ditherList);

    /**
     * @brief Check if astrometry files exist.
     * @return true iff astrometry files found
     */
    bool checkAstrometryFiles();

    /**
     * @brief Check if guiding and dithering is restarted if required.
     */
    bool checkDithering();

    /**
     * @brief Check if re-focusing is issued if required.
     */
    bool checkRefocusing();

    /**
     * @brief Helper function for start of alignment
     * @param exposure time
     */
    bool startAligning(double expTime);

    /**
     * @brief Helper function to stop guiding
     */
    bool stopAligning();

    /**
     * @brief Helper function for start of capturing
     */
    bool startCapturing();

    /**
     * @brief Helper function to stop capturing
     */
    bool stopCapturing();

    /**
     * @brief Helper function for starting the scheduler
     */
    bool startScheduler();

    /**
     * @brief Helper function for stopping the scheduler
     */
    bool stopScheduler();

    /**
     * @brief Helper function for start focusing
     */
    bool startFocusing();

    /**
     * @brief Helper function to stop focusing
     */
    bool stopFocusing();

    /** @brief Check if after a meridian flip all features work as expected: capturing, aligning, guiding and focusing */
    bool checkPostMFBehavior();

    // helper class
    TestEkosCaptureHelper *m_CaptureHelper = nullptr;

    // target position
    SkyPoint *target;

    // initial focuser position
    int initialFocusPosition = -1;

    // regular focusing on?
    bool refocus_checked = false;
    // HFR autofocus on?
    bool autofocus_checked = false;
    // aligning used?
    bool use_aligning = false;
    // regular dithering on?
    bool dithering_checked = false;
    // astrometry files available?
    bool astrometry_available = true;
        
protected slots:
    void initTestCase();
    void cleanupTestCase();

    void init();
    void cleanup();
};

#endif // HAVE_INDI

