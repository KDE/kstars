/*
    Helper class of KStars UI capture tests

    SPDX-FileCopyrightText: 2020 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "config-kstars.h"
#include "test_ekos_debug.h"
#include "test_ekos_helper.h"
#include "test_ekos_simulator.h"

#include "ekos/profileeditor.h"
#include "ekos/capture/placeholderpath.h"

#include <QObject>

#pragma once

/** @brief Helper to retrieve a gadget in KStars.
 * @param klass is the class of the gadget to look for.
 * @param name is the gadget name to look for in the UI configuration.
 * @warning Fails the test if the gadget "name" of class "klass" does not exist in the Capture module
 */
#define KTRY_KSTARS_GADGET(klass, name) klass * const name = KStars::Instance()->findChild<klass*>(#name); \
    QVERIFY2(name != nullptr, QString(#klass " '%1' does not exist and cannot be used").arg(#name).toStdString().c_str())

/** @brief Helper to show the Capture tab
 */
#define KTRY_CAPTURE_SHOW() do { \
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->captureModule() != nullptr, 5000); \
    KTRY_EKOS_GADGET(QTabWidget, toolsWidget); \
    toolsWidget->setCurrentWidget(Ekos::Manager::Instance()->captureModule()); \
    QTRY_COMPARE_WITH_TIMEOUT(toolsWidget->currentWidget(), Ekos::Manager::Instance()->captureModule(), 5000); \
    QTRY_VERIFY_WITH_TIMEOUT(!Ekos::Manager::Instance()->captureModule()->camera().isEmpty(), 5000); } while (false)

/** @brief Helper to retrieve a gadget in the Capture tab specifically.
 * @param klass is the class of the gadget to look for.
 * @param name is the gadget name to look for in the UI configuration.
 * @warning Fails the test if the gadget "name" of class "klass" does not exist in the Capture module
 */
#define KTRY_CAPTURE_GADGET(klass, name) klass * const name = Ekos::Manager::Instance()->captureModule()->findChild<klass*>(#name); \
    QVERIFY2(name != nullptr, QString(#klass " '%1' does not exist and cannot be used").arg(#name).toStdString().c_str())


/** @brief Helper to click a button in the Capture tab specifically.
 * @param button is the gadget name of the button to look for in the UI configuration.
 * @warning Fails the test if the button is not currently enabled.
 */
#define KTRY_CAPTURE_CLICK(button) do { \
    QTimer::singleShot(100, Ekos::Manager::Instance(), []() { \
        KTRY_CAPTURE_GADGET(QPushButton, button); \
        QTRY_VERIFY2_WITH_TIMEOUT(button->isEnabled(), QString("QPushButton '%1' is disabled and cannot be clicked").arg(#button).toStdString().c_str(), 10000); \
        QTest::mouseClick(button, Qt::LeftButton); }); \
    QTest::qWait(200); } while(false)

/** @brief Helper to set a string text into a QComboBox in the Capture module.
 * @param combobox is the gadget name of the QComboBox to look for in the UI configuration.
 * @param text is the string text to set in the gadget.
 * @note This is a contrived method to set a text into a QComboBox programmatically *and* emit the "activated" message.
 * @warning Fails the test if the name does not exist in the Capture UI or if the text cannot be set in the gadget.
 */
#define KTRY_CAPTURE_COMBO_SET(combobox, text) do { \
    KTRY_CAPTURE_GADGET(QComboBox, combobox); \
    int const cbIndex = combobox->findText(text); \
    QVERIFY(0 <= cbIndex); \
    combobox->setCurrentIndex(cbIndex); \
    combobox->activated(cbIndex); \
    QCOMPARE(combobox->currentText(), QString(text)); } while(false);

/** @brief Helper to configure a frame.
 * @param frametype Light, Dark, Bias or Flat
 * @param exposure is the exposure duration.
 * @param count is the number of exposures to execute.
 * @param delay is the delay after exposure.
 * @param filter is the filter name to set.
 * @param target name of the target.
 * @param destination is the folder to store fames to.
 */
#define KTRY_CAPTURE_CONFIGURE_FRAME(frametype, exposure, count, delay, filter, target, destination) do { \
    KTRY_CAPTURE_GADGET(QDoubleSpinBox, captureExposureN); \
    captureExposureN->setValue(static_cast<double>(exposure)); \
    KTRY_CAPTURE_GADGET(QSpinBox, captureCountN); \
    captureCountN->setValue(static_cast<int>(count)); \
    KTRY_CAPTURE_GADGET(QSpinBox, captureDelayN); \
    captureDelayN->setValue(static_cast<int>(delay)); \
    KTRY_CAPTURE_GADGET(QComboBox, captureTypeS); \
    KTRY_CAPTURE_COMBO_SET(captureTypeS, frametype); \
    KTRY_CAPTURE_GADGET(QComboBox, FilterPosCombo); \
    KTRY_CAPTURE_COMBO_SET(FilterPosCombo, (filter)); \
    KTRY_CAPTURE_GADGET(QLineEdit, targetNameT); \
    targetNameT->setText(target); \
    KTRY_CAPTURE_GADGET(QLineEdit, fileDirectoryT); \
    fileDirectoryT->setText(destination); } while(false)

/** @brief Helper to add a frame to a Capture job.
 * @param frametype Light, Dark, Bias or Flat
 * @param exposure is the exposure duration.
 * @param count is the number of exposures to execute.
 * @param delay is the delay after exposure.
 * @param filter is the filter name to set.
 * @param target name of the target.
 * @param destination is the folder to store fames to.
 */
#define KTRY_CAPTURE_ADD_FRAME(frametype, exposure, count, delay, filter, target, destination) do { \
    KTRY_CAPTURE_GADGET(QTableWidget, queueTable); \
    int const jcount = queueTable->rowCount(); \
    KTRY_CAPTURE_CONFIGURE_FRAME(frametype, exposure, count, delay, filter, target, destination); \
    KTRY_CAPTURE_CLICK(addToQueueB); \
    QTRY_VERIFY_WITH_TIMEOUT(queueTable->rowCount() == (jcount+1), 1000); } while(false);


