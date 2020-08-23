/*  KStars UI tests
    Copyright (C) 2020
    Eric Dejouhanet <eric.dejouhanet@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */


#include "test_ekos_guide.h"

#if defined(HAVE_INDI) && HAVE_INDI

#include "kstars_ui_tests.h"
#include "test_ekos.h"
#include "test_ekos_simulator.h"

#include "ekos/manager.h"
#include "ekos/profileeditor.h"
#include "Options.h"

TestEkosGuide::TestEkosGuide(QObject *parent) : QObject(parent)
{
}

void TestEkosGuide::initTestCase()
{
    KVERIFY_EKOS_IS_HIDDEN();
    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();
}

void TestEkosGuide::cleanupTestCase()
{
    KTRY_CLOSE_EKOS();
    KVERIFY_EKOS_IS_HIDDEN();
}

void TestEkosGuide::init()
{

}

void TestEkosGuide::cleanup()
{

}

void TestEkosGuide::testPHD2Connection()
{
    QString testProfileName("phd2_test_profile");
    QString const guider_host("localhost");
    QString const guider_port("4400");

    // Update simulator profile to use PHD2 as guider
    {
        bool isDone = false;
        Ekos::Manager * const ekos = Ekos::Manager::Instance();
        QTimer::singleShot(1000, ekos, [&]
        {
            ProfileEditor* profileEditor = ekos->findChild<ProfileEditor*>("profileEditorDialog");

            // Change guider to PHD2
            QString const gt("PHD2");
            KTRY_PROFILEEDITOR_GADGET(QComboBox, guideTypeCombo);
            guideTypeCombo->setCurrentText(gt);
            QTRY_COMPARE(guideTypeCombo->currentText(), gt);

            // Create a test profile
            KTRY_PROFILEEDITOR_GADGET(QLineEdit, profileIN);
            profileIN->setText(testProfileName);
            QCOMPARE(profileIN->text(), testProfileName);

            // Write PHD2 server specs
            KTRY_PROFILEEDITOR_GADGET(QLineEdit, externalGuideHost);
            externalGuideHost->setText(guider_host);
            QCOMPARE(externalGuideHost->text(), guider_host);
            KTRY_PROFILEEDITOR_GADGET(QLineEdit, externalGuidePort);
            externalGuidePort->setText(guider_port);
            QCOMPARE(externalGuidePort->text(), guider_port);

            // Setting an item programmatically in a treeview combobox...
            KTRY_PROFILEEDITOR_GADGET(QComboBox, mountCombo);
            QString lookup("Telescope Simulator"); // FIXME: Move this to fixtures
            // Match the text recursively in the model, this results in a model index with a parent
            QModelIndexList const list = mountCombo->model()->match(mountCombo->model()->index(0, 0), Qt::DisplayRole, QVariant::fromValue(lookup), 1, Qt::MatchRecursive);
            QVERIFY(0 < list.count());
            QModelIndex const &item = list.first();
            //QWARN(QString("Found text '%1' at #%2, parent at #%3").arg(item.data().toString()).arg(item.row()).arg(item.parent().row()).toStdString().data());
            QCOMPARE(list.value(0).data().toString(), lookup);
            QVERIFY(!item.parent().parent().isValid());
            // Now set the combobox model root to the match's parent
            mountCombo->setRootModelIndex(item.parent());
            // And set the text as if the end-user had selected it
            mountCombo->setCurrentText(lookup);
            QCOMPARE(mountCombo->currentText(), lookup);

            KTRY_PROFILEEDITOR_GADGET(QComboBox, ccdCombo);
            lookup = "CCD Simulator";
            ccdCombo->setCurrentText(lookup);
            QCOMPARE(ccdCombo->currentText(), lookup);

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

    // Start a parallel PHD2 instance
    QProcess phd2(this);

    // No success using that strange --load option, it loads and exits, but does not save anywhere
#if 0
    QStringList args = { "--load=phd2_Simulators.phd" };
    phd2.start(QString("phd2"), args);
    if (!phd2.waitForStarted(1000))
    {
        QString text = QString("Bypassing PHD2 tests, phd2 executable failing or not found. ") + phd2.errorString();
        QWARN(text.toStdString().c_str());
        return;
    }
    // PHD2 will exit after loading the configuration
    QTRY_VERIFY_WITH_TIMEOUT(phd2.state() == QProcess::NotRunning, 500);
#else
    QString const phd2_config = ".PHDGuidingV2";
    QStandardPaths::enableTestMode(false);
    QFileInfo phd2_config_home(QStandardPaths::writableLocation(QStandardPaths::HomeLocation), phd2_config);
    QStandardPaths::enableTestMode(true);
    QWARN(QString("Writing PHD configuration file to '%1'").arg(phd2_config_home.filePath()).toStdString().c_str());
    if (phd2_config_home.exists())
        QVERIFY(QFile::remove(phd2_config_home.filePath()));
    QVERIFY(QFile::copy(phd2_config, phd2_config_home.filePath()));
#endif

    // Start PHD2 with the proper configuration
    phd2.start(QString("phd2"));
    QVERIFY(phd2.waitForStarted(3000));
    QTest::qWait(2000);
    QTRY_VERIFY_WITH_TIMEOUT(phd2.state() == QProcess::Running, 1000);

    // Try to connect to the PHD2 server
    QTcpSocket phd2_server(this);
    //phd2_server.connectToHost("localhost", guider_port.toUInt());
    phd2_server.connectToHost(guider_host, guider_port.toUInt(), QIODevice::ReadOnly, QAbstractSocket::IPv4Protocol);
    if(!phd2_server.waitForConnected(5000))
    {
        QWARN(QString("Cannot continue, PHD2 server is unavailable (%1)").arg(phd2_server.errorString()).toStdString().c_str());
        return;
    }
    phd2_server.disconnectFromHost();
    if (phd2_server.state() == QTcpSocket::ConnectedState)
        QVERIFY(phd2_server.waitForDisconnected(1000));

    // Start Ekos
    KTRY_EKOS_SELECT_PROFILE(testProfileName);
    KTRY_EKOS_CLICK(processINDIB); \
    QWARN("HACK HACK HACK adding delay here for devices to connect"); \
    QTest::qWait(10000);

    // HACK: Reset clock to initial conditions
    KHACK_RESET_EKOS_TIME();

    // Wait for Focus to come up, switch to Focus tab
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->guideModule() != nullptr, 5000);
    KTRY_EKOS_GADGET(QTabWidget, toolsWidget);
    toolsWidget->setCurrentWidget(Ekos::Manager::Instance()->guideModule());
    QTRY_COMPARE_WITH_TIMEOUT(toolsWidget->currentWidget(), Ekos::Manager::Instance()->guideModule(), 1000);

    // Verify the phd2 server was connected successfully - by default, PHD2 has a 16-second timeout to camera connection
    KTRY_GUIDE_GADGET(QPushButton, externalConnectB);
    QTRY_VERIFY_WITH_TIMEOUT(!externalConnectB->isEnabled(), 30000);
    KTRY_GUIDE_GADGET(QPushButton, externalDisconnectB);
    QTRY_VERIFY_WITH_TIMEOUT(externalDisconnectB->isEnabled(), 10000);

    KTRY_GUIDE_GADGET(QPushButton, guideB);
    KTRY_GUIDE_GADGET(QPushButton, stopB);
    KTRY_GUIDE_GADGET(QPushButton, captureB);
    KTRY_GUIDE_GADGET(QPushButton, loopB);
    KTRY_GUIDE_GADGET(QPushButton, clearCalibrationB);
    KTRY_GUIDE_GADGET(QWidget, idlingStateLed);
    KTRY_GUIDE_GADGET(QWidget, preparingStateLed);
    KTRY_GUIDE_GADGET(QWidget, runningStateLed);

    // Run a few connect/disconnect cycles
    for (int count = 0; count < 10; count++)
    {
        QVERIFY(guideB->isEnabled());
        QVERIFY(captureB->isEnabled());
        QVERIFY(loopB->isEnabled());
        QVERIFY(!stopB->isEnabled());
        KTRY_GUIDE_CLICK(externalDisconnectB);
        QTRY_VERIFY_WITH_TIMEOUT(externalConnectB->isEnabled(), 20000);
        QVERIFY(!guideB->isEnabled());
        QVERIFY(!captureB->isEnabled());
        QVERIFY(!loopB->isEnabled());
        QVERIFY(!stopB->isEnabled());
        KTRY_GUIDE_CLICK(externalConnectB);
        QTRY_VERIFY_WITH_TIMEOUT(externalDisconnectB->isEnabled(), 10000);
    }

    // When connected, capture, loop and guide are enabled
    // Run a few guide/stop cycles
    QWARN("Because Guide is changing its state without waiting for PHD2 to reply, we insert a forced delay after each click");
    for (int count = 0; count < 10; count++)
    {
        KTRY_GUIDE_CLICK(loopB);
        QTest::qWait(1000);
        QTRY_VERIFY_WITH_TIMEOUT(stopB->isEnabled(), 10000);
        QEXPECT_FAIL("", "Capture button remains active while looping.", Continue);
        QVERIFY(!captureB->isEnabled());
        QEXPECT_FAIL("", "Loop button remains active while looping.", Continue);
        QVERIFY(!loopB->isEnabled());
        QTRY_VERIFY_WITH_TIMEOUT((dynamic_cast <KLed*> (preparingStateLed))->state() == KLed::On, 5000);
        QVERIFY((dynamic_cast <KLed*> (idlingStateLed))->state() == KLed::Off);
        QVERIFY((dynamic_cast <KLed*> (runningStateLed))->state() == KLed::Off);
        KTRY_GUIDE_CLICK(stopB);
        QTest::qWait(1000);
        QTRY_VERIFY_WITH_TIMEOUT(loopB->isEnabled(), 10000);
        QEXPECT_FAIL("", "Stop button remains active after stopping.", Continue);
        QVERIFY(!stopB->isEnabled());
        QVERIFY(captureB->isEnabled());
        QTRY_VERIFY_WITH_TIMEOUT((dynamic_cast <KLed*> (preparingStateLed))->state() == KLed::Off, 5000);
        QVERIFY((dynamic_cast <KLed*> (preparingStateLed))->state() == KLed::Off);
        QVERIFY((dynamic_cast <KLed*> (runningStateLed))->state() == KLed::Off);
    }

    // Run a calibration with the telescope pointing at NCP (default position) - Calibration is bound to fail bcause of RA
    // Two options: PHD2 either fails to see movement, or just switches to looping without doing anything
    QTRY_VERIFY_WITH_TIMEOUT(guideB->isEnabled(), 500);
    KTRY_GUIDE_CLICK(guideB);

    // Wait for calibration to start
    QTRY_VERIFY_WITH_TIMEOUT((dynamic_cast <KLed*> (preparingStateLed))->state() == KLed::On, 5000);

    // We need to wait a bit more than 62 times the default exposure, which is 1 second

    uint const calibration_timeout = Options::guideCalibrationTimeout();
    Options::setGuideCalibrationTimeout(62);

    uint const loststar_timeout = Options::guideLostStarTimeout();
    Options::setGuideLostStarTimeout(30);

    // Wait for the calibration to fail
    QTRY_VERIFY_WITH_TIMEOUT((dynamic_cast <KLed*> (runningStateLed))->color() == Qt::red, Options::guideCalibrationTimeout() * 1200);

    // Run a calibration with the telescope pointing at Meridian - RA 3h DEC 0 at the current date is SW
    // And don't forget to track!
    QVERIFY(Ekos::Manager::Instance()->mountModule()->sync(3,0));
    Ekos::Manager::Instance()->mountModule()->setTrackEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->mountModule()->status() == ISD::Telescope::MOUNT_TRACKING, 2000);
    QTRY_VERIFY_WITH_TIMEOUT(guideB->isEnabled(), 500);
    KTRY_GUIDE_CLICK(guideB);

    // We need to wait a bit more than 22 times the default exposure, which is 1 second
    QTRY_VERIFY_WITH_TIMEOUT((dynamic_cast <KLed*> (runningStateLed))->color() == Qt::green, Options::guideCalibrationTimeout() * 1200);

    // We can stop guiding now that calibration is done
    KTRY_GUIDE_CLICK(stopB);
    QWARN("Guide aborts without waiting for PHD2 to report abort, so wait after stopping before restarting.");
    QTest::qWait(500);
    QTRY_VERIFY_WITH_TIMEOUT((dynamic_cast <KLed*> (runningStateLed))->color() == Qt::red, 10000);

    // We can restart, and there will be no calibration to wait for
    KTRY_GUIDE_CLICK(guideB);
    QTRY_VERIFY_WITH_TIMEOUT((dynamic_cast <KLed*> (runningStateLed))->color() == Qt::green, 10000);

    // Sync the telescope just enough for the star mass to drop, PHD2 will notify star lost
    QVERIFY(Ekos::Manager::Instance()->mountModule()->sync(3.01,0));
    Ekos::Manager::Instance()->mountModule()->setTrackEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->mountModule()->status() == ISD::Telescope::MOUNT_TRACKING, 5000);
    QTRY_VERIFY_WITH_TIMEOUT((dynamic_cast <KLed*> (runningStateLed))->color() == Qt::yellow, 10000);

    // Sync the telescope back for the star mass to return, PHD2 will notify star selected
    QVERIFY(Ekos::Manager::Instance()->mountModule()->sync(3,0));
    Ekos::Manager::Instance()->mountModule()->setTrackEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->mountModule()->status() == ISD::Telescope::MOUNT_TRACKING, 5000);
    QTRY_VERIFY_WITH_TIMEOUT((dynamic_cast <KLed*> (runningStateLed))->color() == Qt::green, 10000);

    // Sync the telescope just enough for the star mass to drop, PHD2 will notify star lost again
    QVERIFY(Ekos::Manager::Instance()->mountModule()->sync(3.01,0));
    Ekos::Manager::Instance()->mountModule()->setTrackEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->mountModule()->status() == ISD::Telescope::MOUNT_TRACKING, 5000);
    QTRY_VERIFY_WITH_TIMEOUT((dynamic_cast <KLed*> (runningStateLed))->color() == Qt::yellow, 10000);

    // Wait for guiding to abort
    QTRY_VERIFY_WITH_TIMEOUT((dynamic_cast <KLed*> (runningStateLed))->color() == Qt::red, Options::guideLostStarTimeout() * 1200);
    QVERIFY((dynamic_cast <KLed*> (preparingStateLed))->color() == Qt::red);
    QVERIFY((dynamic_cast <KLed*> (idlingStateLed))->color() == Qt::green);

    // We can restart, and wait for calibration to end
    // However that test is not stable - sometimes PHD2 will refuse to continue, sometimes will catch something
    QWARN("Restarting to guide when there is no star locked may or may not look for a lock position, bypassing test.");
    //KTRY_GUIDE_CLICK(guideB);
    //QTRY_VERIFY_WITH_TIMEOUT((dynamic_cast <KLed*> (runningStateLed))->color() == Qt::green, Options::guideCalibrationTimeout() * 1200);

    // Instead, clear calibration and restart
    QTRY_VERIFY_WITH_TIMEOUT(clearCalibrationB->isEnabled(), 10000);
    KTRY_GUIDE_CLICK(clearCalibrationB);
    QWARN("No feedback available on PHD2 calibration removal, so wait a bit.");
    QTest::qWait(1000);
    KTRY_GUIDE_CLICK(guideB);
    QTRY_VERIFY_WITH_TIMEOUT((dynamic_cast <KLed*> (runningStateLed))->color() == Qt::green, Options::guideCalibrationTimeout() * 1200);

    // It is apparently not possible to check the option "Stop guiding when mount moves" through the server connection
    // TODO: update options to enable this
