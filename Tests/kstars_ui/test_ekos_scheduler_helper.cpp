/*
    Helper class of KStars UI scheduler tests

    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "test_ekos_scheduler_helper.h"

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

QString TestEkosSchedulerHelper::getSchedulerFile(const SkyObject *targetObject, StartupCondition startupCondition, ScheduleSteps steps,
                                                  bool enforceTwilight, bool enforceArtificialHorizon, int minAltitude, QString fitsFile)
{
    QString target = QString("<?xml version=\"1.0\" encoding=\"UTF-8\"?><SchedulerList version='1.4'><Profile>Default</Profile>"
                             "<Job><Name>%1</Name><Priority>10</Priority><Coordinates><J2000RA>%2</J2000RA>"
                             "<J2000DE>%3</J2000DE></Coordinates><Rotation>0</Rotation>")
                     .arg(targetObject->name()).arg(targetObject->ra0().Hours()).arg(targetObject->dec0().Degrees());

    if (fitsFile != nullptr)
        target += QString("<FITS>%1</FITS>").arg(fitsFile);

    QString sequence = QString("<Sequence>%1</Sequence>");

    QString startupConditionStr;
    if (startupCondition.type == SchedulerJob::START_ASAP)
        startupConditionStr = QString("<Condition>ASAP</Condition>");
    else if (startupCondition.type == SchedulerJob::START_CULMINATION)
        startupConditionStr = QString("<Condition value='%1'>Culmination</Condition>").arg(startupCondition.culminationOffset);
    else if (startupCondition.type == SchedulerJob::START_AT)
        startupConditionStr = QString("<Condition value='%1'>At</Condition>").arg(
                                  startupCondition.atLocalDateTime.toString(Qt::ISODate));

    QString parameters = QString("<StartupCondition>%1</StartupCondition>"
                                 "<Constraints><Constraint value='%4'>MinimumAltitude</Constraint>%2%3"
                                 "</Constraints><CompletionCondition><Condition>Sequence</Condition></CompletionCondition>"
                                 "%5%6%7%8</Steps></Job>"
                                 "<ErrorHandlingStrategy value='1'><delay>0</delay></ErrorHandlingStrategy><StartupProcedure>"
                                 "<Procedure>UnparkMount</Procedure></StartupProcedure><ShutdownProcedure><Procedure>ParkMount</Procedure>"
                                 "</ShutdownProcedure></SchedulerList>")
            .arg(startupConditionStr).arg(enforceTwilight ? "<Constraint>EnforceTwilight</Constraint>" : "")
            .arg(enforceArtificialHorizon ? "<Constraint>EnforceArtificialHorizon</Constraint>" : "")
            .arg(minAltitude).arg(steps.track ? "<Steps><Step>Track</Step>" : "").arg(steps.focus ? "<Step>Focus</Step>" : "")
            .arg(steps.align ? "<Step>Align</Step>" : "").arg(steps.guide ? "<Step>Guide</Step>" : "");

    return (target + sequence + parameters);
}

// TODO: make a method that creates the below strings depending of a few
// scheduler paramters we want to vary.

bool TestEkosSchedulerHelper::writeSimpleSequenceFiles(const QString &eslContents, const QString &eslFile, const QString &esqContents,
                              const QString &esqFile)
{
    return writeFile(eslFile, eslContents.arg(QString("file:%1").arg(esqFile))) && writeFile(esqFile, esqContents);
}