/** @brief Helper to configure a Light frame.
 * @param exposure is the exposure duration.
 * @param count is the number of exposures to execute.
 * @param delay is the delay after exposure.
 * @param filter is the filter name to set.
 * @param target name of the target.
 * @param destination is the folder to store fames to.
 */
#define KTRY_CAPTURE_CONFIGURE_LIGHT(exposure, count, delay, filter, target, destination) \
    KTRY_CAPTURE_CONFIGURE_FRAME("Light", exposure, count, delay, filter, target, destination)

/** @brief Helper to add a Light frame to a Capture job.
 * @param exposure is the exposure duration.
 * @param count is the number of exposures to execute.
 * @param delay is the delay after exposure.
 * @param filter is the filter name to set.
 * @param target name of the target.
 * @param destination is the folder to store fames to.
 */
#define KTRY_CAPTURE_ADD_LIGHT(exposure, count, delay, filter, target, destination) \
    KTRY_CAPTURE_ADD_FRAME("Light", exposure, count, delay, filter, target, destination)


/** @brief Helper to configure a Flat frame.
 * @param exposure is the exposure duration.
 * @param count is the number of exposures to execute.
 * @param delay is the delay after exposure.
 * @param filter is the filter name to set.
 * @param destination is the folder to store fames to.
 */
#define KTRY_CAPTURE_CONFIGURE_FLAT(exposure, count, delay, filter, destination) \
    KTRY_CAPTURE_CONFIGURE_FRAME("Flat", exposure, count, delay, filter, "", destination)

/** @brief Helper to add a flat frame to a Capture job.
 * @param exposure is the exposure duration.
 * @param count is the number of exposures to execute.
 * @param delay is the delay after exposure.
 * @param filter is the filter name to set.
 * @param destination is the folder to store fames to.
 */
#define KTRY_CAPTURE_ADD_FLAT(exposure, count, delay, filter, destination) \
    KTRY_CAPTURE_ADD_FRAME("Flat", exposure, count, delay, filter, "", destination)



class TestEkosCaptureHelper : public TestEkosHelper
{

public:

    struct OptDouble {
        bool enabled = false;
        double value = 0.0;
    };

    struct OptInt {
        bool enabled = false;
        double value = 0;
    };

    typedef enum {
        ESQ_VERSION_2_4,
        ESQ_VERSION_2_5,
        ESQ_VERSION_2_6
    } ESQVersion;

    const QList<QString> esqVersionNames = {"2.4", "2.5", "2.6"};

    struct CaptureSettings {
        QString observer = "";
        OptDouble guideDeviation, startGuideDeviation;
        OptDouble inSequenceFocus, autofocusOnTemperature;
        OptInt refocusEveryN;
        bool refocusAfterMeridianFlip = false;
    };

