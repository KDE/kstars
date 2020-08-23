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

#define KTRY_EKOS_START_SIMULATORS() do { \
    KTRY_EKOS_SELECT_PROFILE("Simulators"); \
    KTRY_EKOS_CLICK(processINDIB); \
    QWARN("HACK HACK HACK adding delay here for devices to connect"); \
    QTest::qWait(10000); } while(false)

#define KTRY_EKOS_STOP_SIMULATORS() do { \
    KTRY_EKOS_CLICK(disconnectB); \
    KTRY_EKOS_GADGET(QPushButton, processINDIB); \
    QTRY_VERIFY_WITH_TIMEOUT(processINDIB->isEnabled(), 5000); \
    KTRY_EKOS_CLICK(processINDIB); } while(false)

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

#endif // HAVE_INDI
#endif // TESTEKOSSIMULATOR_H
