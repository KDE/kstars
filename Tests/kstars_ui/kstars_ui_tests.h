/*  KStars UI tests
    Copyright (C) 2017 Csaba Kertesz <csaba.kertesz@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "config-kstars.h"

#include <QMutex>
#include <QObject>
#include <QTimer>
#include <QApplication>
#include <QTest>

#include "kstars.h"

class KStars;

#define KTRY_SHOW_KSTARS(K) do { \
    QTRY_VERIFY_WITH_TIMEOUT((K)->isGUIReady(), 1000); \
    (K)->raise(); \
    QTRY_VERIFY_WITH_TIMEOUT((K)->isActiveWindow(), 1000); } while(false)

class KStarsUiTests : public QObject
{
    Q_OBJECT

public:
    KStarsUiTests();
    ~KStarsUiTests() override = default;

private:
    KStars* K {nullptr};

private slots:
    void initTestCase();
    void cleanupTestCase();

    void init();
    void cleanup();

    void basicTest();
    void warnTest();
    void raiseKStarsTest();

#if defined(HAVE_INDI)
private:
    QString testProfileName;
    void initEkos();
    void cleanupEkos();

private slots:
    // UI
    void openEkosTest();

    // Profiles
    void manipulateEkosProfiles();
#endif
};
