/*
    Helper class of KStars UI tests

    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_ekos_helper.h"
#include "ksutils.h"

TestEkosHelper::TestEkosHelper(QString guider) {
    m_Guider = guider;
    m_MountDevice = "Telescope Simulator";
    m_CCDDevice   = "CCD Simulator";
    if (guider != nullptr)
        m_GuiderDevice = "Guide Simulator";
}


void TestEkosHelper::createEkosProfile(QString name, bool isPHD2, bool *isDone)
{
    ProfileEditor* profileEditor = Ekos::Manager::Instance()->findChild<ProfileEditor*>("profileEditorDialog");

    // Disable Port Selector
    KTRY_SET_CHECKBOX(profileEditor, portSelectorCheck, false);
    // Set the profile name
    KTRY_SET_LINEEDIT(profileEditor, profileIN, name);
    // select the guider type
    KTRY_SET_COMBO(profileEditor, guideTypeCombo, isPHD2 ? "PHD2" : "Internal");
    if (isPHD2)
    {
        // Write PHD2 server specs
        KTRY_SET_LINEEDIT(profileEditor, externalGuideHost, "localhost");
        KTRY_SET_LINEEDIT(profileEditor, externalGuidePort, "4400");
    }

    qCInfo(KSTARS_EKOS_TEST) << "Ekos profile " << name << " created.";
    // and now continue with filling the profile
    fillProfile(isDone);
}

void TestEkosHelper::fillProfile(bool *isDone)
{
    qCInfo(KSTARS_EKOS_TEST) << "Fill profile: starting...";
    ProfileEditor* profileEditor = Ekos::Manager::Instance()->findChild<ProfileEditor*>("profileEditorDialog");

    // Select the mount device
    if (m_MountDevice != nullptr) {
        KTRY_PROFILEEDITOR_GADGET(QComboBox, mountCombo);
        setTreeviewCombo(mountCombo, m_MountDevice);
        qCInfo(KSTARS_EKOS_TEST) << "Fill profile: Mount selected.";
    }

    // Selet the CCD device
    if (m_CCDDevice != nullptr) {
        KTRY_PROFILEEDITOR_GADGET(QComboBox, ccdCombo);
        setTreeviewCombo(ccdCombo, m_CCDDevice);
        qCInfo(KSTARS_EKOS_TEST) << "Fill profile: CCD selected.";
    }

    // Select the focuser device
    if (m_FocuserDevice != nullptr) {
        KTRY_PROFILEEDITOR_GADGET(QComboBox, focuserCombo);
        setTreeviewCombo(focuserCombo, m_FocuserDevice);
        qCInfo(KSTARS_EKOS_TEST) << "Fill profile: Focuser selected.";
    }

    // Select the guider device
    if (m_GuiderDevice != nullptr) {
        KTRY_PROFILEEDITOR_GADGET(QComboBox, guiderCombo);
        setTreeviewCombo(guiderCombo, m_GuiderDevice);
        qCInfo(KSTARS_EKOS_TEST) << "Fill profile: Guider selected.";
    }

    // wait a short time to make the setup visible
    QTest::qWait(1000);
    // Save the profile using the "Save" button
    QDialogButtonBox* buttons = profileEditor->findChild<QDialogButtonBox*>("dialogButtons");
    QVERIFY(nullptr != buttons);
    QTest::mouseClick(buttons->button(QDialogButtonBox::Save), Qt::LeftButton);

    qCInfo(KSTARS_EKOS_TEST) << "Fill profile: Selections saved.";

    *isDone = true;
}

bool TestEkosHelper::setupEkosProfile(QString name, bool isPHD2)
{
    qCInfo(KSTARS_EKOS_TEST) << "Setting up Ekos profile...";
    bool isDone = false;
    Ekos::Manager * const ekos = Ekos::Manager::Instance();
    // check if the profile with the given name exists
    KTRY_GADGET_SUB(ekos, QComboBox, profileCombo);
    if (profileCombo->findText(name) >= 0)
    {
        KTRY_GADGET_SUB(ekos, QPushButton, editProfileB);

        // edit Simulators profile
        KWRAP_SUB(KTRY_EKOS_SELECT_PROFILE(name));

        // edit only editable profiles
        if (editProfileB->isEnabled())
        {
            // start with a delay of 1 sec a new thread that edits the profile
            QTimer::singleShot(1000, ekos, [&]{fillProfile(&isDone);});
            KTRY_CLICK_SUB(ekos, editProfileB);
        }
        else
        {
            qCInfo(KSTARS_EKOS_TEST) << "Profile " << name << " not editable, setup finished.";
            isDone = true;
            return true;
        }
    }
    else
    {
        // start with a delay of 1 sec a new thread that edits the profile
        qCInfo(KSTARS_EKOS_TEST) << "Creating new profile " << name << " ...";
        QTimer::singleShot(1000, ekos, [&]{createEkosProfile(name, isPHD2, &isDone);});
        // create new profile addProfileB
        KTRY_CLICK_SUB(ekos, addProfileB);
    }


    // Cancel the profile editor if test did not close the editor dialog within 10 sec
    QTimer * closeDialog = new QTimer(this);
    closeDialog->setSingleShot(true);
    closeDialog->setInterval(10000);
    ekos->connect(closeDialog, &QTimer::timeout, [&]
    {
        ProfileEditor* profileEditor = ekos->findChild<ProfileEditor*>("profileEditorDialog");
        if (profileEditor != nullptr)
        {
            profileEditor->reject();
            qWarning(KSTARS_EKOS_TEST) << "Editing profile aborted.";
        }
    });


    // Click handler returned, stop the timer closing the dialog on failure
    closeDialog->stop();
    delete closeDialog;

    // Verification of the first test step
    return isDone;

}

bool TestEkosHelper::startEkosProfile()
{
    Ekos::Manager * const ekos = Ekos::Manager::Instance();

    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(ekos->setupTab, 1000));

    if (m_Guider == "PHD2")
    {
        // Start a PHD2 instance
        startPHD2();
        // setup the EKOS profile
        KWRAP_SUB(QVERIFY(setupEkosProfile("Simulators (PHD2)", true)));
    }
    else
        KWRAP_SUB(QVERIFY(setupEkosProfile("Simulators", false)));

    // start the profile
    KTRY_EKOS_CLICK(processINDIB);
    // wait for the devices to come up
    KWRAP_SUB(QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->indiStatus() == Ekos::Success, 10000));

    // connect to the alignment process to receive align status changes
    connect(ekos->alignModule(), &Ekos::Align::newStatus, this, &TestEkosHelper::alignStatusChanged,
            Qt::UniqueConnection);

    // connect to the mount process to rmount status changes
    connect(ekos->mountModule(), &Ekos::Mount::newStatus, this,
            &TestEkosHelper::mountStatusChanged, Qt::UniqueConnection);

    // connect to the mount process to receive meridian flip status changes
    connect(ekos->mountModule(), &Ekos::Mount::newMeridianFlipStatus, this,
            &TestEkosHelper::meridianFlipStatusChanged, Qt::UniqueConnection);

    // connect to the guiding process to receive guiding status changes
    connect(ekos->guideModule(), &Ekos::Guide::newStatus, this, &TestEkosHelper::guidingStatusChanged,
            Qt::UniqueConnection);

    connect(ekos->guideModule(), &Ekos::Guide::newAxisDelta, this, &TestEkosHelper::guideDeviationChanged,
            Qt::UniqueConnection);


    // connect to the capture process to receive capture status changes
    connect(ekos->captureModule(), &Ekos::Capture::newStatus, this, &TestEkosHelper::captureStatusChanged,
            Qt::UniqueConnection);

    // connect to the scheduler process to receive scheduler status changes
    connect(ekos->schedulerModule(), &Ekos::Scheduler::newStatus, this, &TestEkosHelper::schedulerStatusChanged,
            Qt::UniqueConnection);

    // connect to the focus process to receive focus status changes
    connect(ekos->focusModule(), &Ekos::Focus::newStatus, this, &TestEkosHelper::focusStatusChanged,
            Qt::UniqueConnection);

    // Everything completed successfully
    return true;
}

bool TestEkosHelper::shutdownEkosProfile()
{
    // disconnect to the focus process to receive focus status changes
    disconnect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::newStatus, this,
               &TestEkosHelper::focusStatusChanged);
    // disconnect the guiding process to receive the current guiding status
    disconnect(Ekos::Manager::Instance()->guideModule(), &Ekos::Guide::newStatus, this,
               &TestEkosHelper::guidingStatusChanged);
    // disconnect the mount process to receive mount status changes
    disconnect(Ekos::Manager::Instance()->mountModule(), &Ekos::Mount::newStatus, this,
               &TestEkosHelper::mountStatusChanged);
    // disconnect the mount process to receive meridian flip status changes
    disconnect(Ekos::Manager::Instance()->mountModule(), &Ekos::Mount::newMeridianFlipStatus, this,
               &TestEkosHelper::meridianFlipStatusChanged);
    // disconnect to the scheduler process to receive scheduler status changes
    disconnect(Ekos::Manager::Instance()->schedulerModule(), &Ekos::Scheduler::newStatus, this,
               &TestEkosHelper::schedulerStatusChanged);
    // disconnect to the capture process to receive capture status changes
    disconnect(Ekos::Manager::Instance()->captureModule(), &Ekos::Capture::newStatus, this,
               &TestEkosHelper::captureStatusChanged);
    // disconnect to the alignment process to receive align status changes
    disconnect(Ekos::Manager::Instance()->alignModule(), &Ekos::Align::newStatus, this,
               &TestEkosHelper::alignStatusChanged);

    if (m_Guider == "PHD2")
    {
        KTRY_GADGET_SUB(Ekos::Manager::Instance()->guideModule(), QPushButton, externalDisconnectB);
        // ensure that PHD2 is connected
        if (externalDisconnectB->isEnabled())
        {
            // click "Disconnect"
            KTRY_CLICK_SUB(Ekos::Manager::Instance()->guideModule(), externalDisconnectB);
            // Stop PHD2
            stopPHD2();
        }
    }

    qCInfo(KSTARS_EKOS_TEST) << "Stopping profile ...";
    KWRAP_SUB(KTRY_EKOS_STOP_SIMULATORS());
    qCInfo(KSTARS_EKOS_TEST) << "Stopping profile ... (done)";
    // Wait until the profile selection is enabled. This shows, that all devices are disconnected.
    KTRY_VERIFY_WITH_TIMEOUT_SUB(Ekos::Manager::Instance()->profileGroup->isEnabled() == true, 10000);
    // Everything completed successfully
    return true;
}

void TestEkosHelper::startPHD2()
{
    phd2 = new QProcess(this);

    // Start PHD2 with the proper configuration
    phd2->start(QString("phd2"));
    QVERIFY(phd2->waitForStarted(3000));
    QTest::qWait(2000);
    QTRY_VERIFY_WITH_TIMEOUT(phd2->state() == QProcess::Running, 1000);

    // Try to connect to the PHD2 server
    QTcpSocket phd2_server(this);
    phd2_server.connectToHost(phd2_guider_host, phd2_guider_port.toUInt(), QIODevice::ReadOnly, QAbstractSocket::IPv4Protocol);
    if(!phd2_server.waitForConnected(5000))
    {
        QWARN(QString("Cannot continue, PHD2 server is unavailable (%1)").arg(phd2_server.errorString()).toStdString().c_str());
        return;
    }
    phd2_server.disconnectFromHost();
    if (phd2_server.state() == QTcpSocket::ConnectedState)
        QVERIFY(phd2_server.waitForDisconnected(1000));
}

void TestEkosHelper::stopPHD2()
{
    phd2->terminate();
    QVERIFY(phd2->waitForFinished(5000));
}

void TestEkosHelper::preparePHD2()
{
    QString const phd2_config_name = ".PHDGuidingV2";
    QString const phd2_config_bak_name = ".PHDGuidingV2.bak";
    QString const phd2_config_orig_name = ".PHDGuidingV2_mf";
    QStandardPaths::enableTestMode(false);
    QFileInfo phd2_config_home(QStandardPaths::writableLocation(QStandardPaths::HomeLocation), phd2_config_name);
    QFileInfo phd2_config_home_bak(QStandardPaths::writableLocation(QStandardPaths::HomeLocation), phd2_config_bak_name);
    QFileInfo phd2_config_orig(phd2_config_orig_name);
    QStandardPaths::enableTestMode(true);
    QWARN(QString("Writing PHD configuration file to '%1'").arg(phd2_config_home.filePath()).toStdString().c_str());
    if (phd2_config_home.exists())
    {
        // remove existing backup file
        if (phd2_config_home_bak.exists())
            QVERIFY(QFile::remove(phd2_config_home_bak.filePath()));
        // rename existing file to backup file
        QVERIFY(QFile::rename(phd2_config_home.filePath(), phd2_config_home_bak.filePath()));
    }
    QVERIFY2(phd2_config_orig.exists(), phd2_config_orig_name.toLocal8Bit() + " not found in current directory!");
    QVERIFY(QFile::copy(phd2_config_orig_name, phd2_config_home.filePath()));
}

void TestEkosHelper::cleanupPHD2()
{
    QString const phd2_config = ".PHDGuidingV2";
    QString const phd2_config_bak = ".PHDGuidingV2.bak";
    QStandardPaths::enableTestMode(false);
    QFileInfo phd2_config_home(QStandardPaths::writableLocation(QStandardPaths::HomeLocation), phd2_config);
    QFileInfo phd2_config_home_bak(QStandardPaths::writableLocation(QStandardPaths::HomeLocation), phd2_config_bak);
    // remove PHD2 test config
    if (phd2_config_home.exists())
        QVERIFY(QFile::remove(phd2_config_home.filePath()));
    // restore the backup
    if (phd2_config_home_bak.exists())
        QVERIFY(QFile::rename(phd2_config_home_bak.filePath(), phd2_config_home.filePath()));
}

bool TestEkosHelper::slewTo(double RA, double DEC, bool fast)
{
    if (fast)
    {
        // reset mount model
        Ekos::Manager::Instance()->mountModule()->resetModel();
        // sync to a point close before the meridian to speed up slewing
        Ekos::Manager::Instance()->mountModule()->sync(RA + 0.002, DEC);
    }
    // now slew very close before the meridian
    Ekos::Manager::Instance()->mountModule()->slew(RA, DEC);
    // wait a certain time until the mount slews
    QTest::qWait(3000);
    // wait until the mount is tracking
    KTRY_VERIFY_WITH_TIMEOUT_SUB(Ekos::Manager::Instance()->mountModule()->status() == ISD::Telescope::MOUNT_TRACKING, 10000);

    // everything succeeded
    return true;
}

bool TestEkosHelper::startGuiding(double expTime)
{
    if (getGuidingStatus() == Ekos::GUIDE_GUIDING)
    {
        QWARN("Start guiding ignored, guiding already running!");
        return false;
    }

    // switch to guiding module
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->guideModule(), 1000));
    //set the exposure time
    KTRY_SET_DOUBLESPINBOX_SUB(Ekos::Manager::Instance()->guideModule(), exposureIN, expTime);

    // start guiding
    KTRY_GADGET_SUB(Ekos::Manager::Instance()->guideModule(), QPushButton, guideB);
    // ensure that the guiding button is enabled (after MF it may take a while)
    KTRY_VERIFY_WITH_TIMEOUT_SUB(guideB->isEnabled(), 10000);
    expectedGuidingStates.enqueue(Ekos::GUIDE_GUIDING);
    KTRY_CLICK_SUB(Ekos::Manager::Instance()->guideModule(), guideB);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(expectedGuidingStates, 120000);
    qCInfo(KSTARS_EKOS_TEST) << "Guiding started.";
    qCInfo(KSTARS_EKOS_TEST) << "Waiting 2sec for settle guiding ...";
    QTest::qWait(2000);
    // all checks succeeded, remember that guiding is running
    use_guiding = true;

    return true;
}

bool TestEkosHelper::stopGuiding()
{
    // check whether guiding is not running or already stopped
    if (use_guiding == false || getGuidingStatus() == Ekos::GUIDE_IDLE || getGuidingStatus() == Ekos::GUIDE_ABORTED)
        return true;

    // switch to guiding module
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->guideModule(), 1000));

    // stop guiding
    KTRY_GADGET_SUB(Ekos::Manager::Instance()->guideModule(), QPushButton, stopB);
    KTRY_CLICK_SUB(Ekos::Manager::Instance()->guideModule(), stopB);
    KTRY_VERIFY_WITH_TIMEOUT_SUB(getGuidingStatus() == Ekos::GUIDE_IDLE || getGuidingStatus() == Ekos::GUIDE_ABORTED, 15000);
    qCInfo(KSTARS_EKOS_TEST) << "Guiding stopped.";
    qCInfo(KSTARS_EKOS_TEST) << "Waiting 2sec for settle guiding stop..."; // Avoid overlapping with focus pausing
    QTest::qWait(2000);
    // all checks succeeded
    return true;
}

/* *********************************************************************************
 *
 * Alignment support
 *
 * ********************************************************************************* */
