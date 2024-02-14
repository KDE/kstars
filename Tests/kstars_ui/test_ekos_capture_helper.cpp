/*
    Helper class of KStars UI capture tests

    SPDX-FileCopyrightText: 2020 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_ekos_capture_helper.h"

#include "test_ekos.h"
#include "ekos/capture/scriptsmanager.h"
#include "ekos/capture/capture.h"
#include "ekos/scheduler/scheduler.h"
#include "ekos/scheduler/schedulerprocess.h"
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
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(expectedCaptureStates, 120000);
    // all checks succeeded
    return true;
}

bool TestEkosCaptureHelper::stopCapturing()
{
    // check if capture is in a stopped state
    if (getCaptureStatus() == Ekos::CAPTURE_IDLE || getCaptureStatus() == Ekos::CAPTURE_ABORTED
            || getCaptureStatus() == Ekos::CAPTURE_COMPLETE)
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

QString TestEkosCaptureHelper::calculateSignature(QString target, QString filter, QString fitsDirectory)
{
    if (target == "")
        return fitsDirectory + QDir::separator() + "Light" + QDir::separator() + filter + QDir::separator() + "Light_" + filter +
               "_";
    else
        return fitsDirectory + QDir::separator() + target + QDir::separator() + "Light" + QDir::separator() + filter +
               QDir::separator() + target + "_" + "Light_" + filter + "_";
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
    if (imageLocation == nullptr || imageLocation->exists() == false)
        imageLocation = new QDir(destination->path() + "/images");

    return imageLocation;
}

bool TestEkosCaptureHelper::fillCaptureSequences(QString target, QString sequence, double exptime, QString fitsDirectory,
        int delay, QString format)
{
    if (sequence == "")
        return true;

    for (QString value : sequence.split(","))
    {
        KVERIFY_SUB(value.indexOf(":") > -1);
        QString filter = value.left(value.indexOf(":"));
        int count      = value.right(value.length() - value.indexOf(":") - 1).toInt();
        KTRY_SET_LINEEDIT_SUB(Ekos::Manager::Instance()->captureModule(), placeholderFormatT, format);
        if (count > 0)
            KWRAP_SUB(KTRY_CAPTURE_ADD_LIGHT(exptime, count, delay, filter, target, fitsDirectory));
        // ensure that no old values are present
        Ekos::Manager::Instance()->captureModule()->setCapturedFramesMap(calculateSignature(target, filter, fitsDirectory), 0);
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
    Ekos::Manager::Instance()->schedulerModule()->process()->stop();
    QTest::qWait(5000);
    // remove jobs
    Ekos::Manager::Instance()->schedulerModule()->process()->removeAllJobs();
}

QStringList TestEkosCaptureHelper::getSimpleEsqContent(CaptureSettings settings, QVector<SimpleCaptureLightsJob> jobs,
        ESQVersion version)
{
    QStringList result = serializeGeneralSettings(settings, version);


    for (QVector<SimpleCaptureLightsJob>::iterator job_iter = jobs.begin(); job_iter !=  jobs.end(); job_iter++)
        result.append(serializeJob(*job_iter, version));

    result.append("</SequenceQueue>");
    return result;
}

QStringList TestEkosCaptureHelper::getSimpleEsqContent(CaptureSettings settings, QVector<SimpleCaptureCalibratingJob> jobs,
        ESQVersion version)
{
    QStringList result = serializeGeneralSettings(settings, version);


    for (QVector<SimpleCaptureCalibratingJob>::iterator job_iter = jobs.begin(); job_iter !=  jobs.end(); job_iter++)
        result.append(serializeJob(*job_iter, version));

    result.append("</SequenceQueue>");
    return result;
}

QStringList TestEkosCaptureHelper::serializeGeneralSettings(CaptureSettings settings, ESQVersion version)
{
    QStringList result({"<?xml version=\"1.0\" encoding=\"UTF-8\"?>",
                        QString("<SequenceQueue version='%1'><CCD>CCD Simulator</CCD>").arg(esqVersionNames[version]),
                        "<FilterWheel>CCD Simulator</FilterWheel>",
                        QString("<Observer>%1</Observer>").arg(settings.observer),
                        QString("<GuideDeviation enabled='%1'>%2</GuideDeviation>").arg(settings.guideDeviation.enabled ? "true" : "false").arg(settings.guideDeviation.value),
                        QString("<GuideStartDeviation enabled='%1'>%2</GuideStartDeviation>").arg(settings.startGuideDeviation.enabled ? "true" : "false").arg(settings.startGuideDeviation.value),
                        QString("<Autofocus enabled='%1'>%2</Autofocus>").arg(settings.inSequenceFocus.enabled ? "true" : "false").arg(settings.inSequenceFocus.value),
                        QString("<RefocusOnTemperatureDelta enabled='%1'>%2</RefocusOnTemperatureDelta>").arg(settings.autofocusOnTemperature.enabled ? "true" : "false").arg(settings.autofocusOnTemperature.value),
                        QString("<RefocusEveryN enabled='%1'>%2</RefocusEveryN>").arg(settings.refocusEveryN.enabled ? "true" : "false").arg(settings.refocusEveryN.value),
                        QString("<RefocusOnMeridianFlip enabled='%1'/>").arg(settings.refocusAfterMeridianFlip ? "true" : "false")});

    return result;
}

QStringList TestEkosCaptureHelper::serializeJob(const TestEkosCaptureHelper::SimpleCaptureLightsJob &job,
        ESQVersion version)
{
    QStringList result({"<Job>",
                        QString("<Exposure>%1</Exposure>").arg(job.exposureTime),
                        "<Format>Mono</Format>",
                        QString("<Encoding>%1</Encoding>").arg(job.encoding),
                        QString("<Binning><X>%1</X><Y>%2</Y></Binning>").arg(job.binX).arg(job.binY),
                        QString("<Frame><X>%1</X><Y>%2</Y><W>%3</W><H>%4</H></Frame>").arg(job.x).arg(job.y).arg(job.w).arg(job.h),
                        QString("<Temperature force='%2'>%1</Temperature>").arg(job.cameraTemperature.value).arg(job.cameraTemperature.enabled ? "true" : "false"),
                        QString("<Filter>%1</Filter>").arg(job.filterName),
                        QString("<Type>%1</Type>").arg(job.type),
                        QString("<Count>%1</Count>").arg(job.count),
                        QString("<Delay>%1</Delay>").arg(job.delayMS / 1000)});
    switch (version)
    {
        case ESQ_VERSION_2_4:
            result.append(QString("<Prefix><RawPrefix>%1</RawPrefix></Prefix>").arg(job.targetName));
        case ESQ_VERSION_2_5:
            // no target specified
            break;
        case ESQ_VERSION_2_6:
            result.append(QString("<TargetName>%1</TargetName>").arg(job.targetName));
            break;
    }

    result.append({QString("<PlaceholderFormat>%1</PlaceholderFormat>").arg(job.placeholderFormat),
                   QString("<PlaceholderSuffix>%1</PlaceholderSuffix>").arg(job.formatSuffix),
                   QString("<FITSDirectory>%1</FITSDirectory>").arg(job.fitsDirectory),
                   QString("<UploadMode>%1</UploadMode>").arg(job.uploadMode),
                   "<Properties />",
                   "<Calibration>"});
    switch (version)
    {
        case ESQ_VERSION_2_4:
        case ESQ_VERSION_2_5:
            result.append({"<FlatSource><Type>Manual</Type></FlatSource>",
                           "<FlatDuration><Type>ADU</Type><Value>15000</Value><Tolerance>1000</Tolerance></FlatDuration>",
                           "<PreMountPark>False</PreMountPark>",
                           "<PreDomePark>False</PreDomePark>",
                           "</Calibration>",
                           "</Job>"});
            break;
        case ESQ_VERSION_2_6:
            result.append({QString("<PreAction><Type>%1</Type></PreAction>").arg(0),
                           "<FlatDuration><Type>ADU</Type><Value>15000</Value><Tolerance>1000</Tolerance></FlatDuration>",
                           "</Calibration>",
                           "</Job>"});
            break;
    }
    return result;
}

QStringList TestEkosCaptureHelper::serializeJob(const SimpleCaptureCalibratingJob &job, ESQVersion version)
{
    QStringList result({"<Job>",
                        QString("<Exposure>%1</Exposure>").arg(job.exposureTime),
                        "<Format>Mono</Format>",
                        QString("<Encoding>%1</Encoding>").arg("FITS"),
                        QString("<Binning><X>%1</X><Y>%2</Y></Binning>").arg(2).arg(2),
                        QString("<Frame><X>%1</X><Y>%2</Y><W>%3</W><H>%4</H></Frame>").arg(0).arg(0).arg(1280).arg(1080),
                        QString("<Temperature force='%2'>%1</Temperature>").arg(-20).arg("true"),
                        QString("<Filter>%1</Filter>").arg("Luminance"),
                        QString("<Type>%1</Type>").arg(job.type),
                        QString("<Count>%1</Count>").arg(job.count),
                        QString("<Delay>%1</Delay>").arg(2),
                        QString("<PlaceholderFormat>%1</PlaceholderFormat>").arg("/%t/%T/%F/%t_%T_%F_%e_%D"),
                        QString("<PlaceholderSuffix>%1</PlaceholderSuffix>").arg(2),
                        QString("<FITSDirectory>%1</FITSDirectory>").arg("/home/pi"),
                        QString("<UploadMode>%1</UploadMode>").arg(0),
                        "<Properties />",
                        "<Calibration>"});


    switch (version)
    {
        case ESQ_VERSION_2_4:
        case ESQ_VERSION_2_5:
            if (job.preAction & ACTION_WALL)
            {
                result.append("<FlatSource><Type>Wall</Type>");
                result.append(QString("<Az>%1</Az>").arg(job.wall_az));
                result.append(QString("<Alt>%1</Alt>").arg(job.wall_alt));
                result.append(QString("</FlatSource>"));
            }
            else
                result.append("<FlatSource><Type>Manual</Type></FlatSource>");

            result.append({QString("<PreMountPark>%1</PreMountPark>").arg((job.preAction & ACTION_PARK_MOUNT) > 0 ? "True" : "False"),
                           QString("<PreDomePark>%1</PreDomePark>").arg((job.preAction & ACTION_PARK_DOME) > 0 ? "True" : "False")});
            break;

        case ESQ_VERSION_2_6:
            result.append(QString("<PreAction><Type>%1</Type>").arg(job.preAction));
            if (job.preAction & ACTION_WALL)
                result.append({QString("<Az>%1</Az>").arg(job.wall_az),
                               QString("<Alt>%1</Alt>").arg(job.wall_alt)});
            result.append("</PreAction>");
            break;
    }
    result.append("<FlatDuration dark='false'>");
    if (job.duration_manual)
        result.append("<Type>Manual</Type>");
    else if (job.duration_adu)
        result.append(QString("<Type>ADU</Type><Value>%1</Value><Tolerance>%2</Tolerance>").arg(job.adu).arg(job.tolerance));

    result.append({"</FlatDuration>",
                   "</Calibration>",
                   "</Job>"});

    return result;

}

