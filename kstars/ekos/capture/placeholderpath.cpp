/*
    SPDX-FileCopyrightText: 2021 Kwon-Young Choi <kwon-young.choi@hotmail.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "placeholderpath.h"

#include "sequencejob.h"
#include "kspaths.h"

#include <QString>
#include <QStringList>

#include <cmath>
#include <algorithm>

namespace Ekos
{

PlaceholderPath::PlaceholderPath(const QString &seqFilename):
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
    PlaceholderPath(QString())
{
}

PlaceholderPath::~PlaceholderPath()
{
}

QString PlaceholderPath::defaultFormat(bool useFilter, bool useExposure, bool useTimestamp)
{
    QString tempFormat = QDir::separator() + "%t" + QDir::separator() + "%T" + QDir::separator();
    if (useFilter)
        tempFormat.append("%F" + QDir::separator());
    tempFormat.append("%t_%T_");
    if (useFilter)
        tempFormat.append("%F_");
    if (useExposure)
        tempFormat.append("%e_");
    if (useTimestamp)
        tempFormat.append("%D");
    return tempFormat;
}

void PlaceholderPath::processJobInfo(SequenceJob *job, const QString &targetName)
{
    job->setCoreProperty(SequenceJob::SJ_TargetName, targetName);

    auto frameType = getFrameType(job->getFrameType());
    auto filterType = job->getCoreProperty(SequenceJob::SJ_Filter).toString();
    auto exposure    = job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble();
    auto jobTargetName = job->getCoreProperty(SequenceJob::SJ_TargetName).toString();
    auto filterEnabled = job->getCoreProperty(SequenceJob::SJ_FilterPrefixEnabled).toBool();
    auto expEnabled = job->getCoreProperty(SequenceJob::SJ_ExpPrefixEnabled).toBool();
    const auto isDarkFlat = job->getCoreProperty(SequenceJob::SJ_DarkFlat).toBool();

    if (isDarkFlat)
        frameType = "DarkFlat";

    // Sanitize name
    QString tempTargetName = targetName;
    tempTargetName.replace( QRegularExpression("\\s|/|\\(|\\)|:|\\*|~|\"" ), "_" )
    // Remove any two or more __
    .replace( QRegularExpression("_{2,}"), "_")
    // Remove any _ at the end
    .replace( QRegularExpression("_$"), "");

    // Because scheduler sets the target name in capture module
    // it would be the same as the raw prefix
    if (tempTargetName.isEmpty() == false && jobTargetName.isEmpty())
        jobTargetName = tempTargetName;

    // Make full prefix
    QString imagePrefix = jobTargetName;

    if (imagePrefix.isEmpty() == false)
        imagePrefix += '_';

    imagePrefix += frameType;

    if (filterEnabled && filterType.isEmpty() == false &&
            (job->getFrameType() == FRAME_LIGHT || job->getFrameType() == FRAME_FLAT || job->getFrameType() == FRAME_NONE
             || isDarkFlat))
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
    if (tempTargetName.isEmpty())
        directoryPostfix = QDir::separator() + frameType;
    else
        directoryPostfix = QDir::separator() + tempTargetName + QDir::separator() + frameType;
    if ((job->getFrameType() == FRAME_LIGHT || job->getFrameType() == FRAME_FLAT || job->getFrameType() == FRAME_NONE
            || isDarkFlat)
            && filterType.isEmpty() == false)
        directoryPostfix += QDir::separator() + filterType;

    job->setCoreProperty(SequenceJob::SJ_DirectoryPostfix, directoryPostfix);

    // handle the case where .esq files do not make usage of placeholder and placeholder suffix
    if (job->getCoreProperty(SequenceJob::SJ_UsingPlaceholders).toBool() == false)
    {
        QString tempFormat = defaultFormat(!job->getCoreProperty(SequenceJob::SJ_Filter).toString().isEmpty(),
                                           job->getCoreProperty(SequenceJob::SJ_ExpPrefixEnabled).toBool(),
                                           job->getCoreProperty(SequenceJob::SJ_TimeStampPrefixEnabled).toBool());
        job->setCoreProperty(SequenceJob::SJ_PlaceholderFormat, tempFormat);
        job->setCoreProperty(SequenceJob::SJ_PlaceholderSuffix, 3);
        // converted, from now on we are in the new file format
        job->setCoreProperty(SequenceJob::SJ_UsingPlaceholders, true);
    }

    QString signature = generateFilename(*job, job->getCoreProperty(SequenceJob::SJ_TargetName).toString(), true, true,
                                         1, ".fits", "", false, true);
    job->setCoreProperty(SequenceJob::SJ_Signature, signature);
}

void PlaceholderPath::addJob(SequenceJob *job, const QString &targetName)
{
    job->setCoreProperty(SequenceJob::SJ_TargetName, targetName);

    auto frameType = job->getFrameType();
    auto frameTypeString = getFrameType(job->getFrameType());
    const auto jobTargetName = job->getCoreProperty(SequenceJob::SJ_TargetName).toString();
    QString imagePrefix = jobTargetName;

    const auto isDarkFlat = job->getCoreProperty(SequenceJob::SJ_DarkFlat).toBool();
    if (isDarkFlat)
        frameTypeString = "DarkFlat";

    // JM 2019-11-26: In case there is no job target name set
    // BUT target name is set, we update the prefix to include
    // the target name, which is usually set by the scheduler.
    if (imagePrefix.isEmpty() && !targetName.isEmpty())
    {
        imagePrefix = targetName;
    }

    QString tempPrefix = constructPrefix(job, imagePrefix);

    job->setCoreProperty(SequenceJob::SJ_FullPrefix, tempPrefix);

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

QString PlaceholderPath::constructPrefix(const SequenceJob *job, const QString &imagePrefix)
{
    CCDFrameType frameType = job->getFrameType();
    auto filter = job->getCoreProperty(SequenceJob::SJ_Filter).toString();
    auto filterEnabled = job->getCoreProperty(SequenceJob::SJ_FilterPrefixEnabled).toBool();
    auto expEnabled = job->getCoreProperty(SequenceJob::SJ_ExpPrefixEnabled).toBool();
    auto tsEnabled = job->getCoreProperty(SequenceJob::SJ_TimeStampPrefixEnabled).toBool();

    double exposure = job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble();

    QString tempImagePrefix = imagePrefix;
    if (tempImagePrefix.isEmpty() == false)
        tempImagePrefix += '_';

    const auto isDarkFlat = job->getCoreProperty(SequenceJob::SJ_DarkFlat).toBool();

    tempImagePrefix += isDarkFlat ? "DarkFlat" : CCDFrameTypeNames[frameType];

    if (filterEnabled && filter.isEmpty() == false &&
            (frameType == FRAME_LIGHT ||
             frameType == FRAME_FLAT ||
             frameType == FRAME_NONE ||
             isDarkFlat))
    {
        tempImagePrefix += '_';
        tempImagePrefix += filter;
    }
    if (expEnabled)
    {
        tempImagePrefix += '_';

        double exposureValue = job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble();

        // Don't use the locale for exposure value in the capture file name, so that we get a "." as decimal separator
        if (exposureValue == static_cast<int>(exposureValue))
            // Whole number
            tempImagePrefix += QString::number(exposure, 'd', 0) + QString("_secs");
        else
        {
            // Decimal
            if (exposure >= 0.001)
                tempImagePrefix += QString::number(exposure, 'f', 3) + QString("_secs");
            else
                tempImagePrefix += QString::number(exposure, 'f', 6) + QString("_secs");
        }
    }
    if (tsEnabled)
    {
        tempImagePrefix += SequenceJob::ISOMarker;
    }

    return tempImagePrefix;
}

QString PlaceholderPath::generateFilename(const SequenceJob &job,
        const QString &targetName,
        bool local,
        const bool batch_mode,
        const int nextSequenceID,
        const QString &extension,
        const QString &filename,
        const bool glob,
        const bool gettingSignature) const
{
    auto filter = job.getCoreProperty(SequenceJob::SJ_Filter).toString();
    auto darkFlat = job.getCoreProperty(SequenceJob::SJ_DarkFlat).toBool();

    QString directory;
    if (local)
        directory = job.getCoreProperty(SequenceJob::SJ_LocalDirectory).toString();
    else
        directory = job.getCoreProperty(SequenceJob::SJ_RemoteDirectory).toString();
    QString format = job.getCoreProperty(SequenceJob::SJ_PlaceholderFormat).toString();
    uint formatSuffix = job.getCoreProperty(SequenceJob::SJ_PlaceholderSuffix).toUInt();

    return generateFilename(directory, format, formatSuffix, targetName, darkFlat, filter, job.getFrameType(),
                            job.getCoreProperty(SequenceJob::SJ_Exposure).toDouble(),
                            batch_mode, nextSequenceID, extension, filename, glob, gettingSignature);
}

QString PlaceholderPath::generateFilename(const bool batch_mode, const int nextSequenceID, const QString &extension,
        const QString &filename, const bool glob, const bool gettingSignature) const
{
    return generateFilename(m_Directory, m_format, m_formatSuffix, m_targetName, m_DarkFlat, m_filter, m_frameType, m_exposure,
                            batch_mode, nextSequenceID, extension, filename, glob, gettingSignature);
}

QString PlaceholderPath::generateFilename(const QString &directory,
        const QString &format,
        uint formatSuffix,
        const QString &targetName,
        const bool isDarkFlat,
        const QString &filter,
        const CCDFrameType &frameType,
        const double exposure,
        const bool batch_mode,
        const int nextSequenceID,
        const QString &extension,
        const QString &filename,
        const bool glob,
        const bool gettingSignature) const
{
    QString targetNameSanitized = targetName;
    targetNameSanitized.replace( QRegularExpression("\\s|/|\\(|\\)|:|\\*|~|\"" ), "_" )
    // Remove any two or more __
    .replace( QRegularExpression("_{2,}"), "_")
    // Remove any _ at the end
    .replace( QRegularExpression("_$"), "");
    int i = 0;

    QString tempFilename = filename;
    QString currentDir;
    if (batch_mode)
        currentDir = directory;
    else
        currentDir = KSPaths::writableLocation(QStandardPaths::TempLocation) + QDir::separator() + "kstars" + QDir::separator();

    QString tempFormat = currentDir + format + "_%s" + QString::number(formatSuffix);

    QRegularExpressionMatch match;
    QRegularExpression
    // This is the original regex with %p & %d tags - disabled for now to simply
    // re("(?<replace>\\%(?<name>(filename|f|Datetime|D|Type|T|exposure|e|Filter|F|target|t|sequence|s|directory|d|path|p))(?<level>\\d+)?)(?<sep>[_/])?");
    re("(?<replace>\\%(?<name>(filename|f|Datetime|D|Type|T|exposure|e|Filter|F|target|t|sequence|s))(?<level>\\d+)?)(?<sep>[_/])?");
    while ((i = tempFormat.indexOf(re, i, &match)) != -1)
    {
        QString replacement = "";
        if ((match.captured("name") == "filename") || (match.captured("name") == "f"))
            replacement = m_seqFilename.baseName();
        else if ((match.captured("name") == "Datetime") || (match.captured("name") == "D"))
        {
            if (glob || gettingSignature)
                replacement = "\\d\\d\\d\\d-\\d\\d-\\d\\dT\\d\\d-\\d\\d-\\d\\d";
            else
                replacement = QDateTime::currentDateTime().toString("yyyy-MM-ddThh-mm-ss");
        }
        else if ((match.captured("name") == "Type") || (match.captured("name") == "T"))
        {
            if (isDarkFlat)
                replacement = "DarkFlat";
            else
                replacement = getFrameType(frameType);
        }
        else if ((match.captured("name") == "exposure") || (match.captured("name") == "e"))
        {
            double fractpart, intpart;
            fractpart = std::modf(exposure, &intpart);
            if (fractpart == 0)
                replacement = QString::number(exposure, 'd', 0) + QString("_secs");
            else if (exposure >= 1e-3)
                replacement = QString::number(exposure, 'f', 3) + QString("_secs");
            else
                replacement = QString::number(exposure, 'f', 6) + QString("_secs");
        }
        else if ((match.captured("name") == "Filter") || (match.captured("name") == "F"))
        {
            if (filter.isEmpty() == false
                    && (frameType == FRAME_LIGHT
                        || frameType == FRAME_FLAT
                        || frameType == FRAME_NONE
                        || isDarkFlat))
            {
                replacement = filter;
            }
        }
        else if ((match.captured("name") == "target") || (match.captured("name") == "t"))
        {
            if (tempFormat.indexOf(QDir::separator(), match.capturedStart()) != -1)
            {
                // in the directory part of the path
                replacement = targetNameSanitized;
            }
            else
            {
                // in the basename part of the path
                replacement = targetNameSanitized;
            }
        }
        // Disable for now %d & %p tags to simplfy
        //        else if ((match.captured("name") == "directory") || (match.captured("name") == "d") ||
        //                 (match.captured("name") == "path") || (match.captured("name") == "p"))
        //        {
        //            int level = 0;
        //            if (!match.captured("level").isEmpty())
        //                level = match.captured("level").toInt() - 1;
        //            QFileInfo dir = m_seqFilename;
        //            for (int j = 0; j < level; ++j)
        //                dir = QFileInfo(dir.dir().path());
        //            if (match.captured("name") == "directory" || match.captured("name") == "d")
        //                replacement = dir.dir().dirName();
        //            else if (match.captured("name") == "path" || match.captured("name") == "p")
        //                replacement = dir.path();
        //        }
        else if ((match.captured("name") == "sequence") || (match.captured("name") == "s"))
        {
            if (glob)
                replacement = "(?<id>\\d+)";
            else
            {
                int level = 0;
                if (!match.captured("level").isEmpty())
                    level = match.captured("level").toInt();
                replacement = QString("%1").arg(nextSequenceID, level, 10, QChar('0'));
            }
        }
        else
            qWarning() << "Unknown replacement string: " << match.captured("replace");
        if (replacement.isEmpty())
            tempFormat = tempFormat.replace(match.capturedStart(), match.capturedLength(), replacement);
        else
            tempFormat = tempFormat.replace(match.capturedStart("replace"), match.capturedLength("replace"), replacement);
        i += replacement.length();
    }

    if (!gettingSignature)
        tempFilename = tempFormat + extension;
    else
        tempFilename = tempFormat.left(tempFormat.lastIndexOf("_"));

    return tempFilename;
}

void PlaceholderPath::setGenerateFilenameSettings(const SequenceJob &job)
{
    m_frameType           = job.getFrameType();
    m_filterPrefixEnabled = job.getCoreProperty(SequenceJob::SJ_FilterPrefixEnabled).toBool();
    m_expPrefixEnabled    = job.getCoreProperty(SequenceJob::SJ_ExpPrefixEnabled).toBool();
    m_filter              = job.getCoreProperty(SequenceJob::SJ_Filter).toString();
    m_exposure            = job.getCoreProperty(SequenceJob::SJ_Exposure).toDouble();
    m_targetName          = job.getCoreProperty(SequenceJob::SJ_TargetName).toString();
    m_Directory           = job.getCoreProperty(SequenceJob::SJ_LocalDirectory).toString();
    m_format              = job.getCoreProperty(SequenceJob::SJ_PlaceholderFormat).toString();
    m_formatSuffix        = job.getCoreProperty(SequenceJob::SJ_PlaceholderSuffix).toUInt();
    m_tsEnabled           = job.getCoreProperty(SequenceJob::SJ_TimeStampPrefixEnabled).toBool();
    m_DarkFlat            = job.getCoreProperty(SequenceJob::SJ_DarkFlat).toBool();
}

QStringList PlaceholderPath::remainingPlaceholders(const QString &filename)
{
    QList<QString> placeholders = {};
    QRegularExpressionMatch match;
    QRegularExpression re("(?<replace>\\%(?<name>[a-z])(?<level>\\d+)?)(?<sep>[_/])?");
    int i = 0;
    while ((i = filename.indexOf(re, i, &match)) != -1)
    {
        if (match.hasMatch())
            placeholders.push_back(match.captured("replace"));
        i += match.capturedLength("replace");
    }
    return placeholders;
}

QList<int> PlaceholderPath::getCompletedFileIds(const SequenceJob &job, const QString &targetName)
{
    QString path = generateFilename(job, targetName, true, true, 0, ".*", "", true);
    QFileInfo path_info(path);
    QDir dir(path_info.dir());
    QStringList matchingFiles = dir.entryList(QDir::Files);
    QRegularExpressionMatch match;
    QRegularExpression re("^" + path_info.fileName() + "$");
    QList<int> ids = {};
    for (auto &name : matchingFiles)
    {
        match = re.match(name);
        if (match.hasMatch())
            ids << match.captured("id").toInt();
    }

    return ids;
}

int PlaceholderPath::getCompletedFiles(const SequenceJob &job, const QString &targetName)
{
    return getCompletedFileIds(job, targetName).length();
}

int PlaceholderPath::checkSeqBoundary(const SequenceJob &job, const QString &targetName)
{
    auto ids = getCompletedFileIds(job, targetName);
    if (ids.length() > 0)
        return *std::max_element(ids.begin(), ids.end()) + 1;
    else
        return 1;
}

}

