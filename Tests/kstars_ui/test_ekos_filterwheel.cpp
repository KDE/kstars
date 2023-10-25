/*  KStars UI tests
    Copyright (C) 2020
    Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "test_ekos_filterwheel.h"

#if defined(HAVE_INDI)

#include "kstars_ui_tests.h"
#include "test_ekos.h"
#include "test_ekos_simulator.h"
#include "ekos/profileeditor.h"
#include "fitsviewer/fitsdata.h"
#include "ekos/capture/capture.h"

TestEkosFilterWheel::TestEkosFilterWheel(QObject *parent) : QObject(parent)
{
}

void TestEkosFilterWheel::initTestCase()
{
    KVERIFY_EKOS_IS_HIDDEN();
    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();

    // Update simulator profile to use PHD2 as guider
    {
        bool isDone = false;
        Ekos::Manager * const ekos = Ekos::Manager::Instance();
        QTimer::singleShot(1000, ekos, [&]
        {
            ProfileEditor* profileEditor = ekos->findChild<ProfileEditor*>("profileEditorDialog");

            // Create a test profile
            KTRY_PROFILEEDITOR_GADGET(QLineEdit, profileIN);
            profileIN->setText(testProfileName);
            QCOMPARE(profileIN->text(), testProfileName);

            // Disable Port Selector
            KTRY_PROFILEEDITOR_GADGET(QCheckBox, portSelectorCheck);
            portSelectorCheck->setChecked(false);
            QCOMPARE(portSelectorCheck->isChecked(), false);

            // Setting an item programmatically in a treeview combobox...
            KTRY_PROFILEEDITOR_TREE_COMBOBOX(mountCombo, "Telescope Simulator");
            KTRY_PROFILEEDITOR_TREE_COMBOBOX(ccdCombo, "Guide Simulator");
            KTRY_PROFILEEDITOR_TREE_COMBOBOX(filterCombo, "Filter Simulator");

            // Save the profile using the "Save" button
            QDialogButtonBox* buttons = profileEditor->findChild<QDialogButtonBox*>("dialogButtons");
            QVERIFY(nullptr != buttons);
            QTest::mouseClick(buttons->button(QDialogButtonBox::Save), Qt::LeftButton);

            isDone = true;
        });

        // Cancel the profile editor if test fails
        QTimer * closeDialog = new QTimer(this);
        closeDialog->setSingleShot(true);
        closeDialog->setInterval(5000);
        ekos->connect(closeDialog, &QTimer::timeout, [&]
        {
            ProfileEditor* profileEditor = ekos->findChild<ProfileEditor*>("profileEditorDialog");
            if (profileEditor != nullptr)
                profileEditor->reject();
        });

        // Click on "New profile" button, and let the async tests run on the modal dialog
        KTRY_EKOS_GADGET(QPushButton, addProfileB);
        QTRY_VERIFY_WITH_TIMEOUT(addProfileB->isEnabled(), 1000);
        QTest::mouseClick(addProfileB, Qt::LeftButton);

        // Click handler returned, stop the timer closing the dialog on failure
        closeDialog->stop();
        delete closeDialog;

        // Verification of the first test step
        QVERIFY(isDone);
    }

    // HACK: Reset clock to initial conditions
    //KHACK_RESET_EKOS_TIME();
}

void TestEkosFilterWheel::cleanupTestCase()
{
    KTRY_EKOS_STOP_SIMULATORS();
    KTRY_CLOSE_EKOS();
    KVERIFY_EKOS_IS_HIDDEN();
}

void TestEkosFilterWheel::init()
{
    // Start Ekos
    KTRY_EKOS_START_PROFILE(testProfileName);
    // Get manager instance
    Ekos::Manager * const ekos = Ekos::Manager::Instance();

    // Wait for Capture to come up, switch to capture tab
    QTRY_VERIFY_WITH_TIMEOUT(ekos->captureModule() != nullptr, 5000);
    KTRY_EKOS_GADGET(QTabWidget, toolsWidget);
    toolsWidget->setCurrentWidget(ekos->captureModule());
    QTRY_COMPARE_WITH_TIMEOUT(toolsWidget->currentWidget(), ekos->captureModule(), 1000);
}

void TestEkosFilterWheel::cleanup()
{
    Ekos::Manager::Instance()->captureModule()->clearSequenceQueue();
    KTRY_CAPTURE_GADGET(QTableWidget, queueTable);
    QTRY_VERIFY_WITH_TIMEOUT(queueTable->rowCount() == 0, 2000);
}

QFileInfoList TestEkosFilterWheel::searchFITS(QDir const &dir) const
{
    QFileInfoList list = dir.entryInfoList(QDir::Files);
    for (const auto &d : dir.entryList(QDir::NoDotAndDotDot | QDir::Dirs))
        list.append(searchFITS(QDir(dir.path() + QDir::separator() + d)));
    return list;
}

void TestEkosFilterWheel::testFilterWheelSync()
{
    QTemporaryDir destination(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/test-XXXXXX");
    QVERIFY(destination.isValid());
    QVERIFY(destination.autoRemove());
    KTRY_CAPTURE_ADD_LIGHT(0.5, 1, 0, "Green", destination.path());

    // Start capturing and wait for procedure to end (visual icon changing)
    KTRY_CAPTURE_GADGET(QPushButton, startB);
    QCOMPARE(startB->icon().name(), QString("media-playback-start"));
    KTRY_CAPTURE_CLICK(startB);
    QTRY_COMPARE_WITH_TIMEOUT(startB->icon().name(), QString("media-playback-start"), 30000);

    QFileInfoList list = searchFITS(QDir(destination.path()));
    QVERIFY(!list.isEmpty());

    QString filepath = list.first().absoluteFilePath();
    QScopedPointer<FITSData> data(new FITSData());
    data->loadFromFile(filepath).waitForFinished();

    QVariant filter;
    QVERIFY(data->getRecordValue("FILTER", filter));
    QVERIFY(filter == "Green");
}

QTEST_KSTARS_MAIN(TestEkosFilterWheel)

#endif // HAVE_INDI
