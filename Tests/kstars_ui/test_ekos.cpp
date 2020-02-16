/*  KStars UI tests
    Copyright (C) 218, 2020
    Csaba Kertesz <csaba.kertesz@gmail.com>
    Jasem Mutlaq <knro@ikarustech.com>
    Eric Dejouhanet <eric.dejouhanet@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "config-kstars.h"

#if defined(HAVE_INDI)

#include "kstars_ui_tests.h"

#include "ekos/manager.h"
#include "ekos/profileeditor.h"
#include "kstars.h"
#include "auxiliary/ksmessagebox.h"

#include <KActionCollection>
#include <KTipDialog>
#include <KCrash/KCrash>

#include <QFuture>
#include <QtConcurrentRun>
#include <QtTest>
#include <QPoint>
#include <QModelIndex>

#include <ctime>
#include <unistd.h>

#define KTRY_ACTION(action_text) do { \
    QAction * const action = KStars::Instance()->actionCollection()->action(action_text); \
    QVERIFY(action != nullptr); \
    action->trigger(); } while(false)

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

void KStarsUiTests::initEkos()
{
    /* No-op */
}

void KStarsUiTests::cleanupEkos()
{
    KTRY_CLOSE_EKOS();
}

void KStarsUiTests::openEkosTest()
{
    KVERIFY_EKOS_IS_HIDDEN();
    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();
    KTRY_CLOSE_EKOS();
    KVERIFY_EKOS_IS_HIDDEN();
}

