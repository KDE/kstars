/*  KStars UI tests
    Copyright (C) 2017 Csaba Kertesz <csaba.kertesz@gmail.com>
    Copyright (C) 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "kstars_ui_tests.h"

#if defined(HAVE_INDI)

#include "Options.h"
#include "test_ekos_wizard.h"
#include "test_ekos.h"
#include "ekos/manager.h"
#include "kswizard.h"
#include "auxiliary/kspaths.h"
#include "ekos/profilewizard.h"

#include <KActionCollection>

TestEkosWizard::TestEkosWizard(QObject *parent) : QObject(parent)
{
}

void TestEkosWizard::init()
{
}

void TestEkosWizard::cleanup()
{
    foreach (QDialog * d, KStars::Instance()->findChildren<QDialog*>())
        if (d->isVisible())
            d->hide();
}

void TestEkosWizard::testProfileWizard()
{
    // Update our INDI installation specs
    Options::setIndiDriversAreInternal(true);

    // Locate INDI server - this is highly suspicious, but will cover most of the installation cases I suppose
    if (QFile("/usr/local/bin/indiserver").exists())
        Options::setIndiServer("/usr/local/bin/indiserver");
    else if (QFile("/usr/bin/indiserver").exists())
        Options::setIndiServer("/usr/bin/indiserver");
    QVERIFY(QDir(Options::indiDriversDir()).exists());

    // Locate INDI drivers - the XML list of drivers is the generic data path
    QFile drivers(KSPaths::locate(QStandardPaths::GenericDataLocation, "indidrivers.xml"));
    if (drivers.exists())
        Options::setIndiDriversDir(QFileInfo(drivers).dir().path());
    QVERIFY(QDir(Options::indiDriversDir()).exists());

    // The Ekos new profile wizard opens when starting Ekos for the first time
    bool wizardDone = false;
    std::function <void()> closeWizard = [&]
    {
        KStars * const k = KStars::Instance();
        QVERIFY(k != nullptr);

        // Wait for the KStars Wizard to appear
        if(k->findChild <ProfileWizard*>() == nullptr)
        {
            QTimer::singleShot(500, KStars::Instance(), closeWizard);
            return;
        }
        ProfileWizard * const w = k->findChild <ProfileWizard*>();
        QVERIFY(w != nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(w->isVisible(), 1000);

        // Just dismiss the wizard for now
        QDialogButtonBox* buttons = w->findChild<QDialogButtonBox*>();
        QVERIFY(nullptr != buttons);
        QTest::mouseClick(buttons->button(QDialogButtonBox::Close), Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(!w->isVisible(), 1000);
        wizardDone = true;
    };
    QTimer::singleShot(500, Ekos::Manager::Instance(), closeWizard);

    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();
    QTRY_VERIFY_WITH_TIMEOUT(wizardDone, 1000);

    KTRY_CLOSE_EKOS();
    KVERIFY_EKOS_IS_HIDDEN();
}

#endif // HAVE_INDI
