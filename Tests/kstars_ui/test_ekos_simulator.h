/*  KStars UI tests
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TESTEKOSSIMULATOR_H
#define TESTEKOSSIMULATOR_H

#include "config-kstars.h"

#ifdef HAVE_INDI

#include <QObject>
#include <QString>
#include <QComboBox>
#include <QTimer>
#include <QPushButton>
#include <QtTest>
#include "ekos/manager.h"

#define KTRY_EKOS_SELECT_PROFILE(profile) do { \
    QString const p(profile); \
    QComboBox* profileCBox = Ekos::Manager::Instance()->findChild<QComboBox*>("profileCombo"); \
    QVERIFY(profileCBox != nullptr); \
    profileCBox->setCurrentText(p); \
    QTRY_COMPARE(profileCBox->currentText(), p); } while(false)

#define KTRY_EKOS_GADGET(klass, name) klass * const name = Ekos::Manager::Instance()->findChild<klass*>(#name); \
    QVERIFY2(name != nullptr, QString(#klass "'%1' does not exist and cannot be used").arg(#name).toStdString().c_str())

#define KTRY_EKOS_CLICK(button) do { \
    QTimer::singleShot(100, Ekos::Manager::Instance(), []() { \
        KTRY_EKOS_GADGET(QPushButton, button); \
        QVERIFY2(button->isEnabled(), QString("QPushButton '%1' is disabled and cannot be clicked").arg(#button).toStdString().c_str()); \
        QTest::mouseClick(button, Qt::LeftButton); }); \
    QTest::qWait(200); } while(false)

#define KTRY_EKOS_START_PROFILE(profile) do { \
    KTRY_EKOS_SELECT_PROFILE(profile); \
    KTRY_EKOS_CLICK(processINDIB); \
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->indiStatus() == Ekos::Success, 10000); \
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->mountModule() != nullptr, 1000); \
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->mountModule()->slewStatus() != IPState::IPS_ALERT, 1000); \
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->guideModule() != nullptr, 1000); \
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->captureModule() != nullptr, 1000); } while(false)

#define KTRY_EKOS_START_SIMULATORS() do { KTRY_EKOS_START_PROFILE("Simulators"); } while (false)

#define KTRY_EKOS_STOP_SIMULATORS() do { \
    KTRY_EKOS_CLICK(disconnectB); \
    KTRY_EKOS_GADGET(QPushButton, processINDIB); \
    QTRY_VERIFY_WITH_TIMEOUT(processINDIB->isEnabled(), 5000); \
    KTRY_EKOS_CLICK(processINDIB); \
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->indiStatus() == Ekos::Idle, 10000); } while(false)

class TestEkosSimulator : public QObject
{
    Q_OBJECT
public:
    explicit TestEkosSimulator(QObject *parent = nullptr);

private slots:
    void initTestCase();
    void cleanupTestCase();

    void init();
    void cleanup();

    void testMountSlew_data();
    void testMountSlew();

    void testColorSchemes_data();
    void testColorSchemes();
};

#endif // HAVE_INDI
#endif // TESTEKOSSIMULATOR_H
