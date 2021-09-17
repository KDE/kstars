/*  KStars UI tests
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TESTEKOSSCHEDULER_H
#define TESTEKOSSCHEDULER_H

#include "config-kstars.h"

#if defined(HAVE_INDI)

#include <QObject>
#include <QPushButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QtTest>

/** @brief Helper to retrieve a gadget in the Scheduler tab specifically.
 * @param klass is the class of the gadget to look for.
 * @param name is the gadget name to look for in the UI configuration.
 * @warning Fails the test if the gadget "name" of class "klass" does not exist in the Scheduler module
 */
#define KTRY_SCHEDULER_GADGET(klass, name) klass * const name = Ekos::Manager::Instance()->schedulerModule()->findChild<klass*>(#name); \
    QVERIFY2(name != nullptr, QString(#klass " '%1' does not exist and cannot be used").arg(#name).toStdString().c_str())

/** @brief Helper to click a button in the Scheduler tab specifically.
 * @param button is the gadget name of the button to look for in the UI configuration.
 * @warning Fails the test if the button is not currently enabled.
 */
#define KTRY_SCHEDULER_CLICK(button) do { \
    bool clicked = false; \
    QTimer::singleShot(100, Ekos::Manager::Instance(), [&]() { \
        KTRY_SCHEDULER_GADGET(QPushButton, button); \
        QVERIFY2(button->isEnabled(), QString("QPushButton '%1' is disabled and cannot be clicked").arg(#button).toStdString().c_str()); \
        QTest::mouseClick(button, Qt::LeftButton); \
        clicked = true; }); \
    QTRY_VERIFY_WITH_TIMEOUT(clicked, 150); } while(false)

/** @brief Helper to set a string text into a QComboBox in the Scheduler module.
 * @param combobox is the gadget name of the QComboBox to look for in the UI configuration.
 * @param text is the string text to set in the gadget.
 * @note This is a contrived method to set a text into a QComboBox programmatically *and* emit the "activated" message.
 * @warning Fails the test if the name does not exist in the Scheduler UI or if the text cannot be set in the gadget.
 */
#define KTRY_SCHEDULER_COMBO_SET(combobox, text) do { \
    KTRY_SCHEDULER_GADGET(QComboBox, combobox); \
    int const cbIndex = combobox->findText(text); \
    QVERIFY(0 <= cbIndex); \
    combobox->setCurrentIndex(cbIndex); \
    combobox->activated(cbIndex); \
    QCOMPARE(combobox->currentText(), QString(text)); } while(false);

class TestEkosScheduler : public QObject
{
    Q_OBJECT

public:
    explicit TestEkosScheduler(QObject *parent = nullptr);

private slots:
    void initTestCase();
    void cleanupTestCase();

    void init();
    void cleanup();

    void testScheduleManipulation_data();
    void testScheduleManipulation();
};

#endif // HAVE_INDI
#endif // TESTEKOSSCHEDULER_H
