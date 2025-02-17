/*
    Helper class of KStars UI scheduler tests

    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "test_ekos_scheduler_helper.h"
#include "skyobject.h"

TestEkosSchedulerHelper::TestEkosSchedulerHelper(): TestEkosHelper() {}

// Simple write-string-to-file utility.
bool TestEkosSchedulerHelper::writeFile(const QString &filename, const QString &contents)
{
    QFile qFile(filename);
    if (qFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream out(&qFile);
        out << contents;
        qFile.close();
        return true;
    }
    return false;
}

QString TestEkosSchedulerHelper::getSchedulerFile(const SkyObject *targetObject, const StartupCondition &startupCondition,
        const CompletionCondition &completionCondition,
        ScheduleSteps steps, bool enforceTwilight, bool enforceArtificialHorizon,
        int minAltitude, double maxMoonAltitude, QString fitsFile, ShutdownProcedure shutdownProcedure, int errorDelay)
{
    QString target = QString("<?xml version=\"1.0\" encoding=\"UTF-8\"?><SchedulerList version='1.4'><Profile>Default</Profile>"
                             "<Job><Name>%1</Name><Priority>10</Priority><Coordinates><J2000RA>%2</J2000RA>"
                             "<J2000DE>%3</J2000DE></Coordinates><Rotation>0</Rotation>")
                     .arg(targetObject->name()).arg(targetObject->ra0().Hours()).arg(targetObject->dec0().Degrees());

    if (fitsFile != nullptr)
        target += QString("<FITS>%1</FITS>").arg(fitsFile);

    QString sequence = QString("<Sequence>%1</Sequence>");

    QString startupConditionStr;
    if (startupCondition.type == Ekos::START_ASAP)
        startupConditionStr = QString("<Condition>ASAP</Condition>");
    else if (startupCondition.type == Ekos::START_AT)
        startupConditionStr = QString("<Condition value='%1'>At</Condition>").arg(
                                  startupCondition.atLocalDateTime.toString(Qt::ISODate));

    QString completionConditionStr;
    if (completionCondition.type == Ekos::FINISH_SEQUENCE)
        completionConditionStr = QString("<Condition>Sequence</Condition>");
    else if (completionCondition.type == Ekos::FINISH_REPEAT)
        completionConditionStr = QString("<Condition value='%1'>Repeat</Condition>").arg(completionCondition.repeat);
    else if (completionCondition.type == Ekos::FINISH_LOOP)
        completionConditionStr = QString("<Condition>Loop</Condition>");
    else if (completionCondition.type == Ekos::FINISH_AT)
        completionConditionStr = QString("<Condition value='%1'>At</Condition>").arg(
                                     completionCondition.atLocalDateTime.toString(Qt::ISODate));

    QString parameters = QString("<StartupCondition>%1</StartupCondition>"
                                 "<Constraints><Constraint value='%4'>MinimumAltitude</Constraint>%2%3%11"
                                 "</Constraints><CompletionCondition>%9</CompletionCondition>"
                                 "%5%6%7%8</Steps></Job>"
                                 "<ErrorHandlingStrategy value='1'><delay>%10</delay></ErrorHandlingStrategy><StartupProcedure>"
                                 "<Procedure>UnparkMount</Procedure></StartupProcedure>")
                         .arg(startupConditionStr)
                         .arg(enforceTwilight ? "<Constraint>EnforceTwilight</Constraint>" : "")
                         .arg(enforceArtificialHorizon ? "<Constraint>EnforceArtificialHorizon</Constraint>" : "")
                         .arg(minAltitude)
                         .arg(steps.track ? "<Steps><Step>Track</Step>" : "")
                         .arg(steps.focus ? "<Step>Focus</Step>" : "")
                         .arg(steps.align ? "<Step>Align</Step>" : "")
                         .arg(steps.guide ? "<Step>Guide</Step>" : "")
                         .arg(completionConditionStr).arg(errorDelay)
                         .arg(maxMoonAltitude < 90 ? QString("<Constraint value='%1'>MoonMaxAltitude</Constraint>").arg(maxMoonAltitude) : "");

    QString shutdown = QString("<ShutdownProcedure>%1%2%3%4</ShutdownProcedure></SchedulerList>")
                       .arg(shutdownProcedure.warm_ccd ? "<Procedure>WarmCCD</Procedure>" : "")
                       .arg(shutdownProcedure.close_cap ? "<Procedure>ParkCap</Procedure>" : "")
                       .arg(shutdownProcedure.park_mount ? "<Procedure>ParkMount</Procedure>" : "")
                       .arg(shutdownProcedure.park_dome ? "<Procedure>ParkDome</Procedure>" : "");

    return (target + sequence + parameters + shutdown);
}

QString TestEkosSchedulerHelper::getEsqContent(QVector<TestEkosSchedulerHelper::CaptureJob> jobs)
{
    QString result = QString("<?xml version=\"1.0\" encoding=\"UTF-8\"?><SequenceQueue version='2.1'><CCD>CCD Simulator</CCD>"
                             "<FilterWheel>CCD Simulator</FilterWheel><GuideDeviation enabled='false'>2</GuideDeviation>"
                             "<GuideStartDeviation enabled='false'>2</GuideStartDeviation><Autofocus enabled='false'>0</Autofocus>"
                             "<RefocusOnTemperatureDelta enabled='false'>1</RefocusOnTemperatureDelta>"
                             "<RefocusEveryN enabled='false'>60</RefocusEveryN>");

    for (QVector<CaptureJob>::iterator job_iter = jobs.begin(); job_iter !=  jobs.end(); job_iter++)
    {
        result += QString("<Job><Exposure>%1</Exposure><Binning><X>1</X><Y>1</Y>"
                          "</Binning><Frame><X>0</X><Y>0</Y><W>1280</W><H>1024</H></Frame><Temperature force='false'>0</Temperature>"
                          "<Filter>%3</Filter><Type>Light</Type><Prefix><RawPrefix></RawPrefix><FilterEnabled>1</FilterEnabled>"
                          "<ExpEnabled>0</ExpEnabled><TimeStampEnabled>0</TimeStampEnabled></Prefix>"
                          "<Count>%2</Count><Delay>0</Delay><FITSDirectory>%4</FITSDirectory><UploadMode>0</UploadMode>"
                          "<Encoding>FITS</Encoding><Properties></Properties><Calibration><FlatSource><Type>Manual</Type>"
                          "</FlatSource><FlatDuration><Type>ADU</Type><Value>15000</Value><Tolerance>1000</Tolerance></FlatDuration>"
                          "<PreMountPark>False</PreMountPark><PreDomePark>False</PreDomePark></Calibration></Job>")
                  .arg(job_iter->exposureTime).arg(job_iter->count).arg(job_iter->filterName).arg(job_iter->fitsDirectory);
    }

    result += "</SequenceQueue>";
    return result;
}

// TODO: make a method that creates the below strings depending of a few
// scheduler paramters we want to vary.

bool TestEkosSchedulerHelper::writeSimpleSequenceFiles(const QString &eslContents, const QString &eslFile,
        const QString &esqContents,
        const QString &esqFile)
{
    return writeFile(eslFile, eslContents.arg(QString("file:%1").arg(esqFile))) && writeFile(esqFile, esqContents);
}