bool TestEkosHelper::startAligning(double expTime)
{
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->alignModule(), 1000));
    // set the exposure time to the given value
    KTRY_SET_DOUBLESPINBOX_SUB(Ekos::Manager::Instance()->alignModule(), exposureIN, expTime);
    // reduce the accuracy to avoid testing problems
    KTRY_SET_SPINBOX_SUB(Ekos::Manager::Instance()->alignModule(), accuracySpin, 300);

    // start alignment
    KTRY_GADGET_SUB(Ekos::Manager::Instance()->alignModule(), QPushButton, solveB);
    // ensure that the guiding button is enabled (after MF it may take a while)
    KTRY_VERIFY_WITH_TIMEOUT_SUB(solveB->isEnabled(), 10000);
    KTRY_CLICK_SUB(Ekos::Manager::Instance()->alignModule(), solveB);
    // success
    return true;
}


bool TestEkosHelper::checkAstrometryFiles()
{
    // first check the current configuration
    QStringList dataDirs = KSUtils::getAstrometryDataDirs();

    // search if configured directories contain some index files
    for(QString dirname : dataDirs)
    {
        QDir dir(dirname);
        dir.setNameFilters(QStringList("index*"));
        dir.setFilter(QDir::Files);
        if (! dir.entryList().isEmpty())
            return true;
    }
    return false;
}

