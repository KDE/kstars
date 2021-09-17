/*  KStars UI tests
    SPDX-FileCopyrightText: 2018, 2020 Csaba Kertesz <csaba.kertesz@gmail.com>
    SPDX-FileCopyrightText: Jasem Mutlaq <knro@ikarustech.com>
    SPDX-FileCopyrightText: Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_ekos.h"

#if defined(HAVE_INDI)

#include "kstars_ui_tests.h"
#include "test_kstars_startup.h"

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

TestEkos::TestEkos(QObject *parent): QObject(parent)
{

}

void TestEkos::initTestCase()
{
}

void TestEkos::cleanupTestCase()
{
}

void TestEkos::init()
{
    KTEST_BEGIN();
    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();
}

void TestEkos::cleanup()
{
    foreach (QDialog * d, KStars::Instance()->findChildren<QDialog*>())
        if (d->isVisible())
            d->hide();

    KTRY_CLOSE_EKOS();
    KVERIFY_EKOS_IS_HIDDEN();
    KTEST_END();
}

void TestEkos::testOpenClose()
{
    /* No-op, we just use init+cleanup */
}

void TestEkos::testSimulatorProfile()
{
    Ekos::Manager * const ekos = Ekos::Manager::Instance();

    // --------- First step: selecting the Simulators profile

    // Verify that the test profile exists, and select it
    QString const p("Simulators");
    QComboBox* profileCBox = Ekos::Manager::Instance()->findChild<QComboBox*>("profileCombo");
    QVERIFY(profileCBox != nullptr);
    profileCBox->setCurrentText(p);
    QTRY_COMPARE(profileCBox->currentText(), p);

    // --------- Second step: starting Ekos with the Simulators profile

    QString const buttonReadyToStart("media-playback-start");
    QString const buttonReadyToStop("media-playback-stop");

    // Check the start button icon as visual feedback about Ekos state, and click to start Ekos
    QPushButton* startEkos = ekos->findChild<QPushButton*>("processINDIB");
    QVERIFY(startEkos != nullptr);
    QVERIFY(!buttonReadyToStart.compare(startEkos->icon().name()));
    QTest::mouseClick(startEkos, Qt::LeftButton);

    // --------- Third step: waiting for Ekos to finish startup

    // The INDI property pages automatically raised on top, but as we got a handle to the button, we continue to test as is

    // Wait until Ekos gives feedback on the INDI client startup - button changes to symbol "stop"
    QTRY_VERIFY_WITH_TIMEOUT(!buttonReadyToStop.compare(startEkos->icon().name()), 7000);

    // It might be that our Simulators profile is not auto-connecting and we should do that manually
    // We assume that by default it's not the case

    // We have no feedback on whether all our devices are connected, so we need to hack a delay in...
    QWARN("HACK HACK HACK adding delay here for devices to connect");
    QTest::qWait(5000);

    // Verify the device connection button is unavailable
    QPushButton * connectDevices = ekos->findChild<QPushButton*>("connectB");
    QVERIFY(connectDevices != nullptr);
    QVERIFY(!connectDevices->isEnabled());

#if QT_VERSION >= 0x050800
    QEXPECT_FAIL("", "Ekos resets the simulation clock when starting a profile.", Continue);
    QCOMPARE(llround(KStars::Instance()->data()->clock()->utc().toLocalTime().toMSecsSinceEpoch()/1000.0), TestKStarsStartup::m_InitialConditions.dateTime.toSecsSinceEpoch());
#endif

    QEXPECT_FAIL("", "Ekos resumes the simulation clock when starting a profile.", Continue);
    QVERIFY(!KStars::Instance()->data()->clock()->isActive());

    // --------- Fourth step: waiting for Ekos to finish stopping

    // Start button that became a stop button is now disabled - we need to disconnect devices first
    QVERIFY(!startEkos->isEnabled());

    // Verify the device disconnection button is available, disconnect devices
    QPushButton * const b = ekos->findChild<QPushButton*>("disconnectB");
    QVERIFY(b != nullptr);
    QVERIFY(b->isEnabled());
    QTimer::singleShot(200, Ekos::Manager::Instance(), [&]
    {
        QTest::mouseClick(b, Qt::LeftButton);
    });

    // --------- Fifth step: waiting for Ekos to finish stopping

    // Start button that became a stop button has to be available
    QTRY_VERIFY_WITH_TIMEOUT(startEkos->isEnabled(), 10000);

    // Hang INDI client up
    QTimer::singleShot(200, ekos, [&]
    {
        QTest::mouseClick(startEkos, Qt::LeftButton);
    });
    QTRY_VERIFY_WITH_TIMEOUT(!buttonReadyToStart.compare(startEkos->icon().name()), 10000);
}

void TestEkos::testManipulateProfiles()
{
    // Because we don't want to manage the order of tests, we do the profile manipulation in three steps of the same test
    // We use that poor man's shared variable to hold the result of the first (creation) and second (edition) test step.
    // The ProfileEditor is exec()'d, so test code must be made asynchronous, and QTimer::singleShot is an easy way to do that.
    // We use two timers, one to run the end-user test, which eventually will close the dialog, and a second one to really close if the test step fails.
    bool testIsSuccessful = false;

    Ekos::Manager * const ekos = Ekos::Manager::Instance();
    QString testProfileName = QString("testUI%1").arg(rand() % 100000); // FIXME: Move this to fixtures

    // --------- First step: creating the profile

    // Because the dialog is modal, the remainder of the test is made asynchronous
    QTimer::singleShot(1000, ekos, [&]
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
        profileNameLE->setText(testProfileName);
        QCOMPARE(profileNameLE->text(), testProfileName);

        // Setting an item programmatically in a treeview combobox...
        QComboBox * const mountCBox = profileEditor->findChild<QComboBox*>("mountCombo");
        QVERIFY(nullptr != mountCBox);
        QString lookup("Telescope Simulator"); // FIXME: Move this to fixtures
        // Match the text recursively in the model, this results in a model index with a parent
        QModelIndexList const list = mountCBox->model()->match(mountCBox->model()->index(0, 0), Qt::DisplayRole, QVariant::fromValue(lookup), 1, Qt::MatchRecursive);
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

        // Same, with a macro helper
        KTRY_PROFILEEDITOR_TREE_COMBOBOX(ccdCombo, "CCD Simulator");

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
    ekos->connect(closeDialog, &QTimer::timeout, [&]
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
    QTimer::singleShot(200, ekos, [&]
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
        QCOMPARE(mountCBox->currentText(), QString("Telescope Simulator"));

        QComboBox* ccdCBox = ekos->findChild<QComboBox*>("ccdCombo");
        QVERIFY(ccdCBox != nullptr);
        QCOMPARE(ccdCBox->currentText(), QString("CCD Simulator"));

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
    ekos->connect(closeDialog, &QTimer::timeout, [&]
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
    QTimer::singleShot(200, ekos, [&]
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

QTEST_KSTARS_MAIN(TestEkos)

#endif // HAVE_INDI
