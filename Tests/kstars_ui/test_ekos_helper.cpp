/*
    Helper class of KStars UI tests

    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_ekos_helper.h"
#include "ksutils.h"

TestEkosHelper::TestEkosHelper() {
    m_MountDevice = "Telescope Simulator";
    m_CCDDevice   = "CCD Simulator";
}


void TestEkosHelper::createEkosProfile(QString name, bool isPHD2, bool *isDone)
{
    ProfileEditor* profileEditor = Ekos::Manager::Instance()->findChild<ProfileEditor*>("profileEditorDialog");

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

    KWRAP_SUB(QVERIFY(setupEkosProfile("Simulators", false)));
    // start the profile
    KTRY_EKOS_CLICK(processINDIB);
    // wait for the devices to come up
    QTest::qWait(10000);

    // Everything completed successfully
    return true;
}

bool TestEkosHelper::shutdownEkosProfile()
{
    qCInfo(KSTARS_EKOS_TEST) << "Stopping profile ...";
    KWRAP_SUB(KTRY_EKOS_STOP_SIMULATORS());
    qCInfo(KSTARS_EKOS_TEST) << "Stopping profile ... (done)";
    // Everything completed successfully
    return true;
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

/** Currently nothing to do */
void TestEkosHelper::init() {}
void TestEkosHelper::cleanup() {}

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

