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

// Select the flat source from the flats calibration options.
// Start with a delay of 1 sec a new thread that edits the calibration options:
//    select the source widget
//    set pre-mount and pre-dome park options
//    select a manual flat ADU value
//    click OK
// open the calibration dialog
#define KTRY_SELECT_FLAT_METHOD(sourceWidget, preMountPark, preDomePark) do { \
    bool completed = false; \
    QTimer::singleShot(5000, capture, [&]() { \
    QDialog *calibrationOptions = nullptr; \
    if (! QTest::qWaitFor([&](){return ((calibrationOptions = Ekos::Manager::Instance()->findChild<QDialog*>("Calibration")) != nullptr);}, 5000)) { \
        QFAIL(qPrintable("Calibrations options dialog not found!")); } \
    KTRY_GADGET(calibrationOptions, QAbstractButton, sourceWidget); \
    sourceWidget->setChecked(true); \
    KTRY_GADGET(calibrationOptions, QCheckBox, captureCalibrationParkMount);  \
    captureCalibrationParkMount->setChecked(preMountPark); \
    KTRY_GADGET(calibrationOptions, QCheckBox, captureCalibrationParkDome);  \
    captureCalibrationParkDome->setChecked(preDomePark); \
    KTRY_GADGET(calibrationOptions, QAbstractButton, captureCalibrationDurationManual);  \
    captureCalibrationDurationManual->setChecked(true); \
    QDialogButtonBox* buttons = calibrationOptions->findChild<QDialogButtonBox*>("buttonBox"); \
    QVERIFY(nullptr != buttons); \
    QTest::mouseClick(buttons->button(QDialogButtonBox::Ok), Qt::LeftButton); \
    }); \
    KTRY_CAPTURE_CLICK(calibrationB); \
    QTRY_VERIFY_WITH_TIMEOUT(completed == true, 5000);  } while (false)

/**
 * @brief Select the Manual flat duration, so that flats are taken at the exposure of the job
 * instead of the ADU calibration that new flat jobs use by default.
 * @param capture capture module
 */
#define KTRY_SELECT_FLAT_MANUAL(capture) do { \
    bool completed = false; \
    QTimer::singleShot(1000, capture, [&]() { \
    QDialog *calibrationOptions = nullptr; \
    if (! QTest::qWaitFor([&](){return ((calibrationOptions = Ekos::Manager::Instance()->findChild<QDialog*>("Calibration")) != nullptr);}, 5000)) { \
        QFAIL(qPrintable("Calibrations options dialog not found!")); } \
    KTRY_GADGET(calibrationOptions, QAbstractButton, captureCalibrationDurationManual);  \
    captureCalibrationDurationManual->setChecked(true); \
    QVERIFY(captureCalibrationDurationManual->isChecked()); \
    QDialogButtonBox* buttons = calibrationOptions->findChild<QDialogButtonBox*>("buttonBox"); \
    QVERIFY(nullptr != buttons); \
    QTest::mouseClick(buttons->button(QDialogButtonBox::Ok), Qt::LeftButton); \
    completed = true; \
    }); \
    KTRY_CLICK(capture, calibrationB); \
    QTRY_VERIFY_WITH_TIMEOUT(completed == true, 5000); } while (false)

#define KTRY_SELECT_FLAT_WALL(capture, azimuth, altitude) do { \
    bool completed = false; \
    QTimer::singleShot(1000, capture, [&]() { \
    QDialog *calibrationOptions = nullptr; \
    if (! QTest::qWaitFor([&](){return ((calibrationOptions = Ekos::Manager::Instance()->findChild<QDialog*>("Calibration")) != nullptr);}, 5000)) { \
        QFAIL(qPrintable("Calibrations options dialog not found!")); } \
    KTRY_GADGET(calibrationOptions, QAbstractButton, captureCalibrationWall); \
    captureCalibrationWall->setChecked(true); \
    QVERIFY(captureCalibrationWall->isChecked()); \
    KTRY_SET_LINEEDIT(calibrationOptions, azBox, azimuth); \
    KTRY_SET_LINEEDIT(calibrationOptions, altBox, altitude); \
    KTRY_GADGET(calibrationOptions, QAbstractButton, captureCalibrationDurationManual);  \
    captureCalibrationDurationManual->setChecked(true); \
    QDialogButtonBox* buttons = calibrationOptions->findChild<QDialogButtonBox*>("buttonBox"); \
    QVERIFY(nullptr != buttons); \
    QTest::mouseClick(buttons->button(QDialogButtonBox::Ok), Qt::LeftButton); \
    completed = true; \
    }); \
    KTRY_CLICK(Ekos::Manager::Instance()->captureModule(), calibrationB); \
    QTRY_VERIFY_WITH_TIMEOUT(completed == true, 5000); } while (false)

