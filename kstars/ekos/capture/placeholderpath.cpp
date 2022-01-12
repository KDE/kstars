/*
    SPDX-FileCopyrightText: 2021 Kwon-Young Choi <kwon-young.choi@hotmail.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "placeholderpath.h"

#include "ekos/scheduler/schedulerjob.h"
#include "sequencejob.h"
#include "Options.h"
#include "kspaths.h"

#include <QString>
#include <QStringList>

#include <cmath>

namespace Ekos
{

PlaceholderPath::PlaceholderPath(QString seqFilename):
    m_frameTypes(
{
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
    job->setCoreProperty(SequenceJob::SJ_TargetName, targetName);

    auto frameType = getFrameType(job->getFrameType());
    auto filterType = job->getCoreProperty(SequenceJob::SJ_Filter).toString();
    auto exposure    = job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble();
    auto rawPrefix = job->getCoreProperty(SequenceJob::SJ_RawPrefix).toString();
    auto filterEnabled = job->getCoreProperty(SequenceJob::SJ_FilterPrefixEnabled).toBool();
    auto expEnabled = job->getCoreProperty(SequenceJob::SJ_ExpPrefixEnabled).toBool();
    //auto tsEnabled = job->getCoreProperty(SequenceJob::SJ_TimeStampPrefixEnabled).toBool();
    const auto isDarkFlat = job->getCoreProperty(SequenceJob::SJ_DarkFlat).toBool();

    if (isDarkFlat)
        frameType = "DarkFlat";

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
            (job->getFrameType() == FRAME_LIGHT || job->getFrameType() == FRAME_FLAT || job->getFrameType() == FRAME_NONE || isDarkFlat))
    {
        imagePrefix += '_';

        imagePrefix += filterType;
    }

    // JM 2021.08.21 For flat frames with specific ADU, the exposure duration is only advisory
    // and the final exposure time would depend on how many seconds are needed to arrive at the
    // target ADU. Therefore we should add duration to the signature.
    //if (expEnabled && !(job->getFrameType() == FRAME_FLAT && job->getFlatFieldDuration() == DURATION_ADU))
    if (expEnabled)
    {
        imagePrefix += '_';

        double fractpart, intpart;
        fractpart = std::modf(exposure, &intpart);
        if (fractpart == 0)
        {
            imagePrefix += QString::number(exposure, 'd', 0) + QString("_secs");
        }
        else if (exposure >= 1e-3)
        {
            imagePrefix += QString::number(exposure, 'f', 3) + QString("_secs");
        }
        else
        {
            imagePrefix += QString::number(exposure, 'f', 6) + QString("_secs");
        }
    }

    job->setCoreProperty(SequenceJob::SJ_FullPrefix, imagePrefix);

    // Directory postfix
    QString directoryPostfix;

    /* FIXME: Refactor directoryPostfix assignment, whose code is duplicated in capture.cpp */
    if (targetName.isEmpty())
        directoryPostfix = QDir::separator() + frameType;
    else
        directoryPostfix = QDir::separator() + targetName + QDir::separator() + frameType;
    if ((job->getFrameType() == FRAME_LIGHT || job->getFrameType() == FRAME_FLAT || job->getFrameType() == FRAME_NONE || isDarkFlat)
            && filterType.isEmpty() == false)
        directoryPostfix += QDir::separator() + filterType;

    job->setCoreProperty(SequenceJob::SJ_DirectoryPostfix, directoryPostfix);

}

void PlaceholderPath::addJob(SequenceJob *job, QString targetName)
{
    job->setCoreProperty(SequenceJob::SJ_TargetName, targetName);

    auto frameType = job->getFrameType();
    auto frameTypeString = getFrameType(job->getFrameType());
    const auto rawPrefix = job->getCoreProperty(SequenceJob::SJ_RawPrefix).toString();
    QString imagePrefix = rawPrefix;

    // Override
    const auto isDarkFlat = job->getCoreProperty(SequenceJob::SJ_DarkFlat).toBool();
    if (isDarkFlat)
        frameTypeString = "DarkFlat";

    // JM 2019-11-26: In case there is no raw prefix set
    // BUT target name is set, we update the prefix to include
    // the target name, which is usually set by the scheduler.
    if (imagePrefix.isEmpty() && !targetName.isEmpty())
    {
        imagePrefix = targetName;
    }

    constructPrefix(job, imagePrefix);

    job->setCoreProperty(SequenceJob::SJ_FullPrefix, imagePrefix);

    QString directoryPostfix;

    const auto filterName = job->getCoreProperty(SequenceJob::SJ_Filter).toString();

    /* FIXME: Refactor directoryPostfix assignment, whose code is duplicated in scheduler.cpp */
    if (targetName.isEmpty())
        directoryPostfix = QDir::separator() + frameTypeString;
    else
        directoryPostfix = QDir::separator() + targetName + QDir::separator() + frameTypeString;


    if ((frameType == FRAME_LIGHT || frameType == FRAME_FLAT || frameType == FRAME_NONE || isDarkFlat)
            &&  filterName.isEmpty() == false)
        directoryPostfix += QDir::separator() + filterName;

    job->setCoreProperty(SequenceJob::SJ_DirectoryPostfix, directoryPostfix);
}

