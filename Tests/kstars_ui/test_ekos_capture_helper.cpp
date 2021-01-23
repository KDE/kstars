/*
    Helper class of KStars UI capture tests

    Copyright (C) 2020
    Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "test_ekos_capture_helper.h"

#include "test_ekos.h"

TestEkosCaptureHelper::TestEkosCaptureHelper() {}

void TestEkosCaptureHelper::initTestCase()
{
    // connect to the capture process to receive capture status changes
    connect(Ekos::Manager::Instance()->captureModule(), &Ekos::Capture::newStatus, this, &TestEkosCaptureHelper::captureStatusChanged,
            Qt::UniqueConnection);
}

void TestEkosCaptureHelper::cleanupTestCase()
{
    // disconnect to the capture process to receive capture status changes
    disconnect(Ekos::Manager::Instance()->captureModule(), &Ekos::Capture::newStatus, this, &TestEkosCaptureHelper::captureStatusChanged);
}

void TestEkosCaptureHelper::fillProfile(bool *isDone)
{
    qCInfo(KSTARS_EKOS_TEST) << "Fill profile: starting...";
    ProfileEditor* profileEditor = Ekos::Manager::Instance()->findChild<ProfileEditor*>("profileEditorDialog");

    // Select the mount device
    KTRY_PROFILEEDITOR_GADGET(QComboBox, mountCombo);
    setTreeviewCombo(mountCombo, m_MountDevice);
    qCInfo(KSTARS_EKOS_TEST) << "Fill profile: Mount selected.";

    // Selet the CCD device
    KTRY_PROFILEEDITOR_GADGET(QComboBox, ccdCombo);
    setTreeviewCombo(ccdCombo, m_CCDDevice);
    qCInfo(KSTARS_EKOS_TEST) << "Fill profile: CCD selected.";

    // Select the focuser device
    KTRY_PROFILEEDITOR_GADGET(QComboBox, focuserCombo);
    setTreeviewCombo(focuserCombo, m_FocuserDevice);
    qCInfo(KSTARS_EKOS_TEST) << "Fill profile: Focuser selected.";

    // Select the guider device
    KTRY_PROFILEEDITOR_GADGET(QComboBox, guiderCombo);
    setTreeviewCombo(guiderCombo, m_GuiderDevice);
    qCInfo(KSTARS_EKOS_TEST) << "Fill profile: Guider selected.";

    // wait a short time to make the setup visible
    QTest::qWait(1000);
    // Save the profile using the "Save" button
    QDialogButtonBox* buttons = profileEditor->findChild<QDialogButtonBox*>("dialogButtons");
    QVERIFY(nullptr != buttons);
    QTest::mouseClick(buttons->button(QDialogButtonBox::Save), Qt::LeftButton);

    qCInfo(KSTARS_EKOS_TEST) << "Fill profile: Selections saved.";

    *isDone = true;
}

void TestEkosCaptureHelper::createEkosProfile(QString name, bool isPHD2, bool *isDone)
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

bool TestEkosCaptureHelper::setupEkosProfile(QString name, bool isPHD2)
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


bool TestEkosCaptureHelper::startCapturing(bool checkCapturing)
{
    // switch to the capture module
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->captureModule(), 1000));

    // check if capture is in a stopped state
    KWRAP_SUB(QVERIFY(getCaptureStatus() == Ekos::CAPTURE_IDLE || getCaptureStatus() == Ekos::CAPTURE_ABORTED
                      || getCaptureStatus() == Ekos::CAPTURE_COMPLETE));

    // ensure at least one capture is started if requested
    if (checkCapturing)
        expectedCaptureStates.enqueue(Ekos::CAPTURE_CAPTURING);
    // press start
    KTRY_GADGET_SUB(Ekos::Manager::Instance()->captureModule(), QPushButton, startB);
    KTRY_CLICK_SUB(Ekos::Manager::Instance()->captureModule(), startB);

    // check if capturing has started
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(expectedCaptureStates, 5000);
    // all checks succeeded
    return true;
}

bool TestEkosCaptureHelper::stopCapturing()
{
    // check if capture is in a stopped state
    if (getCaptureStatus() == Ekos::CAPTURE_IDLE || getCaptureStatus() == Ekos::CAPTURE_ABORTED || getCaptureStatus() == Ekos::CAPTURE_COMPLETE)
        return true;

    // switch to the capture module
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->captureModule(), 1000));

    // else press stop
    expectedCaptureStates.enqueue(Ekos::CAPTURE_ABORTED);
    KTRY_GADGET_SUB(Ekos::Manager::Instance()->captureModule(), QPushButton, startB);
    KTRY_CLICK_SUB(Ekos::Manager::Instance()->captureModule(), startB);

    // check if capturing has stopped
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(expectedCaptureStates, 5000);
    // all checks succeeded
    return true;
}

/* *********************************************************************************
 *
 * Helper functions
 *
 * ********************************************************************************* */
void TestEkosCaptureHelper::setTreeviewCombo(QComboBox *combo, QString lookup)
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

/* *********************************************************************************
 *
 * Slots for catching state changes
 *
 * ********************************************************************************* */

void TestEkosCaptureHelper::captureStatusChanged(Ekos::CaptureState status) {
    m_CaptureStatus = status;
    // check if the new state is the next one expected, then remove it from the stack
    if (!expectedCaptureStates.isEmpty() && expectedCaptureStates.head() == status)
        expectedCaptureStates.dequeue();
}

