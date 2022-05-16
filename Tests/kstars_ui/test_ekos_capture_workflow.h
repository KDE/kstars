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
//    klick OK
// open the calibration dialog
#define KTRY_SELECT_FLAT_METHOD(sourceWidget, preMountPark, preDomePark) do { \
QTimer::singleShot(1000, capture, [&]() { \
    QDialog *calibrationOptions = Ekos::Manager::Instance()->findChild<QDialog*>("calibrationOptions"); \
    KTRY_GADGET(calibrationOptions, QAbstractButton, sourceWidget); \
    sourceWidget->setChecked(true); \
    KTRY_GADGET(calibrationOptions, QCheckBox, parkMountC);  \
    parkMountC->setChecked(preMountPark); \
    KTRY_GADGET(calibrationOptions, QCheckBox, parkDomeC);  \
    parkDomeC->setChecked(preDomePark); \
    KTRY_GADGET(calibrationOptions, QAbstractButton, manualDurationC);  \
    manualDurationC->setChecked(true); \
    QDialogButtonBox* buttons = calibrationOptions->findChild<QDialogButtonBox*>("buttonBox"); \
    QVERIFY(nullptr != buttons); \
    QTest::mouseClick(buttons->button(QDialogButtonBox::Ok), Qt::LeftButton); \
}); \
KTRY_CAPTURE_CLICK(calibrationB);  } while (false)

#define KTRY_SELECT_FLAT_WALL(capture, azimuth, altitude) do { \
QTimer::singleShot(1000, capture, [&]() { \
    QDialog *calibrationOptions = Ekos::Manager::Instance()->findChild<QDialog*>("calibrationOptions"); \
    KTRY_GADGET(calibrationOptions, QAbstractButton, wallSourceC); \
    wallSourceC->setChecked(true); \
    KTRY_SET_LINEEDIT(calibrationOptions, azBox, azimuth); \
    KTRY_SET_LINEEDIT(calibrationOptions, altBox, altitude); \
    KTRY_GADGET(calibrationOptions, QAbstractButton, manualDurationC);  \
    manualDurationC->setChecked(true); \
    QDialogButtonBox* buttons = calibrationOptions->findChild<QDialogButtonBox*>("buttonBox"); \
    QVERIFY(nullptr != buttons); \
    QTest::mouseClick(buttons->button(QDialogButtonBox::Ok), Qt::LeftButton); \
}); \
KTRY_CAPTURE_CLICK(calibrationB);  } while (false)


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
     * @brief Test if a guiding deviation beyond the configured limit blocks the start of
     * capturing until the guiding deviation is below the configured deviation threshold.
     */
    void testInitialGuidingLimitCapture();

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
     * @brief Test capturing flats with a lights panel
     */
    void testFlatLightPanelSource();

    /** @brief Test data for {@see testFlatLightPanelSource()} */
    void testFlatLightPanelSource_data();

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
     * @brief Check mount and dome parking before capturing flats.
     */
    void testFlatPreMountAndDomePark();

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

};

#endif // HAVE_INDI
