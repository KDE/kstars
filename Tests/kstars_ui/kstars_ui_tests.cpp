/*  KStars UI tests
    Copyright (C) 2017 Csaba Kertesz <csaba.kertesz@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */


#include "kstars_ui_tests.h"

#include "auxiliary/kspaths.h"
#if defined(HAVE_INDI)
#include "ekos/ekosmanager.h"
#include "ekos/profileeditor.h"
#endif
#include "kstars.h"

#include <KActionCollection>
#include <KTipDialog>
#include <KCrash/KCrash>

#include <QFuture>
#include <QtConcurrentRun>
#include <QtTest>

#include <ctime>
#include <unistd.h>

namespace
{
KStars *kstarsInstance = nullptr;
QString testProfileName;
}

void waitForKStars()
{
    while (!kstarsInstance->isGUIReady())
    {
        QThread::msleep(1000);
    }
    while (!kstarsInstance->isActiveWindow())
    {
        QThread::msleep(1000);
    }
    QThread::msleep(2000);
}

KStarsUiTests::KStarsUiTests()
{
    srand((int)time(nullptr));
    // Create writable data dir if it does not exist
    QDir writableDir;

    writableDir.mkdir(KSPaths::writableLocation(QStandardPaths::GenericDataLocation));
    KCrash::initialize();
}

void KStarsUiTests::initTestCase()
{
    KTipDialog::setShowOnStart(false);
    kstarsInstance = KStars::createInstance(true);
}

void KStarsUiTests::cleanupTestCase()
{
    kstarsInstance->close();
    delete kstarsInstance;
    kstarsInstance = nullptr;
}

#if defined(HAVE_INDI)
void openEkosTest()
{
    waitForKStars();
    // Show Ekos Manager by clicking on the toolbar icon
    QAction *action = kstarsInstance->actionCollection()->action("show_ekos");

    QCOMPARE(action != nullptr, true);
    action->trigger();
    QThread::msleep(1000);
    QCOMPARE(kstarsInstance->ekosManager() != nullptr, true);
    QCOMPARE(kstarsInstance->ekosManager()->isVisible(), true);
    QCOMPARE(kstarsInstance->ekosManager()->isActiveWindow(), true);
    // Hide Ekos Manager by clicking on the toolbar icon
    action->trigger();
    QThread::msleep(1000);
    QCOMPARE(!kstarsInstance->ekosManager()->isVisible(), true);
    QCOMPARE(!kstarsInstance->ekosManager()->isActiveWindow(), true);
}

void KStarsUiTests::openEkos()
{
    QFuture<void> testFuture = QtConcurrent::run(openEkosTest);

    while (!testFuture.isFinished())
    {
        QCoreApplication::instance()->processEvents();
        usleep(20*1000);
    }
}

void addEkosProfileTest()
{
    waitForKStars();
    // Show/verify Ekos dialog
    QAction *action = kstarsInstance->actionCollection()->action("show_ekos");

    QCOMPARE(action != nullptr, true);
    action->trigger();
    QThread::msleep(500);
    EkosManager *ekos = kstarsInstance->ekosManager();

    QCOMPARE(ekos != nullptr, true);
    QCOMPARE(ekos->isVisible(), true);
    QCOMPARE(ekos->isActiveWindow(), true);
    // Click on "Add profile" button
    QPushButton* addButton = ekos->findChild<QPushButton*>("addProfileB");

    QCOMPARE(addButton != nullptr, true);
    QTest::mouseClick(addButton, Qt::LeftButton);
    QThread::msleep(1000);
    // Verify the Profile Editor dialog
    ProfileEditor* profileEditor = ekos->findChild<ProfileEditor*>("profileEditorDialog");

    QCOMPARE(profileEditor != nullptr, true);
    // Create a test profile
    QLineEdit* profileNameLE = ekos->findChild<QLineEdit*>("profileIN");
    QComboBox* mountCBox = ekos->findChild<QComboBox*>("mountCombo");
    QComboBox* ccdCBox = ekos->findChild<QComboBox*>("ccdCombo");
    QDialogButtonBox* buttons = profileEditor->findChild<QDialogButtonBox*>("dialogButtons");

    QCOMPARE(profileNameLE != nullptr, true);
    QCOMPARE(mountCBox != nullptr, true);
    QCOMPARE(ccdCBox != nullptr, true);
    QCOMPARE(buttons != nullptr, true);
    testProfileName = QString("testUI%1").arg(rand() % 100000);
    profileNameLE->setText(testProfileName);
    QThread::msleep(500);
    QCOMPARE(profileNameLE->text(), testProfileName);
    mountCBox->setCurrentText("EQMod Mount");
    QThread::msleep(500);
    QCOMPARE(mountCBox->currentIndex() != 0, true);
    ccdCBox->setCurrentText("Canon DSLR");
    QThread::msleep(500);
    QCOMPARE(ccdCBox->currentIndex() != 0, true);
    // Save the profile
    emit buttons->accepted();
    QThread::msleep(2000);
    // Hide Ekos Manager by clicking on the toolbar icon
    action->trigger();
    QThread::msleep(1000);
    QCOMPARE(!kstarsInstance->ekosManager()->isVisible(), true);
    QCOMPARE(!kstarsInstance->ekosManager()->isActiveWindow(), true);
}

void KStarsUiTests::addEkosProfile()
{
    QFuture<void> testFuture = QtConcurrent::run(addEkosProfileTest);

    while (!testFuture.isFinished())
    {
        QCoreApplication::instance()->processEvents();
        usleep(20*1000);
    }
}