/** @brief Select ADU as flat duration in the calibration options.
 * @param capture capture module
 * @param target target ADU
 * @param tolerance ADU tolerance
 * @param wall use the wall at az=90°, alt=0° as flat light source
 */
#define KTRY_SELECT_FLAT_ADU(capture, target, tolerance, wall) do { \
    bool completed = false; \
    QTimer::singleShot(1000, capture, [&]() { \
    QDialog *calibrationOptions = nullptr; \
    if (! QTest::qWaitFor([&](){return ((calibrationOptions = Ekos::Manager::Instance()->findChild<QDialog*>("Calibration")) != nullptr);}, 5000)) { \
        QFAIL(qPrintable("Calibrations options dialog not found!")); } \
    KTRY_GADGET(calibrationOptions, QAbstractButton, captureCalibrationWall); \
    captureCalibrationWall->setChecked(wall); \
    QVERIFY(captureCalibrationWall->isChecked() == wall); \
    if (wall) { \
        KTRY_SET_LINEEDIT(calibrationOptions, azBox, "90"); \
        KTRY_SET_LINEEDIT(calibrationOptions, altBox, "0"); } \
    KTRY_GADGET(calibrationOptions, QAbstractButton, captureCalibrationUseADU); \
    captureCalibrationUseADU->setChecked(true); \
    KTRY_SET_SPINBOX(calibrationOptions, captureCalibrationADUValue, target); \
    KTRY_SET_SPINBOX(calibrationOptions, captureCalibrationADUTolerance, tolerance); \
    QDialogButtonBox* buttons = calibrationOptions->findChild<QDialogButtonBox*>("buttonBox"); \
    QVERIFY(nullptr != buttons); \
    QTest::mouseClick(buttons->button(QDialogButtonBox::Ok), Qt::LeftButton); \
    completed = true; \
    }); \
    KTRY_CLICK(Ekos::Manager::Instance()->captureModule(), calibrationB); \
    QTRY_VERIFY_WITH_TIMEOUT(completed == true, 5000); } while (false)


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

    protected Q_SLOTS:
        void initTestCase();
        void cleanupTestCase();

        void init();
        void cleanup();

        /**
         * @brief prepareTestCase start the profile and prepare the modules for a test case
         * @param flatsOnly only prepare what capturing flats needs. Focus, alignment and guiding are
         * left as they are, so that flat tests also run without astrometry index files.
         */
        bool prepareTestCase(bool flatsOnly = false);

        /**
         * @brief Set one element of a number property of the primary train's camera and wait until
         *        the driver confirms it.
         */
        bool setCameraNumber(const QString &property, const QString &element, double value);

        /**
         * @brief Open the Dark Library dialog for the primary train, set to capture count 1 s darks at 1x1.
         * @param darkLibraryDialog set to the dialog
         */
        bool openDarkLibrary(int count, QWidget * &darkLibraryDialog);

        /**
         * @brief Start a Dark Library run of count frames and wait until its master dark is recorded.
         * @param master set to the file name of the new master dark
         */
        bool runDarkLibrary(QWidget *darkLibraryDialog, int count, QString &master);

    private:
        // helper class
        TestEkosCaptureHelper *m_CaptureHelper = nullptr;

        QString target = "test";

        /**
         * @brief Setup capturing
         * @param refocusLimitTime time limit to trigger re-focusing
         * @param refocusHFR HFR limit to trigger re-focusing
         * @param refocusTemp temperature limit to trigger re-focusing
         * @param delay delay between frame captures
         * @return true iff preparation was successful
         */
        bool prepareCapture(int refocusLimitTime = 0.0, double refocusHFR = 0.0, double refocusTemp = 0.0, int delay = 0);

        /**
         * @brief initCaptureSetting Initialize Capture settings with a single bool/double pair
         * @param setting a single setting with an enabling checkbox and the value
         * @param checkboxName name of the QCheckBox widget
         * @param spinBoxName name of the QDoubleSpinBox widget
         */
        void initCaptureSetting(TestEkosCaptureHelper::OptDouble setting, const QString checkboxName, const QString dspinBoxName);

        /**
         * @brief Helper function translating simple QString input into QTest test data rows
         * @param exptime exposure time of the sequence
         * @param sequenceList List of sequences with filter and count as QString("<filter>:<count"), ... list
         */
        void prepareTestData(double exptime, QList<QString> sequenceList);

        /**
         * @brief verifyCalibrationSettings Verify if the flats calibration settings match the test data.
         */
        bool verifyCalibrationSettings();

        /**
         * @brief fillScripts Open the scripts dialog and fill its values from {@see #scripts}
         */
        void fillCaptureScripts();

        // counter for images taken in a single test run
        int image_count;

        QDir *getImageLocation();


    private Q_SLOTS:
        /** @brief Test if re-focusing is triggered after the configured delay. */
        void testCaptureRefocusDelay();

        /** @brief Test data for @see testCaptureRefocusDelay() */
        void testCaptureRefocusDelay_data();

        /** @brief Test if re-focusing is triggered when the HFR gets worse. */
        void testCaptureRefocusHFR();

        /** @brief Test data for @see testCaptureRefocusHFR() */
        void testCaptureRefocusHFR_data();

        /**
         * @brief Regression test for the bug where an in-sequence HFR check switches
         *        the filter wheel to the Focus module's lock filter (Luminance) but
         *        never restores it to the original capture filter (Red) after the
         *        check passes.
         *
         * Steps:
         * 1. Enable in-sequence HFR with a small threshold (0.1 %) so the check
         *    fires after the first captured frame.
         * 2. Run autofocus with the Luminance filter (prepareFocusModule already
         *    set focusFilter = "Luminance" and locked every filter to "Luminance").
         * 3. Add a two-frame Red capture sequence (5 s each).
         * 4. Run the capture sequence and wait for CAPTURE_COMPLETE.
         * 5. Assert that the physical filter wheel is on Red (success), not
         *    Luminance (fail — demonstrates the unrestored-filter bug).
         */
        void testCaptureHFRCheckFilterRestoration();

        /**
         * @brief Regression test: in-sequence autofocus has to run with the filter of the capture job,
         *        not with the filter another module (e.g. Align) left in place.
         *
         * Steps:
         * 1. Run autofocus with the Luminance filter, which leaves Luminance in place like an alignment would.
         * 2. Remove the Luminance lock of the Red filter so that focusing for Red really uses Red.
         * 3. Add a Red capture sequence and request an in-sequence autofocus.
         * 4. Start capturing and assert that the autofocus completes with Red in place.
         */
        void testCaptureRefocusWithJobFilter();

        /** @brief Test if re-focusing is triggered when the temperature changes. */
        void testCaptureRefocusTemperature();

        /** @brief Test data for @see testCaptureRefocusTemperature() */
        void testCaptureRefocusTemperature_data();

        /**
         * @brief Test if capturing continues when re-focusing is triggered while the
         *        sequence is resumed from a pause.
         *
         * This is the situation of a follower camera that has been paused and suspended
         * while the lead camera dithers and that needs to re-focus when it continues.
         */
        void testCaptureRefocusAfterPause();

        /** @brief Test data for @see testCaptureRefocusAfterPause() */
        void testCaptureRefocusAfterPause_data();

        /** @brief Test if re-focusing is aborted if capture is aborted. */
        void testCaptureRefocusAbort();

        /** @brief Test data for @see testCaptureRefocusAbort() */
        void testCaptureRefocusAbort_data();

        /** @brief Test whether a pre/post-job/capture scripts are executed as expected */
        void testCaptureScriptsExecution();

        /** @brief Test data for @see testCaptureScriptsExecution() */
        void testCaptureScriptsExecution_data();

        /**
         * @brief Test if capture continues where it had been suspended by a
         * guiding deviation as soon as guiding is back below the deviation threshold
         */
        void testGuidingDeviationSuspendingCapture();

        /**
         * @brief Test if aborting a job suspended due to a guiding deviation
         * remains aborted when the guiding deviation is below the configured threshold.
         */
        void testGuidingDeviationAbortCapture();

        /**
         * @brief Regression test for the bug where a dithering result restarts a
         *        sequence that has been aborted in the meantime.
         *
         * CaptureModuleState::setGuideStatus() restarts every camera whose capture is
         * stopped when dithering completes, not only those that it stopped for
         * dithering itself. A sequence aborted while the dithering is still running is
         * therefore resurrected, and continues exposing after the mount has already
         * been slewed away or parked.
         *
         * Steps:
         * 1. Start guiding and a three frame sequence with dithering after each frame.
         * 2. Wait until dithering starts after the first frame.
         * 3. Abort the sequence while the dithering is still in progress.
         * 4. Wait for the dithering to complete successfully.
         * 5. Assert that capturing does not resume.
         */
        void testAbortedSequenceNotRestartedAfterDithering();

        /**
         * @brief Companion to {@see testAbortedSequenceNotRestartedAfterDithering()} for
         *        the failure branch. CaptureModuleState::setGuideStatus() handles
         *        GUIDE_DITHERING_SUCCESS and GUIDE_DITHERING_ERROR alike, so an aborted
         *        sequence has to stay aborted for both of them.
         *
         * Steps:
         * 1. Capture the first frame of a three frame sequence.
         * 2. Abort the sequence.
         * 3. Report a failed dithering run to the capture module.
         * 4. Assert that capturing does not resume.
         */
        void testAbortedSequenceNotRestartedAfterDitheringError();

        /**
         * @brief Test if a guiding deviation beyond the configured limit blocks the start of
         * capturing until the guiding deviation is below the configured deviation threshold.
         */
        void testInitialGuidingLimitCapture();

        /** @brief Test data for @see testInitialGuidingLimitCapture() */
        void testInitialGuidingLimitCapture_data();

        /**
         * @brief Wait with start of capturing until the target temperature has been reached
         */
        void testCaptureWaitingForTemperature();

        /** @brief Test data for {@see testCaptureWaitingForTemperature()} */
        void testCaptureWaitingForTemperature_data();

        /**
         * @brief Wait with start of capturing until the target rotator position has been reached
         */
        void testCaptureWaitingForRotator();

        /**
         * @brief Test capturing flats with manual flat light
         */
        void testFlatManualSource();

        /** @brief Test data for {@see testFlatManualSource()} */
        void testFlatManualSource_data();

        /**
         * @brief Test capturing flats, darks and bias with a lights panel
         */
        void testLightPanelSource();

        /** @brief Test data for {@see testLightPanelSource()} */
        void testLightPanelSource_data();

        /**
         * @brief Test that a light panel stays on across consecutive flat jobs, is off for following
         * light frames and is turned off when the sequence is complete
         */
        void testLightPanelConsecutiveFlats();

        /** @brief Test data for {@see testLightPanelConsecutiveFlats()} */
        void testLightPanelConsecutiveFlats_data();

        /**
         * @brief Test that a light panel turned on for flats is turned off when capturing is aborted
         * or stopped, and stays on while capturing is suspended or paused
         */
        void testLightPanelStopFlats();

        /** @brief Test data for {@see testLightPanelStopFlats()} */
        void testLightPanelStopFlats_data();

        /**
         * @brief Test that flats restarted right after an abort are taken with the light on
         */
        void testLightPanelRestartFlatsAfterAbort();

        /**
         * @brief Test that stopping capture does not touch a light that Capture did not turn on
         */
        void testLightPanelManualLightUntouched();

        /**
         * @brief Test capturing flats or darks with a dust cap
         */
        void testDustcapSource();

        /** @brief Test data for {@see testFlatDustcapSource()} */
        void testDustcapSource_data();

        /**
         * @brief Test capturing flats with the wall as flat light source
         */
        void testWallSource();

        /** @brief Test data for {@see testWallSource()} */
        void testWallSource_data();

        /**
         * @brief A flat preview with ADU as flat duration does not calibrate: it takes a single frame
         * and leaves the exposure and the light box brightness unchanged, for a manual target and Auto.
         */
        void testFlatADUPreview_data();
        void testFlatADUPreview();

        /**
         * @brief A flat job that starts high and is too dark at the maximum brightness takes a frame
         * at the search minimum, and stops instead of lengthening the exposure when the light box
         * gives no light.
         */
        void testFlatADUNoLightStops();

        /**
         * @brief A flat job with target 0 (Auto) derives its target and tolerance from the
         * bit depth of the frame and reaches it.
         */
        void testFlatADUAutoTarget();

        /**
         * @brief A brightness calibration frame taken while the light box applied a per-filter preset
         * is taken again, and the search starts at the preset.
         */
        void testFlatADURetakeAfterPresetChange();

        /**
         * @brief A flat job whose target cannot be reached within the exposure range of the
         * camera stops instead of repeating the same frame.
         */
        void testFlatADUStopsAtExposureLimit();

        /**
         * @brief A flat job that is too bright at the minimum brightness continues with a shorter
         * exposure, and restores the starting brightness of the light box when it ends.
         */
        void testFlatADUFallbackTooBright();

        /**
         * @brief Aborting a flat job during the brightness search restores the starting brightness,
         * unless that is 0, which switches some light boxes off.
         */
        void testFlatADUAbortRestoresBrightness_data();
        void testFlatADUAbortRestoresBrightness();

        /**
         * @brief A flat job with ADU duration stops before its first frame when the camera does not
         * transfer FITS or XISF, and says how to go on.
         */
        void testFlatADUNativeEncodingStops();

        /**
         * @brief Wall flats with a light box in the train turn the light box on at the wall, so they
         * adjust its brightness.
         */
        void testFlatADUWallUsesLightBox();

        /**
         * @brief Check mount and dome parking before capturing flats.
         */
        //void testPreMountAndDomePark();

        /** @brief Test data for {@see testFlatPreMountAndDomePark()} */
        //void testPreMountAndDomePark_data();

        /**
         * @brief Check the flat capture behavior if "same focus" is selectee
         *        in the filter settings when capturing flats. Before capturing
         *        flats, the focuser should move to the position last determined
         *        with an autofocus run for the selected filter.
         */
        void testFlatSyncFocus();

        /**
         * @brief Test capturing darks with manual scope covering
         */
        void testDarkManualCovering();

        /** @brief Test data for {@see testDarkManualCovering()} */
        void testDarkManualCovering_data();

        /**
         * @brief Test capturing a simple darks library
         */
        void testDarksLibrary();

        /**
         * @brief A Dark Library run that was stopped must not leave its frames behind: the master dark of the
         *        next run must have the bias level of a single dark frame.
         */
        void testDarksLibraryAfterAbortedRun();

        /**
         * @brief A defect map generated in the Dark Library from a master dark of a sensor with hot and cold
         *        pixels (simulated by the CCD simulator) must remove all of them.
         */
        void testDarksLibraryDefectMap();

        /**
         * @brief testLoadEsqFile Test for correctly loading a capture sequence file. This test case concentrates
         * upon the general values stored in the file, that are valid for all sequence jobs contained.
         */
        void testLoadEsqFileGeneral();

        /** @brief Test data for {@see testLoadEsqFileGeneral()} */
        void testLoadEsqFileGeneral_data();

        /**
         * @brief testLoadEsqFileBasicJobSettings Test for basic job settings when loading capture sequence file.
         */
        void testLoadEsqFileBasicJobSettings();

        /** @brief Test data for {@see testLoadEsqFileBasicJobSettings()} */
        void testLoadEsqFileBasicJobSettings_data();

        /**
         * @brief testLoadEsqFileFlatSettings Test for flat job settings when loading capture sequence file.
         */
        void testLoadEsqFileCalibrationSettings();

        /** @brief Test data for {@see testLoadEsqFileCalibrationSettings()} */
        void testLoadEsqFileCalibrationSettings_data();
};

#endif // HAVE_INDI
