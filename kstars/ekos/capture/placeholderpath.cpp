/***************************************************************************
                     placeholderpath.cpp  -  KStars Ekos
                             -------------------
    begin                : Tue 19 Jan 2021 15:06:21 CDT
    copyright            : (c) 2021 by Kwon-Young Choi
    email                : kwon-young.choi@hotmail.fr
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "placeholderpath.h"

#include "ekos/scheduler/schedulerjob.h"
#include "sequencejob.h"
#include "Options.h"
#include "kspaths.h"

#include <QString>

#include <cmath>

namespace Ekos {

PlaceholderPath::PlaceholderPath(QString seqFilename):
    m_frameTypes({
            {FRAME_LIGHT, "Light"},
            {FRAME_DARK, "Dark"},
            {FRAME_BIAS, "Bias"},
            {FRAME_FLAT, "Flat"},
            {FRAME_NONE, ""},
    }),
    m_seqFilename(seqFilename)
{
}

PlaceholderPath::PlaceholderPath():
    PlaceholderPath("")
{
}

PlaceholderPath::~PlaceholderPath()
{
}

void PlaceholderPath::processJobInfo(SequenceJob *job, QString targetName)
{
    QString frameType = getFrameType(job->getFrameType());
    QString rawPrefix, filterType = job->getFilterName();
    double exposure    = job->getExposure();
    bool filterEnabled = false, expEnabled = false, tsEnabled = false;
    job->getPrefixSettings(rawPrefix, filterEnabled, expEnabled, tsEnabled);

    // Sanitize name
    //QString targetName = schedJob->getName();
    targetName = targetName.replace( QRegularExpression("\\s|/|\\(|\\)|:|\\*|~|\"" ), "_" )
                 // Remove any two or more __
                 .replace( QRegularExpression("_{2,}"), "_")
                 // Remove any _ at the end
                 .replace( QRegularExpression("_$"), "");

    // Because scheduler sets the target name in capture module
    // it would be the same as the raw prefix
    if (targetName.isEmpty() == false && rawPrefix.isEmpty())
        rawPrefix = targetName;

    // Make full prefix
    QString imagePrefix = rawPrefix;

    if (imagePrefix.isEmpty() == false)
        imagePrefix += '_';

    imagePrefix += frameType;

    if (filterEnabled && filterType.isEmpty() == false &&
            (job->getFrameType() == FRAME_LIGHT || job->getFrameType() == FRAME_FLAT || job->getFrameType() == FRAME_NONE))
    {
        imagePrefix += '_';

        imagePrefix += filterType;
    }

    if (expEnabled)
    {
        imagePrefix += '_';

        double fractpart, intpart;
        fractpart = std::modf(exposure, &intpart);
        if (fractpart == 0) {
            imagePrefix += QString::number(exposure, 'd', 0) + QString("_secs");
        } else if (exposure >= 1e-3) {
            imagePrefix += QString::number(exposure, 'f', 3) + QString("_secs");
        } else {
            imagePrefix += QString::number(exposure, 'f', 6) + QString("_secs");
        }
    }

    job->setFullPrefix(imagePrefix);

    // Directory postfix
    QString directoryPostfix;

    /* FIXME: Refactor directoryPostfix assignment, whose code is duplicated in capture.cpp */
    if (targetName.isEmpty())
        directoryPostfix = QLatin1String("/") + frameType;
    else
        directoryPostfix = QLatin1String("/") + targetName + QLatin1String("/") + frameType;
    if ((job->getFrameType() == FRAME_LIGHT || job->getFrameType() == FRAME_FLAT || job->getFrameType() == FRAME_NONE) && filterType.isEmpty() == false)
        directoryPostfix += QLatin1String("/") + filterType;

    job->setDirectoryPostfix(directoryPostfix);
}

void PlaceholderPath::addJob(SequenceJob *job, QString targetName)
{
    CCDFrameType frameType = job->getFrameType();
    QString frameTypeStr = getFrameType(frameType);
    QString imagePrefix;
    QString rawFilePrefix;
    bool filterEnabled, exposureEnabled, tsEnabled;
    job->getPrefixSettings(rawFilePrefix, filterEnabled, exposureEnabled, tsEnabled);

    imagePrefix = rawFilePrefix;

    // JM 2019-11-26: In case there is no raw prefix set
    // BUT target name is set, we update the prefix to include
    // the target name, which is usually set by the scheduler.
    if (imagePrefix.isEmpty() && !targetName.isEmpty())
    {
        imagePrefix = targetName;
    }

    constructPrefix(job, imagePrefix);

    job->setFullPrefix(imagePrefix);

    QString directoryPostfix;

    /* FIXME: Refactor directoryPostfix assignment, whose code is duplicated in scheduler.cpp */
    if (targetName.isEmpty())
        directoryPostfix = QLatin1String("/") + frameTypeStr;
    else
        directoryPostfix = QLatin1String("/") + targetName + QLatin1String("/") + frameTypeStr;
    if ((frameType == FRAME_LIGHT || frameType == FRAME_FLAT || frameType == FRAME_NONE) &&  job->getFilterName().isEmpty() == false)
        directoryPostfix += QLatin1String("/") + job->getFilterName();

    job->setDirectoryPostfix(directoryPostfix);
}

