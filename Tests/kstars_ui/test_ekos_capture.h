/*  KStars UI tests
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TESTEKOSCAPTURE_H
#define TESTEKOSCAPTURE_H

#include "config-kstars.h"

#if defined(HAVE_INDI)

#include <QObject>
#include <QPushButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QtTest>

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
        QVERIFY2(button->isEnabled(), QString("QPushButton '%1' is disabled and cannot be clicked").arg(#button).toStdString().c_str()); \
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

/** @brief Helper to configure a Light frame.
 * @param exposure is the exposure duration.
 * @param count is the number of exposures to execute.
 * @param delay is the delay after exposure.
 * @param filter is the filter name to set.
 * @param destination is the folder to store fames to.
 */
#define KTRY_CAPTURE_CONFIGURE_LIGHT(exposure, count, delay, filter, destination) do { \
    KTRY_CAPTURE_GADGET(QDoubleSpinBox, captureExposureN); \
    captureExposureN->setValue((double)(exposure)); \
    KTRY_CAPTURE_GADGET(QSpinBox, captureCountN); \
    captureCountN->setValue((int)(count)); \
    KTRY_CAPTURE_GADGET(QSpinBox, captureDelayN); \
    captureDelayN->setValue((int)(delay)); \
    KTRY_CAPTURE_GADGET(QComboBox, captureTypeS); \
    KTRY_CAPTURE_COMBO_SET(captureTypeS, "Light"); \
    KTRY_CAPTURE_GADGET(QComboBox, captureFormatS); \
    KTRY_CAPTURE_COMBO_SET(captureFormatS, "FITS"); \
    KTRY_CAPTURE_GADGET(QComboBox, captureFilterS); \
    KTRY_CAPTURE_COMBO_SET(captureFilterS, (filter)); \
    KTRY_CAPTURE_GADGET(QLineEdit, fileDirectoryT); \
    fileDirectoryT->setText(destination); } while(false)

/** @brief Helper to add a Light frame to a Capture job.
 * @param exposure is the exposure duration.
 * @param count is the number of exposures to execute.
 * @param delay is the delay after exposure.
 * @param filter is the filter name to set.
 * @param destination is the folder to store fames to.
 */
#define KTRY_CAPTURE_ADD_LIGHT(exposure, count, delay, filter, destination) do { \
    KTRY_CAPTURE_GADGET(QTableWidget, queueTable); \
    int const jcount = queueTable->rowCount(); \
    KTRY_CAPTURE_CONFIGURE_LIGHT(exposure, count, delay, filter, destination); \
    KTRY_CAPTURE_CLICK(addToQueueB); \
    QTRY_VERIFY_WITH_TIMEOUT(queueTable->rowCount() == (jcount+1), 1000); } while(false);

class TestEkosCapture : public QObject
{
    Q_OBJECT

public:
    explicit TestEkosCapture(QObject *parent = nullptr);

private:
    QStringList searchFITS(QDir const &dir) const;

private slots:
    void initTestCase();
    void cleanupTestCase();

    void init();
    void cleanup();

    /** @brief Test addition, UI sync and removal of capture jobs. */
    void testAddCaptureJob();

    /** @brief Test that storing to a system temporary folder makes the capture a preview. */
    void testCaptureToTemporary();

    /** @brief Test capturing a single frame in multiple attempts. */
    void testCaptureSingle();

    /** @brief Test capturing multiple frames in multiple attempts. */
    void testCaptureMultiple();
};

#endif // HAVE_INDI
#endif // TESTEKOSCAPTURE_H
