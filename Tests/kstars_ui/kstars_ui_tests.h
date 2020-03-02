/*  KStars UI tests
    Copyright (C) 2017 Csaba Kertesz <csaba.kertesz@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "config-kstars.h"
#include "kstars.h"
#include "kstarsdata.h"

#include <QMutex>
#include <QObject>
#include <QTimer>
#include <QApplication>
#include <QtTest>

#include "kstars.h"

class KStars;

#define KTRY_SHOW_KSTARS() do { \
    KStars * const K = KStars::Instance(); \
    QVERIFY(K != nullptr); \
    QTRY_VERIFY_WITH_TIMEOUT((K)->isGUIReady(), 30000); \
    (K)->raise(); \
    QTRY_VERIFY_WITH_TIMEOUT((K)->isActiveWindow(), 1000); } while(false)

#define KTRY_ACTION(action_text) do { \
    QAction * const action = KStars::Instance()->actionCollection()->action(action_text); \
    QVERIFY2(action != nullptr, QString("Action '%1' is not registered and cannot be triggered").arg(action_text).toStdString().c_str()); \
    action->trigger(); } while(false)

class KStarsUiTests : public QObject
{
    Q_OBJECT
public:
    explicit KStarsUiTests(QObject *parent = nullptr);

public:
    static struct _InitialConditions
    {
        QDateTime dateTime;
        bool clockRunning;

        _InitialConditions():
            dateTime(QDate(2020, 01, 01), QTime(23, 0, 0), Qt::UTC),
            clockRunning(false) {};
    }
    const m_InitialConditions;

private slots:
    void initTestCase();
    void cleanupTestCase();

    void init();
    void cleanup();

    void createInstanceTest();

    void initialConditionsTest();
    void raiseKStarsTest();
};