void verifyEkosProfileTest()
{
    waitForKStars();
    // Show/verify Ekos dialog
    QAction *action = kstarsInstance->actionCollection()->action("show_ekos");

    QCOMPARE(action != nullptr, true);
    action->trigger();
    QThread::msleep(1000);
    EkosManager *ekos = kstarsInstance->ekosManager();

    QCOMPARE(ekos != nullptr, true);
    QCOMPARE(ekos->isVisible(), true);
    QCOMPARE(ekos->isActiveWindow(), true);
    // Verify that the test profile exists
    QComboBox* profileCBox = ekos->findChild<QComboBox*>("profileCombo");

    QCOMPARE(profileCBox != nullptr, true);
    qDebug() << "Compare:" << profileCBox->currentText() << "<->" << testProfileName;
    QCOMPARE(profileCBox->currentText() == testProfileName, true);
    QThread::msleep(1000);
    // Click on "Edit profile" button
    QPushButton* editButton = ekos->findChild<QPushButton*>("editProfileB");

    QCOMPARE(editButton != nullptr, true);
    QTest::mouseClick(editButton, Qt::LeftButton);
    QThread::msleep(1000);
    // Verify the Profile Editor dialog
    ProfileEditor* profileEditor = ekos->findChild<ProfileEditor*>("profileEditorDialog");

    QCOMPARE(profileEditor != nullptr, true);
    // Verify the selected drivers
    QLineEdit* profileNameLE = ekos->findChild<QLineEdit*>("profileIN");
    QComboBox* mountCBox = ekos->findChild<QComboBox*>("mountCombo");
    QComboBox* ccdCBox = ekos->findChild<QComboBox*>("ccdCombo");

    QCOMPARE(profileNameLE != nullptr, true);
    QCOMPARE(mountCBox != nullptr, true);
    QCOMPARE(ccdCBox != nullptr, true);
    QCOMPARE(profileNameLE->text(), testProfileName);
    QCOMPARE(mountCBox->currentText() == "EQMod Mount", true);
    QCOMPARE(ccdCBox->currentText() == "Canon DSLR", true);
    // Cancel the dialog
    profileEditor->reject();
    QThread::msleep(2000);
    // Hide Ekos Manager by clicking on the toolbar icon
    action->trigger();
    QThread::msleep(1000);
    QCOMPARE(!kstarsInstance->ekosManager()->isVisible(), true);
    QCOMPARE(!kstarsInstance->ekosManager()->isActiveWindow(), true);
}

void KStarsUiTests::verifyEkosProfile()
{
    QFuture<void> testFuture = QtConcurrent::run(verifyEkosProfileTest);

    while (!testFuture.isFinished())
    {
        QCoreApplication::instance()->processEvents();
        usleep(20*1000);
    }
}

void removeEkosProfileTest()
{
    waitForKStars();
    // Show/verify Ekos dialog
    QAction *action = kstarsInstance->actionCollection()->action("show_ekos");

    QCOMPARE(action != nullptr, true);
    action->trigger();
    QThread::msleep(500);
    EkosManager *ekos = kstarsInstance->ekosManager();

    QCOMPARE(ekos != nullptr, true);
    QCOMPARE(ekos->isVisible(), true);
    QCOMPARE(ekos->isActiveWindow(), true);
    // Verify that the test profile exists
    QComboBox* profileCBox = ekos->findChild<QComboBox*>("profileCombo");

    QCOMPARE(profileCBox != nullptr, true);
    QCOMPARE(profileCBox->currentText() == testProfileName, true);
    QThread::msleep(500);
    // Click on "Remove profile" button
    QPushButton* removeButton = ekos->findChild<QPushButton*>("deleteProfileB");

    QCOMPARE(removeButton != nullptr, true);
    QTest::mouseClick(removeButton, Qt::LeftButton);
    QThread::msleep(1000);
    // Verify the confirmation dialog
    // This trick is from https://stackoverflow.com/questions/38596785
    QWidget* dialog = QApplication::activeModalWidget();

    QCOMPARE(dialog != nullptr, true);
    QDialogButtonBox* buttons = ekos->findChild<QDialogButtonBox*>();

    QCOMPARE(buttons != nullptr, true);
    QTest::mouseClick(buttons->button(QDialogButtonBox::Yes), Qt::LeftButton);
    QThread::msleep(500);
    // Verify that the test profile does not exist
    bool found = false;

    for (int i = 0; i < profileCBox->count(); ++i)
    {
        qDebug() << "Compare: " << profileCBox->itemText(i) << "<->" << testProfileName;
        if (profileCBox->itemText(i) == testProfileName)
        {
            found = true;
        }
    }
    QCOMPARE(found, false);
    // Hide Ekos Manager by clicking on the toolbar icon
    action->trigger();
    QThread::msleep(1000);
    QCOMPARE(!kstarsInstance->ekosManager()->isVisible(), true);
    QCOMPARE(!kstarsInstance->ekosManager()->isActiveWindow(), true);
}

void KStarsUiTests::removeEkosProfile()
{
    QFuture<void> testFuture = QtConcurrent::run(removeEkosProfileTest);

    while (!testFuture.isFinished())
    {
        QCoreApplication::instance()->processEvents();
        usleep(20*1000);
    }
}
#endif

QTEST_MAIN(KStarsUiTests);
