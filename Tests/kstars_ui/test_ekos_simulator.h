/*  KStars UI tests
    Copyright (C) 2020
    Eric Dejouhanet <eric.dejouhanet@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef TESTEKOSSIMULATOR_H
#define TESTEKOSSIMULATOR_H

#include "config-kstars.h"

#ifdef HAVE_INDI

#include <QObject>
#include <KActionCollection>

#define KTRY_EKOS_SELECT_PROFILE(profile) do { \
    QString const p(profile); \
    QComboBox* profileCBox = Ekos::Manager::Instance()->findChild<QComboBox*>("profileCombo"); \
    QVERIFY(profileCBox != nullptr); \
    profileCBox->setCurrentText(p); \
    QTRY_COMPARE(profileCBox->currentText(), p); } while(false)

#define KTRY_EKOS_CLICK(button) do { \
    QPushButton * const b = Ekos::Manager::Instance()->findChild<QPushButton*>(button); \
    QVERIFY2(b != nullptr, QString("QPushButton '%1' does not exist and cannot be clicked").arg(button).toStdString().c_str()); \
    QVERIFY2(b->isEnabled(), QString("QPushButton '%1' is disabled and cannot be clicked").arg(button).toStdString().c_str()); \
    QTimer::singleShot(200, Ekos::Manager::Instance(), [&]() { QTest::mouseClick(b, Qt::LeftButton); }); } while(false)

#define KTRY_EKOS_START_SIMULATORS() do { \
    KTRY_EKOS_SELECT_PROFILE("Simulators"); \
    KTRY_EKOS_CLICK("processINDIB"); \
    QWARN("HACK HACK HACK adding delay here for devices to connect"); \
    QTest::qWait(5000); } while(false)

#define KTRY_EKOS_STOP_SIMULATORS() do { \
    KTRY_EKOS_CLICK("disconnectB"); \
    QWARN("Intentionally leaving a delay here for BZ398192"); \
    QTest::qWait(5000); \
    KTRY_EKOS_CLICK("processINDIB"); \
    QTest::qWait(1000); } while(false)

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
};

#endif
#endif // TESTEKOSSIMULATOR_H
