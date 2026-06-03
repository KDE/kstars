/*
    Helper class of KStars UI tests

    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_ekos_helper.h"
#include "ksutils.h"
#include "Options.h"
#include "ekos/profileeditor.h"
#include "ekos/guide/internalguide/gmath.h"
#include "ekos/auxiliary/opticaltrainmanager.h"
#include "ekos/align/align.h"
#include "ekos/capture/capture.h"
#include "ekos/focus/focusmodule.h"
#include "ekos/scheduler/scheduler.h"
#include "ekos/scheduler/schedulermodulestate.h"
#include "ekos/auxiliary/filtermanager.h"
#include "indi/drivermanager.h"
#include "indi/driverinfo.h"
#include "kstarsdata.h"
#include "../testhelpers.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonObject>

namespace
{
void setTreeviewCombo(QComboBox *combo, const QString &label)
{
    QVERIFY2(combo != nullptr, "Profile combo box is missing.");
    QAbstractItemModel *model = combo->model();
    QVERIFY2(model != nullptr, "Profile combo model is missing.");

    const QModelIndexList matches = model->match(model->index(0, 0), Qt::DisplayRole,
                                    QVariant::fromValue(label), 1, Qt::MatchRecursive);
    QVERIFY2(!matches.isEmpty(), QString("Profile entry '%1' not found").arg(label).toStdString().c_str());

    const QModelIndex &item = matches.first();
    QCOMPARE(matches.value(0).data().toString(), label);
    QVERIFY(!item.parent().parent().isValid());
    combo->setRootModelIndex(item.parent());
    combo->setCurrentText(label);
    QCOMPARE(combo->currentText(), label);
}
}

TestEkosHelper::TestEkosHelper(QString guider)
{
    m_Guider = guider;
    m_MountDevice = "Telescope Simulator";
    m_CCDDevice   = "CCD Simulator";
    if (guider != nullptr)
        m_GuiderDevice = "Guide Simulator";
    m_astrometry_available = false;
}


void TestEkosHelper::createEkosProfile(QString name, bool isPHD2, bool *isDone)
{
    ProfileEditor* profileEditor = Ekos::Manager::Instance()->findChild<ProfileEditor*>("profileEditorDialog");

    // Disable Port Selector
    KTRY_SET_CHECKBOX(profileEditor, portSelectorCheck, false);
    // Set the profile name
    KTRY_SET_LINEEDIT(profileEditor, profileIN, name);
    // select the guider type
    KTRY_SET_COMBO(profileEditor, guideTypeCombo, (isPHD2 ? "PHD2" : "Internal"));
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

void TestEkosHelper::addDriverToProfile(QString drivername)
{
    KTRY_PROFILEEDITOR_GADGET(QLineEdit, driverSearchEdit);
    driverSearchEdit->setText(drivername);
    KTRY_PROFILEEDITOR_GADGET(QTreeView, driversTree);
    auto *driversModel = qobject_cast<QStandardItemModel*>(driversTree->model());

    for (int i = 0; i < driversModel->rowCount(); ++i)
    {
        QStandardItem *familyItem = driversModel->item(i);

        for (int j = 0; j < familyItem->rowCount(); ++j)
        {
            QStandardItem *manufacturerItem = familyItem->child(j);
            for (int k = 0; k < manufacturerItem->rowCount(); ++k)
            {
                QStandardItem *driverItem = manufacturerItem->child(k);
                if (!driversTree->isRowHidden(k, manufacturerItem->index()))
                {
                    // select the driver
                    driversTree->scrollTo(driverItem->index(), QAbstractItemView::PositionAtCenter);
                    driversTree->setCurrentIndex(driverItem->index());
                    driversTree->selectionModel()->select(driverItem->index(), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

                    // add the driver
                    KTRY_PROFILEEDITOR_GADGET(QPushButton, addDriverB);
                    addDriverB->click();
                    return;
                }
            }
        }
    }
}

void TestEkosHelper::fillProfile(bool *isDone)
{
    qCInfo(KSTARS_EKOS_TEST) << "Fill profile: starting...";
    ProfileEditor* profileEditor = Ekos::Manager::Instance()->findChild<ProfileEditor*>("profileEditorDialog");

    auto selectProfileDevice = [&](const QString & label, const char *comboName, const QString & logLabel)
    {
        QComboBox *combo = profileEditor->findChild<QComboBox*>(comboName);
        if (combo != nullptr)
        {
            setTreeviewCombo(combo, label);
        }
        else
        {
            kstarsTestAddProfileDriver(label);
        }
        qCInfo(KSTARS_EKOS_TEST) << "Fill profile: " << logLabel << " selected.";
    };

    // Select the mount device
    if (m_MountDevice != "")
    {
        selectProfileDevice(m_MountDevice, "mountCombo", "Mount");
    }

    // Select the CCD device
    if (m_CCDDevice != "")
    {
        selectProfileDevice(m_CCDDevice, "ccdCombo", "CCD");
    }

    // Select the focuser device
    if (m_FocuserDevice != "")
    {
        selectProfileDevice(m_FocuserDevice, "focuserCombo", "Focuser");
    }

    // Select the guider device, if not empty and different from CCD
    if (m_GuiderDevice != "" && m_GuiderDevice != m_CCDDevice)
    {
        selectProfileDevice(m_GuiderDevice, "guiderCombo", "Guider");
    }

    // Select the light panel device for flats capturing
    if (m_LightPanelDevice != "")
    {
        selectProfileDevice(m_LightPanelDevice, "aux1Combo", "Light panel");
    }

    // Select the dome device
    if (m_DomeDevice != "")
    {
        selectProfileDevice(m_DomeDevice, "domeCombo", "Dome");
    }

    // Select the rotator device
    if (m_RotatorDevice != "")
    {
        selectProfileDevice(m_RotatorDevice, "aux2Combo", "Rotator");
    }

    // Select any extra devices requested
    for (const QString &dev : m_ExtraDevices)
    {
        addDriverToProfile(dev);
    }

    // Save the profile using the "Save" button
    QDialogButtonBox* buttons = profileEditor->findChild<QDialogButtonBox*>("dialogButtons");
    QVERIFY(nullptr != buttons);
    auto *saveButton = buttons->button(QDialogButtonBox::Save);
    QVERIFY(nullptr != saveButton);
    QTRY_VERIFY_WITH_TIMEOUT(saveButton->isEnabled(), 5000);
    QTest::mouseClick(saveButton, Qt::LeftButton);

    qCInfo(KSTARS_EKOS_TEST) << "Fill profile: Selections saved.";

    *isDone = true;
}

bool TestEkosHelper::setupEkosProfile(QString name, bool isPHD2)
{
    qCInfo(KSTARS_EKOS_TEST) << "Setting up Ekos profile...";
    bool isDone = false;
    Ekos::Manager * const ekos = Ekos::Manager::Instance();
    QTimer closeDialog;
    closeDialog.setSingleShot(true);
    closeDialog.setInterval(10000);
    connect(&closeDialog, &QTimer::timeout, this, [ &, ekos]
    {
        ProfileEditor* profileEditor = ekos->findChild<ProfileEditor*>("profileEditorDialog");
        if (profileEditor != nullptr)
        {
            profileEditor->reject();
            qWarning(KSTARS_EKOS_TEST) << "Editing profile aborted.";
        }
    });
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
            QTimer::singleShot(1000, ekos, [&] {fillProfile(&isDone);});
            KTRY_CLICK_SUB(ekos, editProfileB);
            closeDialog.start();
            KTRY_VERIFY_WITH_TIMEOUT_SUB(isDone, 15000);
            closeDialog.stop();
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
        QTimer::singleShot(1000, ekos, [&] {createEkosProfile(name, isPHD2, &isDone);});
        // create new profile addProfileB
        KTRY_CLICK_SUB(ekos, addProfileB);
        closeDialog.start();
        KTRY_VERIFY_WITH_TIMEOUT_SUB(isDone, 15000);
        closeDialog.stop();
    }

    // Verification of the first test step
    return isDone;

}

void TestEkosHelper::connectModules()
{
    Ekos::Manager * const ekos = Ekos::Manager::Instance();

    // wait for modules startup
    if (m_MountDevice != "")
        QTRY_VERIFY_WITH_TIMEOUT(ekos->mountModule() != nullptr, 10000);
    if (m_CCDDevice != "")
    {
        QTRY_VERIFY_WITH_TIMEOUT(ekos->captureModule() != nullptr, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(ekos->alignModule() != nullptr, 10000);
    }
    if (m_GuiderDevice != "")
        QTRY_VERIFY_WITH_TIMEOUT(ekos->guideModule() != nullptr, 10000);
    if (m_FocuserDevice != "")
        QTRY_VERIFY_WITH_TIMEOUT(ekos->focusModule() != nullptr, 10000);

    // connect to the alignment process to receive align status changes
    connect(ekos->alignModule(), &Ekos::Align::newStatus, this, &TestEkosHelper::alignStatusChanged,
            Qt::UniqueConnection);

    if (m_MountDevice != "")
    {
        // connect to the mount process to rmount status changes
        connect(ekos->mountModule(), &Ekos::Mount::newStatus, this,
                &TestEkosHelper::mountStatusChanged, Qt::UniqueConnection);

        // connect to the mount process to receive meridian flip status changes
        connect(ekos->mountModule()->getMeridianFlipState().get(), &Ekos::MeridianFlipState::newMountMFStatus, this,
                &TestEkosHelper::meridianFlipStatusChanged, Qt::UniqueConnection);
    }

    if (m_GuiderDevice != "")
    {
        // connect to the guiding process to receive guiding status changes
        connect(ekos->guideModule(), &Ekos::Guide::newStatus, this, &TestEkosHelper::guidingStatusChanged,
                Qt::UniqueConnection);

        connect(ekos->guideModule(), &Ekos::Guide::newAxisDelta, this, &TestEkosHelper::guideDeviationChanged,
                Qt::UniqueConnection);
    }

    // connect to the capture process to receive capture status changes
    if (m_CCDDevice != "")
        connect(ekos->captureModule(), &Ekos::Capture::newStatus, this, &TestEkosHelper::captureStatusChanged,
                Qt::UniqueConnection);

    // connect to the scheduler process to receive scheduler status changes
    connect(ekos->schedulerModule(), &Ekos::Scheduler::newStatus, this, &TestEkosHelper::schedulerStatusChanged,
            Qt::UniqueConnection);

    // connect to the focus process to receive focus status changes
    if (m_FocuserDevice != "")
        connect(ekos->focusModule(), &Ekos::FocusModule::newStatus, this, &TestEkosHelper::focusStatusChanged,
                Qt::UniqueConnection);

    // connect to the dome process to receive dome status changes
    //    connect(ekos->domeModule(), &Ekos::Dome::newStatus, this, &TestEkosHelper::domeStatusChanged,
    //            Qt::UniqueConnection);
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
        KWRAP_SUB(QVERIFY(setupEkosProfile("Test profile (PHD2)", true)));
    }
    else
        KWRAP_SUB(QVERIFY(setupEkosProfile("Test profile", false)));

    // start the profile
    KTRY_EKOS_CLICK(processINDIB);
    // wait for the devices to come up
    KWRAP_SUB(QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->indiStatus() == Ekos::Success, 10000));
    // receive status updates from all devices
    connectModules();

    // Everything completed successfully
    return true;
}

bool TestEkosHelper::shutdownEkosProfile()
{
    // disconnect to the dome process to receive dome status changes
    //    disconnect(Ekos::Manager::Instance()->domeModule(), &Ekos::Dome::newStatus, this,
    //               &TestEkosHelper::domeStatusChanged);
    // disconnect to the focus process to receive focus status changes
    disconnect(Ekos::Manager::Instance()->focusModule(), &Ekos::FocusModule::newStatus, this,
               &TestEkosHelper::focusStatusChanged);
    // disconnect the guiding process to receive the current guiding status
    disconnect(Ekos::Manager::Instance()->guideModule(), &Ekos::Guide::newStatus, this,
               &TestEkosHelper::guidingStatusChanged);
    // disconnect the mount process to receive mount status changes
    disconnect(Ekos::Manager::Instance()->mountModule(), &Ekos::Mount::newStatus, this,
               &TestEkosHelper::mountStatusChanged);
    // disconnect the mount process to receive meridian flip status changes
    disconnect(Ekos::Manager::Instance()->mountModule()->getMeridianFlipState().get(),
               &Ekos::MeridianFlipState::newMountMFStatus, this,
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
    QStringList arguments;
    // Start PHD2 with the proper configuration
    phd2->start(QString("phd2"), arguments);
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
    const QString phd2_home = KTest::homePath();
    QDir(phd2_home).mkpath("kstars_ui");
    QFileInfo phd2_config_home(QDir(phd2_home).filePath(phd2_config_name));
    QFileInfo phd2_config_home_bak(QDir(phd2_home).filePath(phd2_config_bak_name));
    QFileInfo phd2_config_orig(phd2_config_orig_name);
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
    QFile configSource(phd2_config_orig.filePath());
    QVERIFY2(configSource.open(QIODevice::ReadOnly | QIODevice::Text),
             qPrintable(QString("Unable to read %1: %2").arg(phd2_config_orig.filePath(), configSource.errorString())));
    QString configContents = QString::fromUtf8(configSource.readAll());
    configSource.close();
    configContents.replace(QStringLiteral("@TEST_HOME@"), KTest::homePath());
    QFile configDest(phd2_config_home.filePath());
    QVERIFY2(configDest.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate),
             qPrintable(QString("Unable to write %1: %2").arg(phd2_config_home.filePath(), configDest.errorString())));
    QVERIFY(configDest.write(configContents.toUtf8()) >= 0);
    configDest.close();
}

void TestEkosHelper::cleanupPHD2()
{
    QString const phd2_config = ".PHDGuidingV2";
    QString const phd2_config_bak = ".PHDGuidingV2.bak";
    const QString phd2_home = KTest::homePath();
    QFileInfo phd2_config_home(QDir(phd2_home).filePath(phd2_config));
    QFileInfo phd2_config_home_bak(QDir(phd2_home).filePath(phd2_config_bak));
    // remove PHD2 test config
    if (phd2_config_home.exists())
        QVERIFY(QFile::remove(phd2_config_home.filePath()));
    // restore the backup
    if (phd2_config_home_bak.exists())
        QVERIFY(QFile::rename(phd2_config_home_bak.filePath(), phd2_config_home.filePath()));
}

void TestEkosHelper::prepareOpticalTrains()
{
    Ekos::OpticalTrainManager *otm = Ekos::OpticalTrainManager::Instance();
    // close window, we change everything programmatically
    otm->close();
    // setup train with main scope and camera
    QVariantMap primaryTrain = otm->getOpticalTrain(m_primaryTrain);
    primaryTrain["mount"] = m_MountDevice;
    primaryTrain["camera"] = m_CCDDevice;
    primaryTrain["filterwheel"] = m_CCDDevice;
    primaryTrain["focuser"] = m_FocuserDevice == "" ? "-" : m_FocuserDevice;
    primaryTrain["rotator"] = m_RotatorDevice == "" ? "-" : m_RotatorDevice;
    primaryTrain["lightbox"] = m_LightPanelDevice == "" ? "-" : m_LightPanelDevice;
    primaryTrain["dustcap"] = m_DustCapDevice == "" ? "-" : m_DustCapDevice;
    KStarsData::Instance()->userdb()->UpdateOpticalTrain(primaryTrain, primaryTrain["id"].toInt());
    if (m_GuiderDevice != "")
    {
        // setup guiding scope train
        QVariantMap guidingTrain = otm->getOpticalTrain(m_guidingTrain);
        bool isNew = guidingTrain.size() == 0;
        guidingTrain["mount"] = m_MountDevice;
        guidingTrain["camera"] = m_GuiderDevice;
        guidingTrain["filterwheel"] = "-";
        guidingTrain["focuser"] = "-";
        guidingTrain["guider"] = m_MountDevice;
        if (isNew)
        {
            // create guiding train if missing
            guidingTrain["name"] = m_guidingTrain;
            otm->addOpticalTrain(QJsonObject::fromVariantMap(guidingTrain));
        }
        else
            KStarsData::Instance()->userdb()->UpdateOpticalTrain(guidingTrain, guidingTrain["id"].toInt());
    }
    // ensure that the OTM initializes from the database
    otm->refreshModel();
    otm->refreshTrains();
}

void TestEkosHelper::prepareAlignmentModule()
{

    // check if astrometry files exist
    QTRY_VERIFY(isAstrometryAvailable() == true);
    // switch to alignment module
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->alignModule(), 1000);
    // select the primary train for alignment
    KTRY_SET_COMBO(Ekos::Manager::Instance()->alignModule(), opticalTrainCombo, m_primaryTrain);
    // select local solver
    Ekos::Manager::Instance()->alignModule()->setSolverMode(Ekos::Align::SOLVER_LOCAL);
    // select internal SEP method
    Options::setSolveSextractorType(SSolver::EXTRACTOR_BUILTIN);
    // select StellarSolver
    Options::setSolverType(SSolver::SOLVER_STELLARSOLVER);
    // select fast solve profile option
    Options::setSolveOptionsProfile(SSolver::Parameters::SINGLE_THREAD_SOLVING);
    // select the "Slew to Target" mode
    KTRY_SET_RADIOBUTTON(Ekos::Manager::Instance()->alignModule(), slewR, true);
    // disable rotator check in alignment
    Options::setAstrometryUseRotator(false);
    // select the Luminance filter
    KTRY_SET_COMBO(Ekos::Manager::Instance()->alignModule(), alignFilter, "Luminance");
    // set the exposure time to a standard
    KTRY_SET_DOUBLESPINBOX(Ekos::Manager::Instance()->alignModule(), alignExposure, 3.0);
    // reduce the accuracy to avoid testing problems
    KTRY_SET_SPINBOX(Ekos::Manager::Instance()->alignModule(), alignAccuracyThreshold, 300);
    // disable re-alignment
    Options::setAlignCheckFrequency(0);
}

void TestEkosHelper::prepareCaptureModule()
{
    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    // clear refocusing limits
    KTRY_SET_CHECKBOX(capture, enforceRefocusEveryN, false);
    KTRY_SET_CHECKBOX(capture, enforceAutofocusHFR, false);
    KTRY_SET_CHECKBOX(capture, enforceAutofocusOnTemperature, false);
    // clear the guiding limits
    KTRY_SET_CHECKBOX(capture, enforceStartGuiderDrift, false);
    KTRY_SET_CHECKBOX(capture, enforceGuideDeviation, false);

}

void TestEkosHelper::prepareFocusModule()
{
    // set focus mode defaults
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->focusModule(), 1000);
    // select the primary train for focusing
    KTRY_SET_COMBO(Ekos::Manager::Instance()->focusModule()->mainFocuser().get(), opticalTrainCombo, m_primaryTrain);
    // use full field
    KTRY_SET_RADIOBUTTON(Ekos::Manager::Instance()->focusModule()->mainFocuser().get(), focusUseFullField, true);
    //initial step size 5000
    KTRY_SET_SPINBOX(Ekos::Manager::Instance()->focusModule()->mainFocuser().get(), focusTicks, 5000);
    // max travel 100000
    KTRY_SET_DOUBLESPINBOX(Ekos::Manager::Instance()->focusModule()->mainFocuser().get(), focusMaxTravel, 100000.0);
    // focus tolerance 20% - make focus fast and robust, precision does not matter
    KTRY_GADGET(Ekos::Manager::Instance()->focusModule()->mainFocuser().get(), QDoubleSpinBox, focusTolerance);
    focusTolerance->setMaximum(20.0);
    focusTolerance->setValue(20.0);
    // use single pass linear algorithm
    KTRY_SET_COMBO(Ekos::Manager::Instance()->focusModule()->mainFocuser().get(), focusAlgorithm, "Linear 1 Pass");
    // select star detection
    KTRY_SET_COMBO(Ekos::Manager::Instance()->focusModule()->mainFocuser().get(), focusSEPProfile, "1-Focus-Default");
    // set annulus to 0% - 100%
    KTRY_SET_DOUBLESPINBOX(Ekos::Manager::Instance()->focusModule()->mainFocuser().get(), focusFullFieldInnerRadius, 0.0);
    KTRY_SET_DOUBLESPINBOX(Ekos::Manager::Instance()->focusModule()->mainFocuser().get(), focusFullFieldOuterRadius, 100.0);
    // try to make focusing fast, precision is not relevant here
    KTRY_SET_DOUBLESPINBOX(Ekos::Manager::Instance()->focusModule()->mainFocuser().get(), focusOutSteps, 3.0);
    // select the Luminance filter
    KTRY_SET_COMBO(Ekos::Manager::Instance()->focusModule()->mainFocuser().get(), focusFilter, "Luminance");
    // select SEP algorithm for star detection
    KTRY_SET_COMBO(Ekos::Manager::Instance()->focusModule()->mainFocuser().get(), focusDetection, "SEP");
    // select HFR a star focus measure
    KTRY_SET_COMBO(Ekos::Manager::Instance()->focusModule()->mainFocuser().get(), focusStarMeasure, "HFR");
    // set exp time for current filter
    KTRY_SET_DOUBLESPINBOX(Ekos::Manager::Instance()->focusModule()->mainFocuser().get(), focusExposure, 1.0);
    // set exposure times for all filters
    auto filtermanager = Ekos::Manager::Instance()->focusModule()->mainFocuser()->filterManager();
    for (int pos = 0; pos < filtermanager->getFilterLabels().count(); pos++)
    {
        filtermanager->setFilterExposure(pos, 1.0);
        filtermanager->setFilterLock(pos, "Luminance");
    }

    // Eliminate the noise setting to ensure a working focusing and plate solving
    KTRY_INDI_PROPERTY(m_CCDDevice, "Simulator Config", "SIMULATOR_SETTINGS", ccd_settings);
    INDI_E *noise_setting = ccd_settings->getElement("SIM_NOISE");
    QVERIFY(ccd_settings != nullptr);
    noise_setting->setValue(0.0);
    ccd_settings->processSetButton();

    // gain 100
    KTRY_SET_DOUBLESPINBOX(Ekos::Manager::Instance()->focusModule()->mainFocuser().get(), focusGain, 100.0);
    // suspend guiding while focusing
    KTRY_SET_CHECKBOX(Ekos::Manager::Instance()->focusModule()->mainFocuser().get(), focusSuspendGuiding, true);
}

void TestEkosHelper::prepareGuidingModule()
{
    // switch to guiding module
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->guideModule(), 1000);

    // preserve guiding calibration as good as possible
    Options::setReuseGuideCalibration(true);
    Options::setResetGuideCalibration(false);
    // guide calibration captured with fsq-85 as guiding scope, clear if it creates problems
    // KTRY_CLICK(Ekos::Manager::Instance()->guideModule(), clearCalibrationB);
    Options::setSerializedCalibration("Cal v1.1,bx=1,by=1,pw=0.0024,ph=0.0024,fl=450,ang=269.014,angR=268.755,angD=359.273,ramspas=125.814,decmspas=134.457,swap=0,ra= 80:21:17,dec=00:25:52,side=0,when=2025-12-21 00:13:11,calEnd");
    // 0.5 pixel dithering
    Options::setDitherPixels(0.5);
    // auto star select
    KTRY_SET_CHECKBOX(Ekos::Manager::Instance()->guideModule(), guideAutoStar, true);
    // set the guide star box to size 32
    KTRY_SET_COMBO(Ekos::Manager::Instance()->guideModule(), guideSquareSize, "32");
    // use 1x1 binning for guiding
    KTRY_SET_COMBO(Ekos::Manager::Instance()->guideModule(), guideBinning, "1x1");
    if (m_Guider == "PHD2")
    {
        KTRY_GADGET(Ekos::Manager::Instance()->guideModule(), QPushButton, externalConnectB);
        // ensure that PHD2 is connected
        if (externalConnectB->isEnabled())
        {
            // click "Connect"
            KTRY_CLICK(Ekos::Manager::Instance()->guideModule(), externalConnectB);
            // wait max 60 sec that PHD2 is connected (sometimes INDI connections hang)
            QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->guideModule()->status() == Ekos::GUIDE_CONNECTED, 60000);
            qCInfo(KSTARS_EKOS_TEST) << "PHD2 connected successfully.";
        }
    }
    else
    {
        // select the secondary train for guiding
        KTRY_SET_COMBO(Ekos::Manager::Instance()->guideModule(), opticalTrainCombo, m_guidingTrain);
        // select multi-star
        Options::setGuideAlgorithm(SEP_MULTISTAR);
        // select small star profile
        Options::setGuideOptionsProfile(2);
        // set 2 sec guiding calibration pulse duration
        Options::setCalibrationPulseDuration(2000);
        // use three steps in each direction for calibration
        Options::setAutoModeIterations(3);
        // set the guiding aggressiveness
        Options::setRAProportionalGain(0.5);
        Options::setDECProportionalGain(0.5);
        // use simulator's guide head
        Options::setUseGuideHead(true);
    }
}

Scope *TestEkosHelper::createScopeIfNecessary(QString model, QString vendor, QString type, double aperture,
        double focallenght)
{
    QList<Scope *> scope_list;
    KStarsData::Instance()->userdb()->GetAllScopes(scope_list);

    for (Scope *scope : scope_list)
    {
        if (scope->model() == model && scope->vendor() == vendor && scope->type() == type && scope->aperture() == aperture
                && scope->focalLength() == focallenght)
            return scope;
    }
    // no match found, create it again
    KStarsData::Instance()->userdb()->AddScope(model, vendor, type, aperture, focallenght);
    Ekos::OpticalTrainManager::Instance()->refreshOpticalElements();
    // find it
    scope_list.clear();
    KStarsData::Instance()->userdb()->GetAllScopes(scope_list);
    for (Scope *scope : scope_list)
    {
        if (scope->model() == model && scope->vendor() == vendor && scope->type() == type && scope->aperture() == aperture
                && scope->focalLength() == focallenght)
            return scope;
    }
    // this should never happen
    return nullptr;
}



OAL::Scope *TestEkosHelper::getScope(TestEkosHelper::ScopeType type)
{
    switch (type)
    {
        case SCOPE_FSQ85:
            return fsq85;
        case SCOPE_NEWTON_10F4:
            return newton_10F4;
        case SCOPE_TAKFINDER10x50:
            return takfinder10x50;
    }
    // this should never happen
    return fsq85;
}

void TestEkosHelper::checkModuleConfigurationsCompleted()
{
    if (waitForSettingsUpdated)
    {
        qCInfo(KSTARS_EKOS_TEST) << "Waiting for module settings update";
        QTest::qWait(settingsUpdateDelay);
        qCInfo(KSTARS_EKOS_TEST) << "Waiting for module settings update (finished)";
    }
}

void TestEkosHelper::prepareMountModule(ScopeType primary, ScopeType guiding)
{
    Ekos::OpticalTrainManager *otm = Ekos::OpticalTrainManager::Instance();
    // set mount defaults
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->mountModule() != nullptr, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->mountModule()->slewStatus() != IPState::IPS_ALERT, 1000);

    // define a set of scopes
    fsq85          = createScopeIfNecessary("FSQ-85", "Takahashi", "Refractor", 85.0, 450.0);
    newton_10F4    = createScopeIfNecessary("ONTC", "Teleskop-Service", "Newtonian", 254.0, 1000.0);
    takfinder10x50 = createScopeIfNecessary("Finder 7x50", "Takahashi", "Refractor", 50.0, 170.0);

    QVERIFY(fsq85 != nullptr);
    QVERIFY(newton_10F4 != nullptr);
    QVERIFY(takfinder10x50 != nullptr);

    OAL::Scope *primaryScope = getScope(primary);
    OAL::Scope *guidingScope = getScope(guiding);

    // setup the primary train
    QVariantMap primaryTrain = otm->getOpticalTrain(m_primaryTrain);
    primaryTrain["scope"] = primaryScope->name();
    primaryTrain["reducer"] = 1.0;
    KStarsData::Instance()->userdb()->UpdateOpticalTrain(primaryTrain, primaryTrain["id"].toInt());

    // setup the guiding train
    QVariantMap guidingTrain = otm->getOpticalTrain(m_guidingTrain);
    guidingTrain["scope"] = guidingScope->name();
    guidingTrain["reducer"] = 1.0;
    KStarsData::Instance()->userdb()->UpdateOpticalTrain(guidingTrain, guidingTrain["id"].toInt());
    // ensure that the OTM initializes from the database
    otm->refreshModel();
    otm->refreshTrains();

    // select the primary train for the mount
    KTRY_SET_COMBO(Ekos::Manager::Instance()->mountModule(), opticalTrainCombo, m_primaryTrain);
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
    KTRY_VERIFY_WITH_TIMEOUT_SUB(Ekos::Manager::Instance()->mountModule()->status() == ISD::Mount::MOUNT_TRACKING, 10000);

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

    // setup guiding
    prepareGuidingModule();

    //set the exposure time
    KTRY_SET_DOUBLESPINBOX_SUB(Ekos::Manager::Instance()->guideModule(), guideExposure, expTime);

    // start guiding
    KTRY_GADGET_SUB(Ekos::Manager::Instance()->guideModule(), QPushButton, guideB);
    // ensure that the guiding button is enabled (after MF it may take a while)
    KTRY_VERIFY_WITH_TIMEOUT_SUB(guideB->isEnabled(), 10000);
    expectedGuidingStates.enqueue(Ekos::GUIDE_GUIDING);
    KTRY_CLICK_SUB(Ekos::Manager::Instance()->guideModule(), guideB);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(expectedGuidingStates, 120000);
    qCInfo(KSTARS_EKOS_TEST) << "Guiding started.";
    qCInfo(KSTARS_EKOS_TEST) << "Guiding calibration: " << Options::serializedCalibration();
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
    // check if guiding could be stopped
    if (stopB->isEnabled() == false)
        return true;
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
    KTRY_SET_DOUBLESPINBOX_SUB(Ekos::Manager::Instance()->alignModule(), alignExposure, expTime);
    // reduce the accuracy to avoid testing problems
    KTRY_SET_SPINBOX_SUB(Ekos::Manager::Instance()->alignModule(), alignAccuracyThreshold, 300);
    // select the Luminance filter
    KTRY_SET_COMBO_SUB(Ekos::Manager::Instance()->alignModule(), alignFilter, "Luminance");

    // start alignment
    KTRY_GADGET_SUB(Ekos::Manager::Instance()->alignModule(), QPushButton, solveB);
    // ensure that the solve button is enabled (after MF it may take a while)
    KTRY_VERIFY_WITH_TIMEOUT_SUB(solveB->isEnabled(), 60000);
    KTRY_CLICK_SUB(Ekos::Manager::Instance()->alignModule(), solveB);
    // alignment started
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
    QWARN(QString("No astrometry index files found in %1").arg(
              KSUtils::getAstrometryDataDirs().join(", ")).toStdString().c_str());
    return false;
}

bool TestEkosHelper::isAstrometryAvailable()
{
    // avoid double checks if already found
    if (! m_astrometry_available)
        m_astrometry_available = checkAstrometryFiles();

    return m_astrometry_available;
}

bool TestEkosHelper::executeFocusing(int initialFocusPosition)
{
    // check whether focusing is already running
    if (! (getFocusStatus() == Ekos::FOCUS_IDLE || getFocusStatus() == Ekos::FOCUS_COMPLETE
            || getFocusStatus() == Ekos::FOCUS_ABORTED))
        return true;

    // prepare for focusing tests
    prepareFocusModule();

    initialFocusPosition = 40000;
    KTRY_SET_SPINBOX_SUB(Ekos::Manager::Instance()->focusModule()->mainFocuser().get(), absTicksSpin, initialFocusPosition);
    // start focusing
    KTRY_CLICK_SUB(Ekos::Manager::Instance()->focusModule()->mainFocuser().get(), startGotoB);
    // wait one second for settling
    QTest::qWait(3000);
    // start focusing
    expectedFocusStates.append(Ekos::FOCUS_COMPLETE);
    KTRY_CLICK_SUB(Ekos::Manager::Instance()->focusModule()->mainFocuser().get(), startFocusB);
    // wait for successful completion
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(expectedFocusStates, 180000);
    qCInfo(KSTARS_EKOS_TEST) << "Focusing finished.";
    // all checks succeeded
    return true;
}

bool TestEkosHelper::stopFocusing()
{
    // check whether focusing is already stopped
    if (getFocusStatus() == Ekos::FOCUS_IDLE || getFocusStatus() == Ekos::FOCUS_COMPLETE
            || getFocusStatus() == Ekos::FOCUS_ABORTED)
        return true;

    // switch to focus module
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->focusModule(), 1000));
    // stop focusing if necessary
    KTRY_GADGET_SUB(Ekos::Manager::Instance()->focusModule()->mainFocuser().get(), QPushButton, stopFocusB);
    if (stopFocusB->isEnabled())
        KTRY_CLICK_SUB(Ekos::Manager::Instance()->focusModule()->mainFocuser().get(), stopFocusB);
    KTRY_VERIFY_WITH_TIMEOUT_SUB(getFocusStatus() == Ekos::FOCUS_IDLE
                                 || getFocusStatus() == Ekos::FOCUS_COMPLETE ||
                                 getFocusStatus() == Ekos::FOCUS_ABORTED || getFocusStatus() == Ekos::FOCUS_FAILED, 15000);

    // all checks succeeded
    return true;
}

int TestEkosHelper::secondsToMF(QString message)
{
    QRegularExpression mfPattern("Meridian flip in (\\d+):(\\d+):(\\d+)");

    QRegularExpressionMatch match = mfPattern.match(message);
    if (match.hasMatch())
    {
        int hh  = match.captured(1).toInt();
        int mm  = match.captured(2).toInt();
        int sec = match.captured(3).toInt();
        if (hh >= 0)
            return (((hh * 60) + mm) * 60 + sec);
        else
            return (((hh * 60) - mm) * 60 - sec);
    }

    // unknown time
    return -1;

}

void TestEkosHelper::updateJ2000Coordinates(SkyPoint *target)
{
    SkyPoint J2000Coord(target->ra(), target->dec());
    J2000Coord.catalogueCoord(KStars::Instance()->data()->ut().djd());
    target->setRA0(J2000Coord.ra());
    target->setDec0(J2000Coord.dec());
}

void TestEkosHelper::init()
{
    // initialize the recorded states
    m_AlignStatus   = Ekos::ALIGN_IDLE;
    m_CaptureStatus = Ekos::CAPTURE_IDLE;
    m_FocusStatus   = Ekos::FOCUS_IDLE;
    m_GuideStatus   = Ekos::GUIDE_IDLE;
    m_MFStatus      = Ekos::MeridianFlipState::MOUNT_FLIP_NONE;
    m_DomeStatus    = ISD::Dome::DOME_IDLE;
    // initialize the event queues
    expectedAlignStates.clear();
    expectedCaptureStates.clear();
    expectedFocusStates.clear();
    expectedGuidingStates.clear();
    expectedMountStates.clear();
    expectedDomeStates.clear();
    expectedMeridianFlipStates.clear();
    expectedSchedulerStates.clear();


    // disable by default
    use_guiding = false;
    // reset dithering flag
    dithered = false;
    // disable dithering by default
    Options::setDitherEnabled(false);
    // clear the script map
    scripts.clear();
    // do not open INDI window on Ekos startup
    Options::setShowINDIwindowInitially(false);
}
void TestEkosHelper::cleanup()
{

}

/* *********************************************************************************
 *
 * Helper functions
 *
 * ********************************************************************************* */

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