#if 0
    // Slew the telescope somewhere else, PHD2 will notify guiding abort
    QVERIFY(Ekos::Manager::Instance()->mountModule()->slew(3.1,0));
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->mountModule()->status() == ISD::Telescope::MOUNT_TRACKING, 10000);
    QTRY_VERIFY_WITH_TIMEOUT((dynamic_cast <KLed*> (runningStateLed))->color() == Qt::red, 10000);

    // We can restart, and there will be no calibration to wait for
    KTRY_GUIDE_CLICK(guideB);
    QTRY_VERIFY_WITH_TIMEOUT((dynamic_cast <KLed*> (runningStateLed))->color() == Qt::green, 5000);
#endif

    // Run device disconnection tests
    // TODO: Ekos::Manager::Instance()->mountModule()->currentTelescope->Disconnect();
    // TODO: Ekos::Manager::Instance()->captureModule()->currentCCD->Disconnect();

    // TODO: Manipulate PHD2 directly to dis/connect and loop

    // Stop now - if the previous test successfully started to guide
    if (stopB->isEnabled())
    {
        KTRY_GUIDE_CLICK(stopB);
        QWARN("Guide aborts without waiting for PHD2 to report abort, so wait after stopping before restarting.");
        QTest::qWait(500);
        QTRY_VERIFY_WITH_TIMEOUT((dynamic_cast <KLed*> (runningStateLed))->state() == KLed::Off, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(guideB->isEnabled(), 2000);
    }

    // Revert timeout options
    Options::setGuideCalibrationTimeout(calibration_timeout);
    Options::setGuideLostStarTimeout(loststar_timeout);

    // Disconnect
    KTRY_GUIDE_CLICK(externalDisconnectB);
    QTRY_VERIFY_WITH_TIMEOUT(externalConnectB->isEnabled(), 10000);

    // Stop Simulators
    KTRY_EKOS_STOP_SIMULATORS();

    // Stop PHD2
    phd2.terminate();
    QVERIFY(phd2.waitForFinished(5000));
}

QTEST_KSTARS_MAIN(TestEkosGuide)

#endif