void TestEkosHelper::init() {
    // initialize the recorded states
    m_AlignStatus   = Ekos::ALIGN_IDLE;
    m_CaptureStatus = Ekos::CAPTURE_IDLE;
    m_FocusStatus   = Ekos::FOCUS_IDLE;
    m_GuideStatus   = Ekos::GUIDE_IDLE;
    m_MFStatus      = Ekos::Mount::FLIP_NONE;

    // disable by default
    use_guiding     = false;
}
void TestEkosHelper::cleanup() {

}

/* *********************************************************************************
 *
 * Helper functions
 *
 * ********************************************************************************* */
void TestEkosHelper::setTreeviewCombo(QComboBox *combo, QString lookup)
{
    // Match the text recursively in the model, this results in a model index with a parent
    QModelIndexList const list = combo->model()->match(combo->model()->index(0, 0), Qt::DisplayRole, QVariant::fromValue(lookup), 1, Qt::MatchRecursive);
    QVERIFY(0 < list.count());
    QModelIndex const &index = list.first();
    QCOMPARE(list.value(0).data().toString(), lookup);
    QVERIFY(!index.parent().parent().isValid());
    // Now set the combobox model root to the match's parent
    combo->setRootModelIndex(index.parent());
    combo->setModelColumn(index.column());
    combo->setCurrentIndex(index.row());

    // Now reset
    combo->setRootModelIndex(QModelIndex());
    combo->view()->setCurrentIndex(index);

    // Check, if everything went well
    QCOMPARE(combo->currentText(), lookup);
}