bool TestEkosHelper::createCountingScript(Ekos::ScriptTypes scripttype, const QString scriptname)
{
    QString logfilename = scriptname;
    logfilename.replace(scriptname.lastIndexOf(".sh"), 3, ".log");
    QStringList script_content({"#!/bin/sh",
                                QString("nr=`head -1 %1|| echo 0` 2> /dev/null").arg(logfilename),
                                QString("nr=$(($nr+1))\necho $nr > %1").arg(logfilename),
                                QString("sleep 0.1")});
    // create executable script
    bool result = writeFile(scriptname, script_content,
                            QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    scripts.insert(scripttype, scriptname);
    // clear log file
    QFile logfile(logfilename);
    result |= logfile.remove();
    // ready
    return result;
}

bool TestEkosHelper::createAllCaptureScripts(QTemporaryDir *destination)
{
    KWRAP_SUB(QVERIFY2(createCountingScript(Ekos::SCRIPT_PRE_JOB,
                                            destination->path() + "/prejob.sh"), "Creating pre-job script failed!"));
    KWRAP_SUB(QVERIFY2(createCountingScript(Ekos::SCRIPT_PRE_CAPTURE,
                                            destination->path() + "/precapture.sh"), "Creating pre-capture script failed!"));
    KWRAP_SUB(QVERIFY2(createCountingScript(Ekos::SCRIPT_POST_CAPTURE,
                                            destination->path() + "/postcapture.sh"), "Creating post-capture script failed!"));
    KWRAP_SUB(QVERIFY2(createCountingScript(Ekos::SCRIPT_POST_JOB,
                                            destination->path() + "/postjob.sh"), "Creating post-job script failed!"));
    // everything succeeded
    return true;
}

int TestEkosHelper::countScriptRuns(Ekos::ScriptTypes scripttype)
{
    if (scripts.contains(scripttype))
    {
        QString scriptname = scripts[scripttype];
        QString logfilename = scriptname;
        logfilename.replace(scriptname.lastIndexOf(".sh"), 3, ".log");
        QFile logfile(logfilename);
        if (!logfile.open(QIODevice::ReadOnly | QIODevice::Text))
            return 0;
        QTextStream in(&logfile);
        QString countstr = in.readLine();
        logfile.close();
        return countstr.toInt();
    }
    else
        // no script found
        return -1;
}

bool TestEkosHelper::checkScriptRuns(int captures_per_sequence, int sequences)
{
    // check the log file if it holds the expected number
    int runs = countScriptRuns(Ekos::SCRIPT_PRE_JOB);
    KWRAP_SUB(QVERIFY2(runs == sequences,
                       QString("Pre-job script not executed as often as expected: %1 expected, %2 detected.")
                       .arg(captures_per_sequence).arg(runs).toLocal8Bit()));
    runs = countScriptRuns(Ekos::SCRIPT_PRE_CAPTURE);
    KWRAP_SUB( QVERIFY2(runs == sequences * captures_per_sequence,
                        QString("Pre-capture script not executed as often as expected: %1 expected, %2 detected.")
                        .arg(captures_per_sequence).arg(runs).toLocal8Bit()));
    runs = countScriptRuns(Ekos::SCRIPT_POST_JOB);
    KWRAP_SUB(QVERIFY2(runs == sequences,
                       QString("Post-job script not executed as often as expected: %1 expected, %2 detected.")
                       .arg(captures_per_sequence).arg(runs).toLocal8Bit()));
    runs = countScriptRuns(Ekos::SCRIPT_POST_CAPTURE);
    KWRAP_SUB(QVERIFY2(runs == sequences * captures_per_sequence,
                       QString("Post-capture script not executed as often as expected: %1 expected, %2 detected.")
                       .arg(captures_per_sequence).arg(runs).toLocal8Bit()));
    // everything succeeded
    return true;
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

void TestEkosHelper::mountStatusChanged(ISD::Mount::Status status)
{
    m_MountStatus = status;
    // check if the new state is the next one expected, then remove it from the stack
    if (!expectedMountStates.isEmpty() && expectedMountStates.head() == status)
        expectedMountStates.dequeue();
}

void TestEkosHelper::meridianFlipStatusChanged(Ekos::MeridianFlipState::MeridianFlipMountState status)
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
    // dithering detected?
    if (status == Ekos::GUIDE_DITHERING || status == Ekos::GUIDE_DITHERING_SUCCESS || status == Ekos::GUIDE_DITHERING_ERROR)
        dithered = true;

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

void TestEkosHelper::domeStatusChanged(ISD::Dome::Status status)
{
    m_DomeStatus = status;
    // check if the new state is the next one expected, then remove it from the stack
    if (!expectedDomeStates.isEmpty() && expectedDomeStates.head() == status)
        expectedDomeStates.dequeue();
}

bool TestEkosHelper::ensureSimulatorProfile(const QString &profileName, const QStringList &extraDevices)
{
    Ekos::Manager *manager = Ekos::Manager::Instance();
    if (!manager)
        return false;

    auto &profiles = const_cast<QList<QSharedPointer<ProfileInfo>>&>(manager->getAllProfiles());

    QSharedPointer<ProfileInfo> pi;
    for (auto &profile : profiles)
    {
        if (profile->name == profileName)
        {
            pi = profile;
            break;
        }
    }

    if (!pi)
    {
        int id = KStarsData::Instance()->userdb()->AddProfile(profileName);
        if (id < 0) return false;
        pi.reset(new ProfileInfo(id, profileName));
        profiles.append(pi);

        QComboBox* profileCBox = manager->findChild<QComboBox*>("profileCombo");
        if (profileCBox && profileCBox->findText(profileName) < 0)
        {
            profileCBox->addItem(profileName);
        }
    }

    pi->host = "localhost";
    pi->port = 7624;
    pi->autoConnect = true;

    pi->drivers.clear();

    auto addDrv = [&](const QString & label) -> bool
    {
        auto dInfo = DriverManager::Instance()->findDriverByLabel(label);
        if (!dInfo)
        {
            qWarning() << "Driver not found:" << label;
            return false;
        }
        pi->addDriver(dInfo->getType(), label);
        return true;
    };

    if (!addDrv("Telescope Simulator")) return false;
    if (!addDrv("CCD Simulator")) return false;
    if (!addDrv("Focuser Simulator")) return false;
    if (!addDrv("Guide Simulator")) return false;

    for (const auto &dev : extraDevices)
    {
        if (!addDrv(dev)) return false;
    }

    return KStarsData::Instance()->userdb()->SaveProfile(pi);
}

bool TestEkosHelper::ensureRemoteProfile(const QString &profileName, const QString &host,
        int port, const QStringList &drivers)
{
    qCInfo(KSTARS_EKOS_TEST) << "Setting up remote Ekos profile" << profileName
                             << "->" << host << ":" << port;

    Ekos::Manager *ekos = Ekos::Manager::Instance();
    if (!ekos)
        return false;

    bool isDone = false;

    // Safety timer: reject the dialog if it hasn't been saved within 10 s
    QTimer closeDialog;
    closeDialog.setSingleShot(true);
    closeDialog.setInterval(10000);
    QObject::connect(&closeDialog, &QTimer::timeout, ekos, [&]()
    {
        ProfileEditor *profileEditor = ekos->findChild<ProfileEditor*>("profileEditorDialog");
        if (profileEditor)
        {
            profileEditor->reject();
            qWarning(KSTARS_EKOS_TEST) << "Remote profile setup timed out and was aborted.";
        }
    });

    QComboBox *profileCBox = ekos->findChild<QComboBox*>("profileCombo");
    if (!profileCBox)
        return false;

    bool profileExists = profileCBox->findText(profileName) >= 0;

    // Build the JSON for ProfileEditor::setSettings — one call fills every field:
    // name, mode (remote), host, port, auto_connect, port_selector, and the driver list.
    QJsonObject settings;
    settings["name"]          = profileName;
    settings["mode"]          = QStringLiteral("remote");
    settings["remote_host"]   = host;
    settings["remote_port"]   = QString::number(port);
    settings["auto_connect"]  = false;
    settings["port_selector"] = false;

    if (!drivers.isEmpty())
    {
        QJsonArray driverArray;
        for (const QString &drv : drivers)
            driverArray.append(drv);
        settings["drivers"] = driverArray;
    }

    // After the dialog opens (1 s delay), configure it via setSettings and click Save.
    QTimer::singleShot(1000, ekos, [&]()
    {
        ProfileEditor *profileEditor = ekos->findChild<ProfileEditor*>("profileEditorDialog");
        if (!profileEditor)
        {
            qWarning(KSTARS_EKOS_TEST) << "ProfileEditor dialog not found";
            return;
        }

        // Single call populates every visible field in the dialog
        profileEditor->setSettings(settings);

        QDialogButtonBox *buttons = profileEditor->findChild<QDialogButtonBox*>("dialogButtons");
        if (!buttons)
            return;
        auto *saveButton = buttons->button(QDialogButtonBox::Save);
        if (!saveButton)
            return;
        QTRY_VERIFY_WITH_TIMEOUT(saveButton->isEnabled(), 5000);
        QTest::mouseClick(saveButton, Qt::LeftButton);

        qCInfo(KSTARS_EKOS_TEST) << "Remote profile" << profileName << "saved.";
        isDone = true;
    });

    if (profileExists)
    {
        // Select the existing profile so we can edit it
        profileCBox->setCurrentIndex(profileCBox->findText(profileName));
        QPushButton *editB = ekos->findChild<QPushButton*>("editProfileB");
        if (!editB || !editB->isEnabled())
            return false;
        QTest::mouseClick(editB, Qt::LeftButton);
    }
    else
    {
        QPushButton *addB = ekos->findChild<QPushButton*>("addProfileB");
        if (!addB)
            return false;
        QTest::mouseClick(addB, Qt::LeftButton);
    }

    closeDialog.start();
    KTRY_VERIFY_WITH_TIMEOUT_SUB(isDone, 15000);
    closeDialog.stop();

    return isDone;
}

bool TestEkosHelper::setSimulatedWeather(bool alert, QSharedPointer<Ekos::Scheduler> &scheduler, int timeoutMs)
{
    int result = QProcess::execute(QString("indi_setprop"), {QString("-n"), QString("Weather Simulator.WEATHER_CONTROL.Weather=%1").arg(alert ? 1 : 0)});
    if (result != 0) return false;

    int result2 = QProcess::execute(QString("indi_setprop"), {QString("-s"), QString("Weather Simulator.WEATHER_REFRESH.REFRESH=On")});
    if (result2 != 0) return false;

    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs)
    {
        if (scheduler->moduleState()->weatherStatus() == (alert ? ISD::Weather::WEATHER_ALERT : ISD::Weather::WEATHER_OK))
        {
            return true;
        }
        QTest::qWait(100);
    }
    return false;
}
