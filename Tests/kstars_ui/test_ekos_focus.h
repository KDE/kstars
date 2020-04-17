/*  KStars UI tests
    Copyright (C) 2020
    Eric Dejouhanet <eric.dejouhanet@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef TESTEKOSFOCUS_H
#define TESTEKOSFOCUS_H

#include "config-kstars.h"

#if defined(HAVE_INDI)

#include <QObject>
#include <QPushButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QtTest>

/** @brief Helper to retrieve a gadget in the Focus tab specifically.
 * @param klass is the class of the gadget to look for.
 * @param name is the gadget name to look for in the UI configuration.
 * @warning Fails the test if the gadget "name" of class "klass" does not exist in the Focus module
 */
#define KTRY_FOCUS_GADGET(klass, name) klass * const name = Ekos::Manager::Instance()->focusModule()->findChild<klass*>(#name); \
    QVERIFY2(name != nullptr, QString(#klass " '%1' does not exist and cannot be used").arg(#name).toStdString().c_str())

/** @brief Helper to click a button in the Focus tab specifically.
 * @param button is the gadget name of the button to look for in the UI configuration.
 * @warning Fails the test if the button is not currently enabled.
 */
#define KTRY_FOCUS_CLICK(button) do { \
    QTimer::singleShot(200, Ekos::Manager::Instance(), []() { \
        KTRY_FOCUS_GADGET(QPushButton, button); \
        QVERIFY2(button->isEnabled(), QString("QPushButton '%1' is disabled and cannot be clicked").arg(#button).toStdString().c_str()); \
        QTest::mouseClick(button, Qt::LeftButton); }); } while(false)

/** @brief Helper to set a string text into a QComboBox in the Focus module.
 * @param combobox is the gadget name of the QComboBox to look for in the UI configuration.
 * @param text is the string text to set in the gadget.
 * @note This is a contrived method to set a text into a QComboBox programmatically *and* emit the "activated" message.
 * @warning Fails the test if the name does not exist in the Focus UI or if the text cannot be set in the gadget.
 */
#define KTRY_FOCUS_COMBO_SET(combobox, text) do { \
    KTRY_FOCUS_GADGET(QComboBox, combobox); \
    int const cbIndex = combobox->findText(text); \
    QVERIFY(0 <= cbIndex); \
    combobox->setCurrentIndex(cbIndex); \
    combobox->activated(cbIndex); \
    QCOMPARE(combobox->currentText(), QString(text)); } while(false);

/** @brief Helper for exposure.
 * @param exposure is the amount of seconds to expose for.
 * @param averaged is the number of frames the procedure should average before computation.
 * @note The Focus capture button is disabled during exposure.
 * @warning Fails the test if the exposure cannot be entered or if the capture button is
 * disabled or does not toggle during exposure or if the stop button is not the opposite of the capture button.
 */
#define KTRY_FOCUS_CAPTURE(exposure, averaged) do { \
    KTRY_FOCUS_GADGET(QDoubleSpinBox, exposureIN); \
    exposureIN->setValue(exposure); \
    KTRY_FOCUS_GADGET(QSpinBox, focusFramesSpin); \
    focusFramesSpin->setValue(averaged); \
    KTRY_FOCUS_GADGET(QPushButton, captureB); \
    KTRY_FOCUS_GADGET(QPushButton, stopFocusB); \
    QTRY_VERIFY_WITH_TIMEOUT(captureB->isEnabled(), 1000); \
    QTRY_VERIFY_WITH_TIMEOUT(!stopFocusB->isEnabled(), 1000); \
    KTRY_FOCUS_CLICK(captureB); \
    QTRY_VERIFY_WITH_TIMEOUT(!captureB->isEnabled(), 1000); \
    QTRY_VERIFY_WITH_TIMEOUT(stopFocusB->isEnabled(), 1000); \
    QTest::qWait(1.2*exposure*averaged); \
    QTRY_VERIFY_WITH_TIMEOUT(captureB->isEnabled(), 5000); \
    QTRY_VERIFY_WITH_TIMEOUT(!stopFocusB->isEnabled(), 5000); } while (false)

/** brief Helper to configure main star detection parameters.
 * @param detection is the name of the star detection method to use.
 * @param algorithm is the name of the autofocus algorithm to use.
 * @param fieldin is the lower radius of the annulus filtering stars.
 * @param fieldout is the upper radius of the annulus filtering stars.
 * @warning Fails the test if detection, algorithm, full-field checkbox or annulus fields cannot be used.
 */
#define KTRY_FOCUS_CONFIGURE(detection, algorithm, fieldin, fieldout) do { \
    KTRY_FOCUS_GADGET(QCheckBox, useFullField); \
    useFullField->setCheckState(Qt::CheckState::Checked); \
    KTRY_FOCUS_GADGET(QDoubleSpinBox, fullFieldInnerRing); \
    fullFieldInnerRing->setValue(fieldin); \
    KTRY_FOCUS_GADGET(QDoubleSpinBox, fullFieldOuterRing); \
    fullFieldOuterRing->setValue(fieldout); \
    KTRY_FOCUS_COMBO_SET(focusDetectionCombo, detection); \
    KTRY_FOCUS_COMBO_SET(focusAlgorithmCombo, algorithm); } while (false)

class TestEkosFocus : public QObject
{
    Q_OBJECT

public:
    explicit TestEkosFocus(QObject *parent = nullptr);

private slots:
    void initTestCase();
    void cleanupTestCase();

    void init();
    void cleanup();

    void testStarDetection_data();
    void testStarDetection();
};

#endif // HAVE_INDI
#endif // TESTEKOSFOCUS_H