void PlaceholderPath::constructPrefix(SequenceJob *job, QString &imagePrefix)
{
    CCDFrameType frameType = job->getFrameType();
    QString filter = job->getFilterName();
    QString rawFilePrefix;
    bool filterEnabled, exposureEnabled, tsEnabled;
    job->getPrefixSettings(rawFilePrefix, filterEnabled, exposureEnabled, tsEnabled);
    double exposure = job->getExposure();

    if (imagePrefix.isEmpty() == false)
        imagePrefix += '_';

    imagePrefix += getFrameType(frameType);

    /*if (fileFilterS->isChecked() && captureFilterS->currentText().isEmpty() == false &&
            captureTypeS->currentText().compare("Bias", Qt::CaseInsensitive) &&
            captureTypeS->currentText().compare("Dark", Qt::CaseInsensitive))*/
    if (filterEnabled && filter.isEmpty() == false &&
            (frameType == FRAME_LIGHT || frameType == FRAME_FLAT || frameType == FRAME_NONE))
    {
        imagePrefix += '_';
        imagePrefix += filter;
    }
    if (exposureEnabled)
    {
        //if (imagePrefix.isEmpty() == false || frameTypeCheck->isChecked())
        imagePrefix += '_';

        double exposureValue = job->getExposure();

        // Don't use the locale for exposure value in the capture file name, so that we get a "." as decimal separator
        if (exposureValue == static_cast<int>(exposureValue))
            // Whole number
            imagePrefix += QString::number(exposure, 'd', 0) + QString("_secs");
        else
        {
            // Decimal
            if (exposure >= 0.001)
                imagePrefix += QString::number(exposure, 'f', 3) + QString("_secs");
            else
                imagePrefix += QString::number(exposure, 'f', 6) + QString("_secs");
        }
    }
    if (tsEnabled)
    {
        imagePrefix += SequenceJob::ISOMarker;
    }
}

void PlaceholderPath::generateFilename(
        const QString &format, bool batch_mode, QString *filename,
        QString fitsDir, QString seqPrefix, int nextSequenceID
        )
{
    QString currentDir;
    if (batch_mode)
        currentDir = fitsDir.isEmpty() ? Options::fitsDir() : fitsDir;
    else
        currentDir = KSPaths::writableLocation(QStandardPaths::TempLocation);

    /*
    if (QDir(currentDir).exists() == false)
        QDir().mkpath(currentDir);
    */

    if (currentDir.endsWith('/') == false)
        currentDir.append('/');

    // IS8601 contains colons but they are illegal under Windows OS, so replacing them with '-'
    // The timestamp is no longer ISO8601 but it should solve interoperality issues
    // between different OS hosts
    QString ts = QDateTime::currentDateTime().toString("yyyy-MM-ddThh-mm-ss");

    if (seqPrefix.contains("_ISO8601"))
    {
        QString finalPrefix = seqPrefix;
        finalPrefix.replace("ISO8601", ts);
        *filename = currentDir + finalPrefix +
                    QString("_%1%2").arg(QString().asprintf("%03d", nextSequenceID), format);
    }
    else
        *filename = currentDir + seqPrefix + (seqPrefix.isEmpty() ? "" : "_") +
                    QString("%1%2").arg(QString().asprintf("%03d", nextSequenceID), format);
}

void PlaceholderPath::generateFilename(
        QString format, SequenceJob &job, QString targetName, bool batch_mode, int nextSequenceID, QString *filename)
{
    QString filter = job.getFilterName();
    CCDFrameType frameType = job.getFrameType();

    targetName = targetName.replace( QRegularExpression("\\s|/|\\(|\\)|:|\\*|~|\"" ), "_" )
        // Remove any two or more __
        .replace( QRegularExpression("_{2,}"), "_")
        // Remove any _ at the end
        .replace( QRegularExpression("_$"), "");
    int i = 1;

    QRegularExpressionMatch match;
    QRegularExpression re("(?<replace>\\%(?<name>[f,D,T,e,F,t,d,p,s])(?<level>\\d+)?)(?<sep>[_/])?");
    while ((i = format.indexOf(re, i-1, &match)) != -1) {
        QString replacement = "";
        if (match.captured("name") == "f") {
            replacement = m_seqFilename.baseName();
        } else if (match.captured("name") == "D") {
            replacement = QDateTime::currentDateTime().toString("yyyy-MM-ddThh-mm-ss");
        } else if (match.captured("name") == "T") {
            replacement = getFrameType(frameType);
        } else if (match.captured("name") == "e") {
            double exposure = job.getExposure();
            double fractpart, intpart;
            fractpart = std::modf(exposure, &intpart);
            if (fractpart == 0) {
                replacement = QString::number(exposure, 'd', 0) + QString("_secs");
            } else if (exposure >= 1e-3) {
                replacement = QString::number(exposure, 'f', 3) + QString("_secs");
            } else {
                replacement = QString::number(exposure, 'f', 6) + QString("_secs");
            }
        } else if (match.captured("name") == "F") {
            replacement = job.getFilterName();
        } else if (match.captured("name") == "t") {
            replacement = targetName;
        } else if (match.captured("name") == "d" || match.captured("name") == "p") {
            int level = 0;
            if (!match.captured("level").isEmpty()) {
                level = match.captured("level").toInt() - 1;
            }
            QFileInfo dir = m_seqFilename;
            for (int j = 0; j < level; ++j) {
                dir = QFileInfo(dir.dir().path());
            }
            if (match.captured("name") == "d") {
                replacement = dir.dir().dirName();
            } else if (match.captured("name") == "p") {
                replacement = dir.path();
            }
        } else if (match.captured("name") == "s") {
            int level = 1;
            if (!match.captured("level").isEmpty()) {
                level = match.captured("level").toInt();
            }
            replacement = QString("%1").arg(nextSequenceID, level, 10, QChar('0'));
        } else {
            qWarning() << "Unknown replacement string: " << match.captured("replace");
        }
        if (replacement.isEmpty()) {
            format = format.replace(match.capturedStart(), match.capturedLength(), replacement);
        } else {
            format = format.replace(match.capturedStart("replace"), match.capturedLength("replace"), replacement);
        }
        ++i;
    }
    *filename = format + ".fits";
}

}