void PlaceholderPath::constructPrefix(SequenceJob *job, QString &imagePrefix)
{
    CCDFrameType frameType = job->getFrameType();
    auto filter = job->getCoreProperty(SequenceJob::SJ_Filter).toString();
    auto rawPrefix = job->getCoreProperty(SequenceJob::SJ_RawPrefix).toString();
    auto filterEnabled = job->getCoreProperty(SequenceJob::SJ_FilterPrefixEnabled).toBool();
    auto expEnabled = job->getCoreProperty(SequenceJob::SJ_ExpPrefixEnabled).toBool();
    auto tsEnabled = job->getCoreProperty(SequenceJob::SJ_TimeStampPrefixEnabled).toBool();

    double exposure = job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble();

    if (imagePrefix.isEmpty() == false)
        imagePrefix += '_';

    const auto isDarkFlat = job->getCoreProperty(SequenceJob::SJ_DarkFlat).toBool();

    imagePrefix += isDarkFlat ? "DarkFlat" : CCDFrameTypeNames[frameType];

    if (filterEnabled && filter.isEmpty() == false &&
            (frameType == FRAME_LIGHT ||
             frameType == FRAME_FLAT ||
             frameType == FRAME_NONE ||
             isDarkFlat))
    {
        imagePrefix += '_';
        imagePrefix += filter;
    }
    if (expEnabled)
    {
        imagePrefix += '_';

        double exposureValue = job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble();

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

void PlaceholderPath::generateFilenameOld(const QString &format, bool batch_mode, QString *filename,
        QString fitsDir, QString seqPrefix, int nextSequenceID
                                         )
{
    QString currentDir;
    if (batch_mode)
        currentDir = fitsDir.isEmpty() ? Options::fitsDir() : fitsDir;
    else
        currentDir = KSPaths::writableLocation(QStandardPaths::TempLocation) + "/kstars";

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
    QString format, SequenceJob &job, QString targetName, bool batch_mode, int nextSequenceID, const QString &extension,
    QString *filename) const
{
    auto filter = job.getCoreProperty(SequenceJob::SJ_Filter).toString();
    auto rawPrefix = job.getCoreProperty(SequenceJob::SJ_RawPrefix).toString();
    auto filterEnabled = job.getCoreProperty(SequenceJob::SJ_FilterPrefixEnabled).toBool();
    auto expEnabled = job.getCoreProperty(SequenceJob::SJ_ExpPrefixEnabled).toBool();
    auto tsEnabled = job.getCoreProperty(SequenceJob::SJ_TimeStampPrefixEnabled).toBool();
    auto darkFlat = job.getCoreProperty(SequenceJob::SJ_DarkFlat).toBool();


    generateFilename(format, rawPrefix, filterEnabled, expEnabled,
                     tsEnabled, darkFlat, filter, job.getFrameType(), job.getCoreProperty(SequenceJob::SJ_Exposure).toDouble(),
                     targetName, batch_mode, nextSequenceID, extension, filename);
}

void PlaceholderPath::generateFilename(QString format, bool tsEnabled, bool batch_mode,
                                       int nextSequenceID, const QString &extension, QString *filename) const
{
    generateFilename(format, m_RawPrefix, m_filterPrefixEnabled, m_expPrefixEnabled,
                     tsEnabled, m_DarkFlat, m_filter, m_frameType, m_exposure, m_targetName, batch_mode,
                     nextSequenceID, extension, filename);
}

void PlaceholderPath::generateFilename(
    QString format, QString rawFilePrefix, bool filterEnabled, bool exposureEnabled,
    bool tsEnabled, bool isDarkFlat, QString filter, CCDFrameType frameType, double exposure, QString targetName,
    bool batch_mode, int nextSequenceID,  const QString &extension, QString *filename) const
{
    targetName = targetName.replace( QRegularExpression("\\s|/|\\(|\\)|:|\\*|~|\"" ), "_" )
                 // Remove any two or more __
                 .replace( QRegularExpression("_{2,}"), "_")
                 // Remove any _ at the end
                 .replace( QRegularExpression("_$"), "");
    int i = 0;

    QString currentDir;
    if (batch_mode)
    {
        currentDir = m_seqFilename.path().isEmpty() ? Options::fitsDir() : currentDir;
    }
    else
    {
        currentDir = KSPaths::writableLocation(QStandardPaths::TempLocation) + "/kstars";
    }

    if (currentDir.endsWith('/') == true)
        currentDir.chop(1);

    if (!currentDir.isEmpty())
        format = currentDir + "/" + format.section("/", -1);

    QRegularExpressionMatch match;
    QRegularExpression re("(?<replace>\\%(?<name>[f,D,T,e,F,t,d,p,s])(?<level>\\d+)?)(?<sep>[_/])?");
    while ((i = format.indexOf(re, i, &match)) != -1)
    {
        QString replacement = "";
        if (match.captured("name") == "f")
        {
            replacement = m_seqFilename.baseName();
        }
        else if (match.captured("name") == "D")
        {
            if (tsEnabled)
            {
                replacement = QDateTime::currentDateTime().toString("yyyy-MM-ddThh-mm-ss");
            }
        }
        else if (match.captured("name") == "T")
        {
            if (isDarkFlat)
                replacement = "DarkFlat";
            else
                replacement = getFrameType(frameType);
        }
        else if (match.captured("name") == "e")
        {
            if (exposureEnabled)
            {
                double fractpart, intpart;
                fractpart = std::modf(exposure, &intpart);
                if (fractpart == 0)
                {
                    replacement = QString::number(exposure, 'd', 0) + QString("_secs");
                }
                else if (exposure >= 1e-3)
                {
                    replacement = QString::number(exposure, 'f', 3) + QString("_secs");
                }
                else
                {
                    replacement = QString::number(exposure, 'f', 6) + QString("_secs");
                }
            }
        }
        else if (match.captured("name") == "F")
        {
            if (format.indexOf("/", match.capturedStart()) == -1)
            {
                // in the basename part of the path
                if (filterEnabled && filter.isEmpty() == false
                        && (frameType == FRAME_LIGHT
                            || frameType == FRAME_FLAT
                            || frameType == FRAME_NONE
                            || m_DarkFlat))
                {
                    replacement = filter;
                }
            }
            else
            {
                // in the directory part of the path
                if (filter.isEmpty() == false
                        && (frameType == FRAME_LIGHT
                            || frameType == FRAME_FLAT
                            || frameType == FRAME_NONE
                            || m_DarkFlat))
                {
                    replacement = filter;
                }
            }
        }
        else if (match.captured("name") == "t")
        {
            if (format.indexOf("/", match.capturedStart()) != -1)
            {
                // in the directory part of the path
                replacement = targetName;
            }
            else
            {
                // in the basename part of the path
                replacement = rawFilePrefix;
                if (replacement.isEmpty() && !targetName.isEmpty())
                {
                    replacement = targetName;
                }
            }
        }
        else if (match.captured("name") == "d" || match.captured("name") == "p")
        {
            int level = 0;
            if (!match.captured("level").isEmpty())
            {
                level = match.captured("level").toInt() - 1;
            }
            QFileInfo dir = m_seqFilename;
            for (int j = 0; j < level; ++j)
            {
                dir = QFileInfo(dir.dir().path());
            }
            if (match.captured("name") == "d")
            {
                replacement = dir.dir().dirName();
            }
            else if (match.captured("name") == "p")
            {
                replacement = dir.path();
            }
        }
        else if (match.captured("name") == "s")
        {
            int level = 1;
            if (!match.captured("level").isEmpty())
            {
                level = match.captured("level").toInt();
            }
            replacement = QString("%1").arg(nextSequenceID, level, 10, QChar('0'));
        }
        else
        {
            qWarning() << "Unknown replacement string: " << match.captured("replace");
        }
        if (replacement.isEmpty())
        {
            format = format.replace(match.capturedStart(), match.capturedLength(), replacement);
        }
        else
        {
            format = format.replace(match.capturedStart("replace"), match.capturedLength("replace"), replacement);
        }
        i += replacement.length();
    }
    *filename = format + extension;
}

void PlaceholderPath::setGenerateFilenameSettings(const SequenceJob &job)
{
    m_frameType           = job.getFrameType();
    m_RawPrefix           = job.getCoreProperty(SequenceJob::SJ_RawPrefix).toString();
    m_filterPrefixEnabled = job.getCoreProperty(SequenceJob::SJ_FilterPrefixEnabled).toBool();
    m_expPrefixEnabled    = job.getCoreProperty(SequenceJob::SJ_ExpPrefixEnabled).toBool();
    m_filter              = job.getCoreProperty(SequenceJob::SJ_Filter).toString();
    m_exposure            = job.getCoreProperty(SequenceJob::SJ_Exposure).toDouble();
    m_targetName          = job.getCoreProperty(SequenceJob::SJ_TargetName).toString();
    m_DarkFlat            = job.getCoreProperty(SequenceJob::SJ_DarkFlat).toBool();
}

QStringList PlaceholderPath::remainingPlaceholders(QString filename)
{
    QList<QString> placeholders = {};
    QRegularExpressionMatch match;
    QRegularExpression re("(?<replace>\\%(?<name>[a-z])(?<level>\\d+)?)(?<sep>[_/])?");
    int i = 0;
    while ((i = filename.indexOf(re, i, &match)) != -1)
    {
        if (match.hasMatch())
        {
            placeholders.push_back(match.captured("replace"));
        }
        i += match.capturedLength("replace");
    }
    return placeholders;
}

}

