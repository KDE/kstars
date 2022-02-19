/*
    Helper class of KStars UI capture tests

    SPDX-FileCopyrightText: 2020 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_ekos_capture_helper.h"

#include "test_ekos.h"
#include "ekos/capture/scriptsmanager.h"
#include "Options.h"

TestEkosCaptureHelper::TestEkosCaptureHelper(QString guider) : TestEkosHelper(guider) {}

void TestEkosCaptureHelper::init()
{
    TestEkosHelper::init();
    QStandardPaths::setTestModeEnabled(true);
    QFileInfo test_dir(QStandardPaths::writableLocation(QStandardPaths::DataLocation), "test");
    destination = new QTemporaryDir(test_dir.absolutePath());
    QVERIFY(destination->isValid());
    QVERIFY(destination->autoRemove());
}

void TestEkosCaptureHelper::cleanup()
{
    // remove destination directory
    if (destination != nullptr)
        destination->remove();
    delete destination;
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

QString TestEkosCaptureHelper::calculateSignature(QString target, QString filter)
{
    if (target == "")
        return getImageLocation()->path() + "/Light/" + filter + "/Light";
    else
        return getImageLocation()->path() + "/" + target + "/Light/" + filter + "/" + target + "_Light";
}

QStringList TestEkosCaptureHelper::searchFITS(const QDir &dir) const
{
    QStringList list = dir.entryList(QDir::Files);

    //foreach (auto &f, list)
    //    QWARN(QString(dir.path()+'/'+f).toStdString().c_str());

    foreach (auto &d, dir.entryList(QDir::NoDotAndDotDot | QDir::Dirs))
        list.append(searchFITS(QDir(dir.path() + '/' + d)));

    return list;
}

void TestEkosCaptureHelper::ensureCCDShutter(bool shutter)
{
    if (m_CCDDevice != "")
    {
        // ensure that we know that the CCD has a shutter
        QStringList shutterlessCCDs = Options::shutterlessCCDs();
        QStringList shutterfulCCDs  = Options::shutterfulCCDs();
        if (shutter)
        {
            // remove CCD device from shutterless CCDs
            shutterlessCCDs.removeAll(m_CCDDevice);
            Options::setShutterlessCCDs(shutterlessCCDs);
            // add CCD device if necessary to shutterful CCDs
            if (! shutterfulCCDs.contains(m_CCDDevice))
            {
                shutterfulCCDs.append(m_CCDDevice);
                Options::setShutterfulCCDs(shutterfulCCDs);
            }
        }
        else
        {
            // remove CCD device from shutterful CCDs
            shutterfulCCDs.removeAll(m_CCDDevice);
            Options::setShutterfulCCDs(shutterfulCCDs);
            // add CCD device if necessary to shutterless CCDs
            if (! shutterlessCCDs.contains(m_CCDDevice))
            {
                shutterlessCCDs.append(m_CCDDevice);
                Options::setShutterlessCCDs(shutterlessCCDs);
            }
        }

    }
}

QDir *TestEkosCaptureHelper::getImageLocation()
{
    if (imageLocation == nullptr || imageLocation->exists())
        imageLocation = new QDir(destination->path() + "/images");

    return imageLocation;
}

bool TestEkosCaptureHelper::fillCaptureSequences(QString target, QString sequence, double exptime, QString fitsDirectory)
{
    if (sequence == "")
        return true;

    for (QString value : sequence.split(","))
    {
        KVERIFY_SUB(value.indexOf(":") > -1);
        QString filter = value.left(value.indexOf(":"));
        int count      = value.right(value.length()-value.indexOf(":")-1).toInt();
        KTRY_SET_CHECKBOX_SUB(Ekos::Manager::Instance()->captureModule(), fileTimestampS, true);
        KTRY_SET_LINEEDIT_SUB(Ekos::Manager::Instance()->captureModule(), filePrefixT, target);
        if (count > 0)
            KWRAP_SUB(KTRY_CAPTURE_ADD_LIGHT(exptime, count, 0, filter, fitsDirectory));
        // ensure that no old values are present
        Ekos::Manager::Instance()->captureModule()->setCapturedFramesMap(calculateSignature(target, filter), 0);
    }

    return true;
}

bool TestEkosCaptureHelper::fillScriptManagerDialog(const QMap<Ekos::ScriptTypes, QString> &scripts)
{
    Ekos::ScriptsManager *manager = Ekos::Manager::Instance()->findChild<Ekos::ScriptsManager*>();
    // fail if manager not found
    KVERIFY2_SUB(manager != nullptr, "Cannot find script manager!");
    manager->setScripts(scripts);
    manager->done(QDialog::Accepted);
    return true;
}

void TestEkosCaptureHelper::cleanupScheduler()
{
    Ekos::Manager::Instance()->schedulerModule()->stop();
    QTest::qWait(5000);
    // remove jobs
    Ekos::Manager::Instance()->schedulerModule()->removeAllJobs();
}
