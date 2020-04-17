/*  KStars UI tests
    Copyright (C) 2018, 2020
    Csaba Kertesz <csaba.kertesz@gmail.com>
    Jasem Mutlaq <knro@ikarustech.com>
    Eric Dejouhanet <eric.dejouhanet@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef TEST_EKOS_H
#define TEST_EKOS_H

#include "config-kstars.h"

#if defined(HAVE_INDI)

#include <KActionCollection>
#include <QtTest>
#include "kstars.h"
#include "ekos/manager.h"
#include "test_kstars_startup.h"

#define KVERIFY_EKOS_IS_HIDDEN() do { \
    if (Ekos::Manager::Instance() != nullptr) { \
        QVERIFY(!Ekos::Manager::Instance()->isVisible()); \
        QVERIFY(!Ekos::Manager::Instance()->isActiveWindow()); }} while(false)

#define KVERIFY_EKOS_IS_OPENED() do { \
    QVERIFY(Ekos::Manager::Instance() != nullptr); \
    QVERIFY(Ekos::Manager::Instance()->isVisible()); \
    QVERIFY(Ekos::Manager::Instance()->isActiveWindow()); } while(false)

#define KTRY_OPEN_EKOS() do { \
    if (Ekos::Manager::Instance() == nullptr || !Ekos::Manager::Instance()->isVisible()) { \
        KTRY_ACTION("show_ekos"); \
        QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance() != nullptr, 200); \
        QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->isVisible(), 200); \
        QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->isActiveWindow(), 200); }} while(false)

#define KTRY_CLOSE_EKOS() do { \
    if (Ekos::Manager::Instance() != nullptr && Ekos::Manager::Instance()->isVisible()) { \
        KTRY_ACTION("show_ekos"); \
        QTRY_VERIFY_WITH_TIMEOUT(!Ekos::Manager::Instance()->isActiveWindow(), 200); \
        QTRY_VERIFY_WITH_TIMEOUT(!Ekos::Manager::Instance()->isVisible(), 200); }} while(false)

#define KHACK_RESET_EKOS_TIME() do { \
    QWARN("HACK HACK HACK: Reset clock to initial conditions when starting Ekos"); \
    if (KStars::Instance() != nullptr) \
        if (KStars::Instance()->data() != nullptr) \
            KStars::Instance()->data()->clock()->setUTC(KStarsDateTime(TestKStarsStartup::m_InitialConditions.dateTime)); } while(false)

class TestEkos: public QObject
{
    Q_OBJECT
public:
    explicit TestEkos(QObject *parent = nullptr);

private slots:
    void initTestCase();
    void cleanupTestCase();

    void init();
    void cleanup();

    void testOpenClose();
    void testSimulatorProfile();
    void testManipulateProfiles();
};

#endif // HAVE_INDI
#endif // TEST_EKOS_H
