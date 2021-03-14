/*
    KStars UI tests for meridian flip

    Copyright (C) 2020
    Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
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


#define QTEST_KSTARS_MAIN_GUIDERSELECT(klass) \
int main(int argc, char *argv[]) { \
    QString guider = "Internal"; \
    std::vector<char *> testargv; \
    bool showfunctions = false; \
    for (int i = 0; i < argc; i++) { \
        if (!strcmp("-functions", argv[i])) \
        {testargv.push_back(argv[i]); showfunctions = true;} \
        else if (!strcmp("-guider", argv[i]) && i+1 < argc) \
        { guider = argv[i+1]; i++; } \
        else testargv.push_back(argv[i]); } \
    klass tc(guider); \
    if (showfunctions) return QTest::qExec(&tc, testargv.size(), testargv.data()); \
    QApplication app(argc, argv); \
    prepare_tests(); \
    int failure = 0; \
    QTimer::singleShot(1000, &app, [&] { \
        qDebug("Starting tests..."); \
        failure |= run_wizards(testargv.size(), testargv.data()); \
        if (!failure) { failure |= QTest::qExec(&tc, testargv.size(), testargv.data()); } \
        qDebug("Tests are done."); \
        app.quit(); }); \
    execute_tests(); \
    return failure; }


class TestEkosMeridianFlipBase : public QObject
{
    Q_OBJECT

public:
    explicit TestEkosMeridianFlipBase(QObject *parent = nullptr);
    explicit TestEkosMeridianFlipBase(QString guider, QObject *parent = nullptr);

protected:
    /**
     * @brief Configure the EKOS profile
     * @param name of the profile
     * @param isPHD2 use internal guider or PHD2
     */
    bool setupEkosProfile(QString name, bool isPHD2);

    /**
     * @brief create a new EKOS profile
     * @param name name of the profile
     * @param isPHD2 use internal guider or PHD2
     * @param isDone will be true if everything succeeds
     */
    void createEkosProfile(QString name, bool isPHD2, bool *isDone);

    /**
     * @brief Start a test EKOS profile.
     */
    bool startEkosProfile();

    /**
     * @brief Shutdown the current test EKOS profile.
     */
    bool shutdownEkosProfile(QString guider);

    /**
     * @brief Fill mount, guider, CCD and focuser of an EKOS profile
     * @param isDone will be true if everything succeeds
     */
    void fillProfile(bool *isDone);

    /**
     * @brief Set a tree view combo to a given value
     * @param combo box with tree view
     * @param lookup target value
     */
    void setTreeviewCombo(QComboBox *combo, const QString lookup);

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
     * @param calibrate execute initial guiding to calibrate the guider
     * @param initialFocus execute upfront focusing
     * @param guideDeviation select "Abort if Guide Deviation"
     */
    bool prepareCaptureTestcase(int secsToMF, bool calibrate, bool initialFocus, bool guideDeviation);

    /**
     * @brief Prepare test data iterating over all combination of parameters.
     * @param exptime exposure time of the test frames
     * @param filterList variants of filter parameter tests
     * @param focusList variants with/without focus tests
     * @param autofocusList variants with/without HFR autofocus tests
     * @param ditherList variants with/without dithering tests
     */
    void prepareTestData(double exptime, QList<QString> filterList, QList<bool> focusList, QList<bool> autofocusList, QList<bool> ditherList);

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
     * @param is guiding on?
     */
    bool checkRefocusing(bool guiding);

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
     * @brief Helper function for start focusing
     */
    bool startFocusing();

    /**
     * @brief Helper function to stop focusing
     */
    bool stopFocusing();

    /**
     * @brief Helper function for start of guiding
     * @param guiding exposure time
     */
    bool startGuiding(double expTime);

    /**
     * @brief Helper function to stop guiding
     */
    bool stopGuiding();

    /**
     * @brief Helper function starting PHD2
     */
    void startPHD2();

    /**
     * @brief Helper function stopping PHD2
     */
    void stopPHD2();

    /**
     * @brief Helper function for preparing the PHD2 test configuration
     */
    void preparePHD2();

    /**
     * @brief Helper function for cleaning up PHD2 test configuration
     */
    void cleanupPHD2();


    // Mount device
    QString m_MountDevice = "Telescope Simulator";
    // CCD device
    QString m_CCDDevice = "CCD Simulator";
    // Guiding device
    QString m_GuiderDevice = "Guide Simulator";
    // Focusing device
    QString m_FocuserDevice = "Focuser Simulator";
    // Guider (PHD2 or Internal)
    QString m_Guider = "Internal";

    // PHD2 setup (host and port)
    QProcess *phd2 { nullptr };
    QString const phd2_guider_host = "localhost";
    QString const phd2_guider_port = "4400";

    // current mount meridian flip status
    Ekos::Mount::MeridianFlipStatus m_MFStatus { Ekos::Mount::FLIP_NONE };
    // sequence of alignment states that are expected
    QQueue<Ekos::AlignState> expectedAlignStates;
    // sequence of capture states that are expected
    QQueue<Ekos::CaptureState> expectedCaptureStates;
    // sequence of focus states that are expected
    QQueue<Ekos::FocusState> expectedFocusStates;
    // sequence of guiding states that are expected
    QQueue<Ekos::GuideState> expectedGuidingStates;
    // sequence of meridian flip states that are expected
    QQueue<Ekos::Mount::MeridianFlipStatus> expectedMeridianFlipStates;

    // regular focusing on?
    bool refocus_checked = false;
    // HFR autofocus on?
    bool autofocus_checked = false;
    // regular dithering on?
    bool dithering_checked = false;

    /**
     * @brief Retrieve the current alignment status.
     */
    inline Ekos::AlignState getAlignStatus() {return m_AlignStatus;}
    /**
     * @brief Retrieve the current capture status.
     */
    inline Ekos::CaptureState getCaptureStatus() {return m_CaptureStatus;}
    /**
     * @brief Retrieve the current focus status.
     */
    inline Ekos::FocusState getFocusStatus() {return m_FocusStatus;}
    /**
     * @brief Retrieve the current guiding status.
     */
    Ekos::GuideState getGuidingStatus() {return m_GuideStatus;};
    /**
     * @brief Retrieve the current mount meridian flip status.
     */
    Ekos::Mount::MeridianFlipStatus getMeridianFlipStatus() {return m_MFStatus;};

        
