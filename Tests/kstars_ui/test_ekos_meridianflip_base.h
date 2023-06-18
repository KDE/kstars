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
#include <QTest>
#include <QQueue>

#include "ekos/align/align.h"
#include "ekos/guide/guide.h"
#include "ekos/mount/mount.h"
#include "ekos/scheduler/scheduler.h"
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
     * @brief Check if meridian flip has been started
     * @param startDelay upper limit for expected time in seconds that are expected the flip to start
     */
    bool checkMFStarted (int startDelay);

    /**
     * @brief Check if meridian flip runs and completes
     * @param startDelay upper limit for expected time in seconds that are expected the flip to start
     */
    bool checkMFExecuted(int startDelay);

    /**
     * @brief Determine the number of seconds until the meridian flip should take place by reading
     *        the displayed meridian flip status.
     */
    int secondsToMF();

    /**
     * @brief Determine the target close to the meridian
     * @param secsToMF seconds to meridian flip
     * @param set to true if a sync close to the target should be executed
     */
    void findMFTestTarget(int secsToMF, bool fast);

    /**
     * @brief General preparations used both for pure capturing test cases as well as those with the scheduler.
     * @param guideDeviation select "Abort if Guide Deviation"
     * @param initialGuideDeviation select "Only Start if Guide Deviation <"
     */
    bool prepareMFTestcase(bool guideDeviation, bool initialGuideDeviation);

    /**
     * @brief Helper function that reads capture sequence test data, creates entries in the capture module,
     *        executes upfront focusing if necessary and positions the mount close to the meridian.
     * @param secsToMF seconds until the meridian will be crossed
     * @param guideDeviation select "Abort if Guide Deviation"
     * @param initialGuideDeviation select "Only Start if Guide Deviation <"
     */
    bool prepareCaptureTestcase(int secsToMF, bool guideDeviation, bool initialGuideDeviation);

    /**
     * @brief Prepare the scheduler with a single based upon the capture sequences filled
     *        by @see prepareCaptureTestcase(int,bool,bool,bool)
     * @param secsToMF seconds until the meridian will be crossed
     * @param useAlign use alignment for the scheduler job
     * @param completionCondition completion condition for the scheduler
     * @param iterations number of iterations to be executed (only relevant if completionCondition == FINISH_REPEAT)
     * @return true iff preparation was successful
     */
    bool prepareSchedulerTestcase(int secsToMF, bool useAlign, Ekos::Scheduler::SchedulerAlgorithm algorithm, SchedulerJob::CompletionCondition completionCondition, int iterations);

    /**
     * @brief Prepare test data iterating over all combination of parameters.
     * @param exptime exposure time of the test frames
     * @param locationList variants of locations
     * @param culminationList variants of upper or lower culmination (upper = true)
     * @param filterList variants of filter parameter tests
     * @param focusList variants with/without the focus after flip option selected ( 0=no, 1=refocus, 2=HFR autofocus, 3=after meridian flip)
     * @param guideList variants with/without guiding tests
     * @param ditherList variants with/without dithering tests
     */
    void prepareTestData(double exptime, QList<QString> locationList, QList<bool> culminationList, QList<std::pair<QString, int> > filterList,
                         QList<int> focusList, QList<bool> guideList, QList<bool> ditherList);

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
    bool executeAlignment(double expTime);

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

    /** @brief Check if after a meridian flip all features work as expected: capturing, aligning, guiding and focusing */
    bool checkPostMFBehavior();

    // helper class
    TestEkosCaptureHelper *m_CaptureHelper = nullptr;

    // target position
    SkyPoint *target;

    // initial focuser position
    int initialFocusPosition = -1;

    // focus after meridian flip selected?
    bool refocus_checked = false;
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

