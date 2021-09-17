/*
    SPDX-FileCopyrightText: 2017 Csaba Kertesz <csaba.kertesz@gmail.com>
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "config-kstars.h"

#include <QObject>
#include <QtTest>
#include <QDialogButtonBox>
#include <QtConcurrent>

#include "Options.h"
#include "kstars.h"
#include "kspaths.h"
#include "kswizard.h"
#include <KTipDialog>

#include "kstars_ui_tests.h"
#include "test_kstars_startup.h"


struct TestKStarsStartup::_InitialConditions const TestKStarsStartup::m_InitialConditions;

TestKStarsStartup::TestKStarsStartup(QObject *parent) : QObject(parent)
{
}

void TestKStarsStartup::initTestCase()
{
    if (KStars::Instance() != nullptr)
        KTRY_SHOW_KSTARS();
}

void TestKStarsStartup::cleanupTestCase()
{
    foreach (QDialog * d, KStars::Instance()->findChildren<QDialog*>())
        if (d->isVisible())
            d->hide();
}

void TestKStarsStartup::init()
{
}

void TestKStarsStartup::cleanup()
{
}

void TestKStarsStartup::createInstanceTest()
{
#if defined(HAVE_INDI)
    QWARN("INDI driver registry is unexpectedly required before we start the KStars wizard");

    // Locate INDI drivers like drivermanager.cpp does
    Options::setIndiDriversDir(
        QStandardPaths::locate(QStandardPaths::GenericDataLocation, "indi", QStandardPaths::LocateDirectory));
    QVERIFY(QDir(Options::indiDriversDir()).exists());

    // Look for the second usual place - developer install - OSX should be there too?
    if (QFile("/usr/local/bin/indiserver").exists())
        Options::setIndiServer("/usr/local/bin/indiserver");
    QVERIFY(QFile(Options::indiServer()).exists());
#endif

    // Prepare to close the wizard pages when the KStars instance will start - we could just use the following to bypass
    // Options::setRunStartupWizard(false);
    // Remaining in the timer signal waiting for the app to load actually prevents the app from
    // loading, so retrigger the timer until the app is ready
    volatile bool installWizardDone = false;
    if (Options::runStartupWizard() == true) {
        std::function <void()> closeWizard = [&]
        {
            QTRY_VERIFY_WITH_TIMEOUT(KStars::Instance() != nullptr, 5000);
            KStars * const k = KStars::Instance();
            QVERIFY(k != nullptr);

            // Wait for the KStars Wizard to appear, or retrigger the signal
            if(k->findChild <KSWizard*>() == nullptr)
            {
                QTimer::singleShot(500, KStars::Instance(), closeWizard);
                return;
            }

            // Verify it is a KSWizard that appeared
            KSWizard * const w = k->findChild <KSWizard*>();
            QVERIFY(w != nullptr);
            QTRY_VERIFY_WITH_TIMEOUT(w->isVisible(), 1000);

            // Wait for the New Installation Wizard inside that KSWizard
            QTRY_VERIFY_WITH_TIMEOUT(w->findChild <QWidget*>("WizWelcome") != nullptr, 1000);
            QWidget * ww = KStars::Instance()->findChild <QWidget*>("WizWelcome");
            QTRY_VERIFY_WITH_TIMEOUT(ww->isVisible(), 1000);

            // We could shift to all pages one after the other, but the Next button is difficult to locate, so just dismiss the wizard lazily
            QDialogButtonBox* buttons = w->findChild<QDialogButtonBox*>();
            QVERIFY(nullptr != buttons);

            // search the "Done" button
            QAbstractButton *doneButton;
            for (QAbstractButton *button: buttons->buttons())
            {
                if (button->text().toStdString() == "Done")
                    doneButton = button;
            }
            QVERIFY(nullptr != doneButton);
            QTest::mouseClick(doneButton, Qt::LeftButton);

            installWizardDone = true;
        };
        QTimer::singleShot(500, KStars::Instance(), closeWizard);
    }
    else {
        installWizardDone = true;
    }
    // Initialize our instance and wait for the test to finish
    KTipDialog::setShowOnStart(false);
    KStars::createInstance(true, m_InitialConditions.clockRunning, m_InitialConditions.dateTime.toString());
    QVERIFY(KStars::Instance() != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(installWizardDone, 10000);

    // With our instance created, initialize our location
    // FIXME: do this via UI in the Startup Wizard
    KStarsData * const d = KStars::Instance()->data();
    QVERIFY(d != nullptr);
    GeoLocation * const g = d->locationNamed("Greenwich");
    QVERIFY(g != nullptr);
    d->setLocation(*g);

    // Verify our location is properly selected
    QCOMPARE(d->geo()->lat()->Degrees(), g->lat()->Degrees());
    QCOMPARE(d->geo()->lng()->Degrees(), g->lng()->Degrees());
}

void TestKStarsStartup::testInitialConditions()
{
    QVERIFY(KStars::Instance() != nullptr);
    QVERIFY(KStars::Instance()->data() != nullptr);
    QVERIFY(KStars::Instance()->data()->clock() != nullptr);

    QCOMPARE(KStars::Instance()->data()->clock()->isActive(), m_InitialConditions.clockRunning);

    QEXPECT_FAIL("", "Initial KStars clock is set from system local time, not geolocation, and is untestable for now.",
                 Continue);
    QCOMPARE(KStars::Instance()->data()->clock()->utc().toString(), m_InitialConditions.dateTime.toString());

    QEXPECT_FAIL("", "Precision of KStars local time conversion to local time does not allow strict millisecond comparison.",
                 Continue);
    QCOMPARE(KStars::Instance()->data()->clock()->utc().toLocalTime(), m_InitialConditions.dateTime);

#if QT_VERSION >= 0x050800
    // However comparison down to nearest second is expected to be OK
    QCOMPARE(llround(KStars::Instance()->data()->clock()->utc().toLocalTime().toMSecsSinceEpoch() / 1000.0),
             m_InitialConditions.dateTime.toSecsSinceEpoch());

    // Test setting time
    KStars::Instance()->data()->clock()->setUTC(KStarsDateTime(m_InitialConditions.dateTime));
    QCOMPARE(llround(KStars::Instance()->data()->clock()->utc().toLocalTime().toMSecsSinceEpoch() / 1000.0),
             m_InitialConditions.dateTime.toSecsSinceEpoch());
#endif
}
