/*  KStars UI tests
    Copyright (C) 2020
    Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TestEkosFilterWheel_H
#define TestEkosFilterWheel_H

#include "config-kstars.h"

#if defined(HAVE_INDI)

#include <QObject>
#include <QPushButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QTest>

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
#define KTRY_CAPTURE_COMBO_SET(combobox, text) { \
    int __ktry_combo_iter__=0; \
    do { \
      KTRY_CAPTURE_GADGET(QComboBox, combobox); \
      int const cbIndex = combobox->findText(text); \
      if (cbIndex >= 0) { \
        combobox->setCurrentIndex(cbIndex); \
        combobox->activated(cbIndex); \
        QCOMPARE(combobox->currentText(), QString(text)); \
        break; \
      } else { \
        QVERIFY(__ktry_combo_iter__++ < 3); \
        QTest::qWait(2000); \
      } \
    } while(true);}

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
    KTRY_CAPTURE_GADGET(QDoubleSpinBox, captureExposureN); \
    captureExposureN->setValue((double)(exposure)); \
    KTRY_CAPTURE_GADGET(QSpinBox, captureCountN); \
    captureCountN->setValue((int)(count)); \
    KTRY_CAPTURE_GADGET(QSpinBox, captureDelayN); \
    captureDelayN->setValue((int)(delay)); \
    KTRY_CAPTURE_GADGET(QComboBox, captureTypeS); \
    KTRY_CAPTURE_COMBO_SET(captureTypeS, "Light"); \
    KTRY_CAPTURE_GADGET(QComboBox, captureEncodingS); \
    KTRY_CAPTURE_COMBO_SET(captureEncodingS, "FITS"); \
    KTRY_CAPTURE_GADGET(QComboBox, FilterPosCombo); \
    KTRY_CAPTURE_COMBO_SET(FilterPosCombo, (filter)); \
    KTRY_CAPTURE_GADGET(QLineEdit, fileDirectoryT); \
    fileDirectoryT->setText(destination); \
    KTRY_CAPTURE_CLICK(addToQueueB); \
    QTRY_VERIFY_WITH_TIMEOUT(queueTable->rowCount() == (jcount+1), 1000); } while(false);

class TestEkosFilterWheel : public QObject
{
        Q_OBJECT

    public:
        explicit TestEkosFilterWheel(QObject *parent = nullptr);

    protected:
        QString testProfileName { "filter_sync_test_profile" };
    private:
        QFileInfoList searchFITS(QDir const &dir) const;

    private slots:
        void initTestCase();
        void cleanupTestCase();

        void init();
        void cleanup();

        /** @brief Test the filter wheel is set in ACTIVE_DEVICE and generated FITS header contains the matching filter. */
        void testFilterWheelSync();

};

#endif // HAVE_INDI
#endif // TestEkosFilterWheel_H
