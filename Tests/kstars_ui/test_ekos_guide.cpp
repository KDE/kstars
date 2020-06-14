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

    // Verify the phd2 server was connected successfully
    KTRY_GUIDE_GADGET(QPushButton, externalConnectB);
    QTRY_VERIFY_WITH_TIMEOUT(!externalConnectB->isEnabled(), 10000);
    KTRY_GUIDE_GADGET(QPushButton, externalDisconnectB);
    QTRY_VERIFY_WITH_TIMEOUT(externalDisconnectB->isEnabled(), 10000);

    KTRY_GUIDE_GADGET(QPushButton, guideB);
    KTRY_GUIDE_GADGET(QPushButton, stopB);
    KTRY_GUIDE_GADGET(QPushButton, captureB);
    KTRY_GUIDE_GADGET(QPushButton, loopB);

    // Run a few connect/disconnect cycles
    for (int count = 0; count < 10; count++)
    {
        QVERIFY(guideB->isEnabled());
        QVERIFY(captureB->isEnabled());
        QVERIFY(loopB->isEnabled());
        QVERIFY(!stopB->isEnabled());
        KTRY_GUIDE_CLICK(externalDisconnectB);
        QTRY_VERIFY_WITH_TIMEOUT(externalConnectB->isEnabled(), 10000);
        QVERIFY(!guideB->isEnabled());
        QVERIFY(!captureB->isEnabled());
        QVERIFY(!loopB->isEnabled());
        QVERIFY(!stopB->isEnabled());
        KTRY_GUIDE_CLICK(externalConnectB);
        QTRY_VERIFY_WITH_TIMEOUT(externalDisconnectB->isEnabled(), 10000);
    }

    // When connected, capture, loop and guide are enabled
    // Run a few guide/stop cycles
    for (int count = 0; count < 10; count++)
    {
        KTRY_GUIDE_CLICK(guideB);
        QTest::qWait(500);
        QEXPECT_FAIL("", "BUG Guide button is not disabled after being clicked", Continue);
        QVERIFY(!guideB->isEnabled());
        QTRY_VERIFY_WITH_TIMEOUT(stopB->isEnabled(), 10000);
        QVERIFY(!guideB->isEnabled());
        QVERIFY(!captureB->isEnabled());
        QVERIFY(!loopB->isEnabled());
        QWARN("HACK HACK HACK Guide/Stop test waiting for synchronisation to happen before stopping...");
        QTest::qWait(500);
        KTRY_GUIDE_CLICK(stopB);
        QTRY_VERIFY_WITH_TIMEOUT(guideB->isEnabled(), 10000);
        QVERIFY(!stopB->isEnabled());
        QVERIFY(captureB->isEnabled());
        QVERIFY(loopB->isEnabled());
        QWARN("HACK HACK HACK Guide/Stop test waiting for synchronisation to happen after stopping...");
        QTest::qWait(2000);
    }

    // Run a calibration with the telescope pointing at NCP (default position) - Calibration is bound to fail bcause of RA
    // Two options: PHD2 either fails to see movement, or just switches to looping without doing anything
    QTRY_VERIFY_WITH_TIMEOUT(guideB->isEnabled(), 500);
    KTRY_GUIDE_CLICK(guideB);

    // We need to wait a bit more than 62 times the default exposure, which is 1 second
    QWARN("HACK HACK HACK Ekos does not feedback when calibration is done, waiting...");
    QTest::qWait(62*1000*1.2);

    // We have now two solutions: either PHD2 decided it should loop for an unknown reason very quickly, or PHD2 failed calibration after 62 attempts

    // In the first case, Ekos is stuck waiting for PHD2
    if (stopB->isEnabled())
    {
        QEXPECT_FAIL("", "BUG Guide calibration at NCP fails, but PHD2 loops and Ekos simply hangs without feedback.", Continue);
        QVERIFY(guideB->isEnabled());

        // Stop guiding, and attempt calibration again
        KTRY_GUIDE_CLICK(stopB);
        QTest::qWait(2000);
        QVERIFY(guideB->isEnabled());
        KTRY_GUIDE_CLICK(guideB);

        // Again, we need to wait a bit more than 62 times the default exposure, which is 1 second
        QWARN("HACK HACK HACK Ekos does not feedback when calibration is done, waiting...");
        QTest::qWait(62*1000*1.2);
    }

    // We may be in the same situation again after recalibration
    if (stopB->isEnabled())
    {
        // In that situation we can't even restore functionality by doing a disconnect/reconnect
        KTRY_GUIDE_CLICK(externalDisconnectB);
        QTRY_VERIFY_WITH_TIMEOUT(externalConnectB->isEnabled(), 10000);
        KTRY_GUIDE_CLICK(externalConnectB);
        QTRY_VERIFY_WITH_TIMEOUT(externalDisconnectB->isEnabled(), 10000);

        // Now the stop button is not enabled neither, and PHD2 loops
    }

    // In the second case, Ekos detected the abort and returned to the initial state, but PHD2 still loops
    // Same situation if we disconnected and reconnected
    if (!stopB->isEnabled())
    {
        // Now we're stuck because PHD2 is looping, and Ekos prevents stopping and will refuse to guide
        KTRY_GUIDE_CLICK(guideB);
        QTest::qWait(2000);
        QEXPECT_FAIL("", "BUG Ekos re-enabled the guide button, but it is not functional anymore.", Continue);
        QVERIFY(stopB->isEnabled());
        // Instead:
        QVERIFY(guideB->isEnabled());

        // Now we can't even restore functionality by doing a disconnect/reconnect
        KTRY_GUIDE_CLICK(externalDisconnectB);
        QTRY_VERIFY_WITH_TIMEOUT(externalConnectB->isEnabled(), 10000);
        KTRY_GUIDE_CLICK(externalConnectB);
        QTRY_VERIFY_WITH_TIMEOUT(externalDisconnectB->isEnabled(), 10000);

        // Try to guide, will be rejected
        QTRY_VERIFY_WITH_TIMEOUT(guideB->isEnabled(), 5000);
        KTRY_GUIDE_CLICK(guideB);
        QTest::qWait(2000);
        QEXPECT_FAIL("", "BUG After Ekos reconnects, the guide button is still not functional.", Continue);
        QVERIFY(stopB->isEnabled());
        // Instead:
        QVERIFY(guideB->isEnabled());

        // Send an abort directly through the guider interface to restore functionality
        Ekos::Manager::Instance()->guideModule()->getGuider()->abort();
    }

    // Run a calibration with the telescope pointing at Meridian - RA 3h DEC 0 at the current date is SW
    // And don't forget to track!
    QVERIFY(Ekos::Manager::Instance()->mountModule()->sync(3,0));
    Ekos::Manager::Instance()->mountModule()->setTrackEnabled(true);
    QTest::qWait(500);
    QTRY_VERIFY_WITH_TIMEOUT(guideB->isEnabled(), 500);
    KTRY_GUIDE_CLICK(guideB);

    // We need to wait a bit more than 22 times the default exposure, which is 1 second
    QWARN("HACK HACK HACK Ekos does not feedback when calibration is done, waiting...");
    QTest::qWait(4000+22*1000*1.5+4000);
    QTRY_VERIFY_WITH_TIMEOUT(stopB->isEnabled(), 2000);

    // We can stop guiding now that calibration is done
    KTRY_GUIDE_CLICK(stopB);
    QWARN("HACK HACK HACK Guide/Stop test waiting for synchronisation to happen after stopping...");
    QTest::qWait(2000);
    QTRY_VERIFY_WITH_TIMEOUT(guideB->isEnabled(), 2000);

    // We can restart, and there will be no calibration to wait for
    KTRY_GUIDE_CLICK(guideB);
    QTRY_VERIFY_WITH_TIMEOUT(stopB->isEnabled(), 10000);

    // Run device disconnection tests
    // TODO: Ekos::Manager::Instance()->mountModule()->currentTelescope->Disconnect();
    // TODO: Ekos::Manager::Instance()->captureModule()->currentCCD->Disconnect();

    // Stop now
    KTRY_GUIDE_CLICK(stopB);
    QWARN("HACK HACK HACK Guide/Stop test waiting for synchronisation to happen after stopping...");
    QTest::qWait(2000);
    QTRY_VERIFY_WITH_TIMEOUT(guideB->isEnabled(), 2000);

    // Disconnect
    KTRY_GUIDE_CLICK(externalDisconnectB);
    QTRY_VERIFY_WITH_TIMEOUT(externalConnectB->isEnabled(), 10000);

    // Stop Simulators
    KTRY_EKOS_STOP_SIMULATORS();

    // Stop PHD2
    phd2.terminate();
    QVERIFY(phd2.waitForFinished(5000));
}

#endif