// Simple write-string-to-file utility.
bool TestEkosHelper::writeFile(const QString &filename, const QStringList &lines, QFileDevice::Permissions permissions)
{
    QFile qFile(filename);
    if (qFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream out(&qFile);
        for (QStringList::const_iterator it = lines.begin(); it != lines.end(); it++)
            out << *it + QChar::LineFeed;
        qFile.close();
        qFile.setPermissions(filename, permissions);
        return true;
    }
    return false;
}

/* *********************************************************************************
 *
 * Slots for catching state changes
 *
 * ********************************************************************************* */

void TestEkosHelper::alignStatusChanged(Ekos::AlignState status)
{
    m_AlignStatus = status;
    // check if the new state is the next one expected, then remove it from the stack
    if (!expectedAlignStates.isEmpty() && expectedAlignStates.head() == status)
        expectedAlignStates.dequeue();
}

void TestEkosHelper::mountStatusChanged(ISD::Telescope::Status status)
{
    m_MountStatus = status;
    // check if the new state is the next one expected, then remove it from the stack
    if (!expectedMountStates.isEmpty() && expectedMountStates.head() == status)
        expectedMountStates.dequeue();
}

void TestEkosHelper::meridianFlipStatusChanged(Ekos::Mount::MeridianFlipStatus status)
{
    m_MFStatus = status;
    // check if the new state is the next one expected, then remove it from the stack
    if (!expectedMeridianFlipStates.isEmpty() && expectedMeridianFlipStates.head() == status)
        expectedMeridianFlipStates.dequeue();
}