void KStarsUiTests::manipulateEkosProfiles()
{
    KVERIFY_EKOS_IS_HIDDEN();
    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();

    // Because we don't want to manager the order of tests, we do the profile manipulation in three steps of the same test
    // We use that poor man's shared variable to hold the result of the first (creation) and second (edition) test step.
    // The ProfileEditor is exec()'d, so test code must be made asynchronous, and QTimer::singleShot is an easy way to do that.
    // We use two timers, one to run the end-user test, which eventually will close the dialog, and a second one to really close if the test step fails.
    bool testIsSuccessful = false;

    Ekos::Manager * const ekos = Ekos::Manager::Instance();

    // --------- First step: creating the profile

    // Because the dialog is modal, the remainder of the test is made asynchronous
    QTimer::singleShot(200, ekos, [&]()
    {
        // Find the Profile Editor dialog
        ProfileEditor* profileEditor = ekos->findChild<ProfileEditor*>("profileEditorDialog");
        /*
        // If the name of the widget is unknown, use this snippet to look it up from its class
        if (profileEditor == nullptr)
            foreach (QWidget *w, QApplication::topLevelWidgets())
                if (w->inherits("ProfileEditor"))
                    profileEditor = qobject_cast <ProfileEditor*> (w);
        */
        QVERIFY(profileEditor != nullptr);

        // Create a test profile
        QLineEdit * const profileNameLE = profileEditor->findChild<QLineEdit*>("profileIN");
        QVERIFY(nullptr != profileNameLE);
        testProfileName = QString("testUI%1").arg(rand() % 100000); // FIXME: Move this to fixtures
        profileNameLE->setText(testProfileName);
        QCOMPARE(profileNameLE->text(), testProfileName);

        // Setting an item programmatically in a treeview combobox...
        QComboBox * const mountCBox = profileEditor->findChild<QComboBox*>("mountCombo");
        QVERIFY(nullptr != mountCBox);
        QString lookup("Telescope Simulator"); // FIXME: Move this to fixtures
        // Match the text recursively in the model, this results in a model index with a parent
        QModelIndexList const list = mountCBox->model()->match(mountCBox->model()->index(0,0),Qt::DisplayRole,QVariant::fromValue(lookup),1,Qt::MatchRecursive);
        QVERIFY(0 < list.count());
        QModelIndex const &item = list.first();
        //QWARN(QString("Found text '%1' at #%2, parent at #%3").arg(item.data().toString()).arg(item.row()).arg(item.parent().row()).toStdString().data());
        QCOMPARE(list.value(0).data().toString(), lookup);
        QVERIFY(!item.parent().parent().isValid());
        // Now set the combobox model root to the match's parent
        mountCBox->setRootModelIndex(item.parent());
        // And set the text as if the end-user had selected it
        mountCBox->setCurrentText(lookup);
        QCOMPARE(mountCBox->currentText(), lookup);

        QComboBox * const ccdCBox = profileEditor->findChild<QComboBox*>("ccdCombo");
        QVERIFY(nullptr != ccdCBox);
        lookup = "CCD Simulator";
        ccdCBox->setCurrentText(lookup); // FIXME: Move this to fixtures
        QCOMPARE(ccdCBox->currentText(), lookup);

        // Save the profile using the "Save" button
        QDialogButtonBox* buttons = profileEditor->findChild<QDialogButtonBox*>("dialogButtons");
        QVERIFY(nullptr != buttons);
        QTest::mouseClick(buttons->button(QDialogButtonBox::Save), Qt::LeftButton);

        testIsSuccessful = true;
    });

    // Cancel the Profile Editor dialog if the test failed - this will happen after pushing the add button below
    QTimer * closeDialog = new QTimer(this);
    closeDialog->setSingleShot(true);
    closeDialog->setInterval(1000);
    ekos->connect(closeDialog, &QTimer::timeout, [&]()
    {
        ProfileEditor* profileEditor = ekos->findChild<ProfileEditor*>("profileEditorDialog");
        if (profileEditor != nullptr)
            profileEditor->reject();
    });

    // Click on "Add profile" button, and let the async tests run on the modal dialog
    QPushButton* addButton = ekos->findChild<QPushButton*>("addProfileB");
    QVERIFY(addButton != nullptr);
    QTest::mouseClick(addButton, Qt::LeftButton);

    // Click handler returned, stop the timer closing the dialog on failure
    closeDialog->stop();
    delete closeDialog;

    // Verification of the first test step
    QVERIFY(testIsSuccessful);
    testIsSuccessful = false;

    // --------- Second step: editing and verifying the profile

    // Verify that the test profile exists, and select it
    QComboBox* profileCBox = ekos->findChild<QComboBox*>("profileCombo");
    QVERIFY(profileCBox != nullptr);
    profileCBox->setCurrentText(testProfileName);
    QCOMPARE(profileCBox->currentText(), testProfileName);

    // Because the dialog is modal, the remainder of the test is made asynchronous
    QTimer::singleShot(200, ekos, [&]()
    {
        // Find the Profile Editor dialog
        ProfileEditor* profileEditor = ekos->findChild<ProfileEditor*>("profileEditorDialog");
        QVERIFY(profileEditor != nullptr);

        // Verify the values set by addEkosProfileTest
        QLineEdit* profileNameLE = ekos->findChild<QLineEdit*>("profileIN");
        QVERIFY(profileNameLE != nullptr);
        QCOMPARE(profileNameLE->text(), profileCBox->currentText());

        QComboBox* mountCBox = ekos->findChild<QComboBox*>("mountCombo");
        QVERIFY(mountCBox != nullptr);
        QCOMPARE(mountCBox->currentText(),"Telescope Simulator");

        QComboBox* ccdCBox = ekos->findChild<QComboBox*>("ccdCombo");
        QVERIFY(ccdCBox != nullptr);
        QCOMPARE(ccdCBox->currentText(), "CCD Simulator");

        // Cancel the dialog using the "Close" button
        QDialogButtonBox* buttons = profileEditor->findChild<QDialogButtonBox*>("dialogButtons");
        QVERIFY(nullptr != buttons);
        QTest::mouseClick(buttons->button(QDialogButtonBox::Close), Qt::LeftButton);

        testIsSuccessful = true;
    });

    // Cancel the Profile Editor dialog if the test failed - this will happen after pushing the edit button below
    closeDialog = new QTimer(this);
    closeDialog->setSingleShot(true);
    closeDialog->setInterval(1000);
    ekos->connect(closeDialog, &QTimer::timeout, [&]()
    {
        ProfileEditor* profileEditor = ekos->findChild<ProfileEditor*>("profileEditorDialog");
        if (profileEditor != nullptr)
            profileEditor->reject();
    });

    // Click on "Edit profile" button, and let the async tests run on the modal dialog
    QPushButton* editButton = ekos->findChild<QPushButton*>("editProfileB");
    QVERIFY(editButton != nullptr);
    QTest::mouseClick(editButton, Qt::LeftButton);

    // Click handler returned, stop the timer closing the dialog on failure
    closeDialog->stop();
    delete closeDialog;

    // Verification of the second test step
    QVERIFY(testIsSuccessful);
    testIsSuccessful = false;

    // Verify that the test profile still exists, and select it
    profileCBox = ekos->findChild<QComboBox*>("profileCombo");
    QVERIFY(profileCBox != nullptr);
    profileCBox->setCurrentText(testProfileName);
    QCOMPARE(profileCBox->currentText(), testProfileName);

    // --------- Third step: deleting the profile

    // The yes/no modal dialog is not really used as a blocking question, but let's keep the remainder of the test is made asynchronous
    QTimer::singleShot(200, ekos, [&]()
    {
        // This trick is from https://stackoverflow.com/questions/38596785
        QTRY_VERIFY_WITH_TIMEOUT(QApplication::activeModalWidget() != nullptr, 1000);
        // This is not a regular dialog but a KStars-customized dialog, so we can't search for a yes button from QDialogButtonBox
        KSMessageBox * const dialog = qobject_cast <KSMessageBox*> (QApplication::activeModalWidget());
        QVERIFY(dialog != nullptr);
        emit dialog->accept();

        testIsSuccessful = true;
    });

    // Click on "Remove profile" button - this will display a modal yes/no dialog
    QPushButton* removeButton = ekos->findChild<QPushButton*>("deleteProfileB");
    QVERIFY(removeButton != nullptr);
    QTest::mouseClick(removeButton, Qt::LeftButton);
    // Pressing delete-profile triggers the open() function of the yes/no modal dialog, which returns immediately, so wait for it to disappear
    QTRY_VERIFY_WITH_TIMEOUT(QApplication::activeModalWidget() == nullptr, 1000);

    // Verification of the third test step
    QVERIFY(testIsSuccessful);
    testIsSuccessful = false;

    // Verify that the test profile doesn't exist anymore
    profileCBox = ekos->findChild<QComboBox*>("profileCombo");
    QVERIFY(profileCBox != nullptr);
    profileCBox->setCurrentText(testProfileName);
    QVERIFY(profileCBox->currentText() != testProfileName);
}

#endif