    struct SimpleCaptureLightsJob
    {
        ESQVersion version = ESQ_VERSION_2_6;
        int exposureTime = 1.0;
        int count = 1;
        int delayMS = 0; // delay in milliseconds
        int binX = 1, binY = 1;
        int x = 0, y = 0, w = 1280, h = 1080;
        QString filterName = "Luminance";
        QString type = "Light";
        QString encoding = "FITS";
        QString targetName = "test";
        QString fitsDirectory = "/home/pi";
        QString placeholderFormat = "/%t/%T/%F/%t_%T_%F_%e_%D";
        int formatSuffix = 3;
        OptDouble cameraTemperature = {false, 20.0};
        int uploadMode = 0;
    };

    struct SimpleCaptureCalibratingJob
    {
        ESQVersion version = ESQ_VERSION_2_6;
        int exposureTime = 1.0;
        QString type = "Flat";
        int count = 1;
        uint preAction = 0;
        double wall_az = 90, wall_alt = 0;
        bool duration_manual = true, duration_adu = false;
        int adu = 10000, tolerance = 1000;
    };

    explicit TestEkosCaptureHelper(QString guider = nullptr);

    /**
     * @brief Initialization ahead of executing the test cases.
     */
    void init() override;

    /**
     * @brief Cleanup after test cases have been executed.
     */
    void cleanup() override;

    /**
     * @brief Helper function for start of capturing
     * @param checkCapturing set to true if check of capturing should be included
     */
    bool startCapturing(bool checkCapturing = true);

    /**
     * @brief Helper function to stop capturing
     */
    bool stopCapturing();

    /**
     * @brief Fill the capture sequences in the Capture GUI
     * @param target capturing target name
     * @param sequence comma separated list of <filter>:<count>
     * @param exptime exposure time
     * @param fitsDirectory directory where the captures will be placed
     * @param format image location for
     * @param delay delay between frame captures
     * @return true if everything was successful
     */
    bool fillCaptureSequences(QString target, QString sequence, double exptime, QString fitsDirectory, int delay = 0, QString format = Ekos::PlaceholderPath::defaultFormat(true, false, false));

    /**
     * @brief Fill the fields of the script manager in the capture module
     * @param scripts Pre-... and post-... scripts
     */
    bool fillScriptManagerDialog(const QMap<Ekos::ScriptTypes, QString> &scripts);

    /**
     * @brief Stop and clean up scheduler
     */
    void cleanupScheduler();

    /**
     * @brief getSimpleEsqContent Create a simple lights capture sequence file
     * @param settings overall settings
     * @param jobs list of sequence jobs
     * @param version file version
     * @return XML string list
     */
    QStringList getSimpleEsqContent(CaptureSettings settings, QVector<SimpleCaptureLightsJob> jobs, ESQVersion version = ESQ_VERSION_2_6);

    /**
     * @brief getSimpleEsqContent Create a simple flats capture sequence file
     * @param settings overall settings
     * @param jobs list of sequence jobs
     * @param version file version
     * @return XML string list
     */
    QStringList getSimpleEsqContent(CaptureSettings settings, QVector<SimpleCaptureCalibratingJob> jobs, ESQVersion version = ESQ_VERSION_2_6);

    /**
     * @brief serializeGeneralSettings Create the XML representation of the general settings
     */
    QStringList serializeGeneralSettings(CaptureSettings settings, ESQVersion version = ESQ_VERSION_2_6);

    /**
     * @brief serializeJob Create the XML representation of a single lights job
     */
    QStringList serializeJob(const SimpleCaptureLightsJob &job, ESQVersion version = ESQ_VERSION_2_6);

    /**
     * @brief serializeJob Create the XML representation of a single flats job
     */
    QStringList serializeJob(const SimpleCaptureCalibratingJob &job, ESQVersion version = ESQ_VERSION_2_6);

    /**
     * @brief calculateSignature Calculate the signature of a given filter
     * @param filter filter name
     * @param fitsDirectory base directory holding all images
     * @return signature
     */
    QString calculateSignature(QString target, QString filter, QString fitsDirectory);

    /**
     * @brief Search for FITS files recursively
     * @param dir starting directory
     * @return list of file names
     */
    QStringList searchFITS(QDir const &dir) const;

    /**
     * @brief Ensure that it is known whether the CCD device has a shutter or not
     * @param shutter set to true iff a shutter should be present
     */
    void ensureCCDShutter(bool shutter);

    QDir *getImageLocation();

    // destination where images will be located
    QTemporaryDir *destination;

private:
    QDir *imageLocation = nullptr;


};