void TestEkosHelper::focusStatusChanged(Ekos::FocusState status)
{
    m_FocusStatus = status;
    // check if the new state is the next one expected, then remove it from the stack
    if (!expectedFocusStates.isEmpty() && expectedFocusStates.head() == status)
        expectedFocusStates.dequeue();
}

void TestEkosHelper::guidingStatusChanged(Ekos::GuideState status)
{
    m_GuideStatus = status;
    // check if the new state is the next one expected, then remove it from the stack
    if (!expectedGuidingStates.isEmpty() && expectedGuidingStates.head() == status)
        expectedGuidingStates.dequeue();
}

void TestEkosHelper::guideDeviationChanged(double delta_ra, double delta_dec)
{
    m_GuideDeviation = std::hypot(delta_ra, delta_dec);
}

void TestEkosHelper::captureStatusChanged(Ekos::CaptureState status)
{
    m_CaptureStatus = status;
    // check if the new state is the next one expected, then remove it from the stack
    if (!expectedCaptureStates.isEmpty() && expectedCaptureStates.head() == status)
        expectedCaptureStates.dequeue();
}

void TestEkosHelper::schedulerStatusChanged(Ekos::SchedulerState status)
{
    m_SchedulerStatus = status;
    // check if the new state is the next one expected, then remove it from the stack
    if (!expectedSchedulerStates.isEmpty() && expectedSchedulerStates.head() == status)
        expectedSchedulerStates.dequeue();
}