protected slots:
    void initTestCase();
    void cleanupTestCase();

    void init();
    void cleanup();


private:
    // current alignment status
    Ekos::AlignState m_AlignStatus { Ekos::ALIGN_IDLE };

    // current capture status
    Ekos::CaptureState m_CaptureStatus { Ekos::CAPTURE_IDLE };

    // current focus status
    Ekos::FocusState m_FocusStatus { Ekos::FOCUS_IDLE };

    // current guiding status
    Ekos::GuideState m_GuideStatus { Ekos::GUIDE_IDLE };

    /**
     * @brief Slot to track the align status of the mount
     * @param status new align state
     */
    void alignStatusChanged(Ekos::AlignState status);

    /**
     * @brief Slot to track the meridian flip stage of the mount
     * @param status new meridian flip state
     */
    void meridianFlipStatusChanged(Ekos::Mount::MeridianFlipStatus status);

    /**
     * @brief Slot to track the focus status
     * @param status new focus status
     */
    void focusStatusChanged(Ekos::FocusState status);

    /**
     * @brief Slot to track the guiding status
     * @param status new guiding status
     */
    void guidingStatusChanged(Ekos::GuideState status);

    /**
     * @brief Slot to track the capture status
     * @param status new capture status
     */
    void captureStatusChanged(Ekos::CaptureState status);

};

#endif // HAVE_INDI

