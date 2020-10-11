/*  KStars UI tests
    Copyright (C) 2020
    Eric Dejouhanet <eric.dejouhanet@gmail.com>
    Fabrizio Pollastri <mxgbot@gmail.com> 2020-08-30

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef TESTEKOSMOUNT_H
#define TESTEKOSMOUNT_H

#include "config-kstars.h"
#include "test_ekos.h"

#if defined(HAVE_INDI)

#include <QObject>
#include <QPushButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QtTest>

/** @brief Helper to retrieve a gadget in the Mount tab specifically.
 * @param klass is the class of the gadget to look for.
 * @param name is the gadget name to look for in the UI configuration.
 * @warning Fails the test if the gadget "name" of class "klass" does not exist in the Mount module
 */
#define KTRY_MOUNT_GADGET(klass, name) klass * const name = Ekos::Manager::Instance()->mountModule()->findChild<klass*>(#name); \
    QVERIFY2(name != nullptr, QString(#klass " '%1' does not exist and cannot be used").arg(#name).toStdString().c_str())

/** @brief Helper to click a button in the Mount tab specifically.
 * @param button is the gadget name of the button to look for in the UI configuration.
 * @warning Fails the test if the button is not currently enabled.
 */
#define KTRY_MOUNT_CLICK(button) do { \
    QTimer::singleShot(100, Ekos::Manager::Instance(), []() { \
        KTRY_MOUNT_GADGET(QPushButton, button); \
        QVERIFY2(button->isEnabled(), QString("QPushButton '%1' is disabled and cannot be clicked").arg(#button).toStdString().c_str()); \
        QTest::mouseClick(button, Qt::LeftButton); }); \
    QTest::qWait(200); } while(false)


class TestEkosMount : public QObject
{
    Q_OBJECT

public:
    explicit TestEkosMount(QObject *parent = nullptr);

private slots:
    void initTestCase();
    void cleanupTestCase();

    void init();
    void cleanup();

    void testMountCtrlCoordLabels();
    void testMountCtrlCoordConversion();
    void testMountCtrlGoto();
    void testMountCtrlSync();

private:
    Ekos::Manager *ekos;
    QWindow *mountControl;
    QObject *raLabel, *deLabel, *raText, *deText,
        *coordRaDe, *coordAzAl, *coordHaDe,
        *raValue, *deValue, *azValue, *altValue, *haValue, *zaValue,
        *gotoButton, *syncButton;
    double degreePrecision = 2 * 1.0 / 3600.0;
    double hourPrecision = 2 * 15.0 / 3600.0;


};

#endif // HAVE_INDI
#endif // TESTEKOSMOUNT_H
