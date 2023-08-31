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
#include <ekos_capture_debug.h>

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
    QString jobTargetName = targetName;
    auto frameType = getFrameType(job->getFrameType());
    auto filterType = job->getCoreProperty(SequenceJob::SJ_Filter).toString();
    auto exposure    = job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble();
    const auto isDarkFlat = job->jobType() == SequenceJob::JOBTYPE_DARKFLAT;

    if (isDarkFlat)
        frameType = "DarkFlat";

    // Sanitize name
    QString tempTargetName = KSUtils::sanitize(targetName);

    // Because scheduler sets the target name in capture module
    // it would be the same as the raw prefix
    if (tempTargetName.isEmpty() == false && jobTargetName.isEmpty())
        jobTargetName = tempTargetName;

    // Make full prefix
    QString imagePrefix = jobTargetName;

    if (imagePrefix.isEmpty() == false)
        imagePrefix += '_';

    imagePrefix += frameType;

    if (isFilterEnabled(job->getCoreProperty(SequenceJob::SJ_PlaceholderFormat).toString()) && filterType.isEmpty() == false &&
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
    if (isExpEnabled(job->getCoreProperty(SequenceJob::SJ_PlaceholderFormat).toString()))
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

    QString signature = generateSequenceFilename(*job, jobTargetName, true, true, 1, ".fits", "", false, true);
    job->setCoreProperty(SequenceJob::SJ_Signature, signature);

    job->setCoreProperty(SequenceJob::SJ_RemoteFormatDirectory, directoryPostfix);
    job->setCoreProperty(SequenceJob::SJ_RemoteFormatFilename, directoryPostfix);
}

void PlaceholderPath::addJob(SequenceJob *job, const QString &targetName)
{
    auto frameType = job->getFrameType();
    auto frameTypeString = getFrameType(job->getFrameType());
    QString imagePrefix = KSUtils::sanitize(targetName);

    const auto isDarkFlat = job->jobType() == SequenceJob::JOBTYPE_DARKFLAT;
    if (isDarkFlat)
        frameTypeString = "DarkFlat";

    QString tempPrefix = constructPrefix(job, imagePrefix);

    job->setCoreProperty(SequenceJob::SJ_FullPrefix, tempPrefix);

    QString directoryPostfix;

    const auto filterName = job->getCoreProperty(SequenceJob::SJ_Filter).toString();

    /* FIXME: Refactor directoryPostfix assignment, whose code is duplicated in scheduler.cpp */
    if (targetName.isEmpty())
        directoryPostfix = QDir::separator() + frameTypeString;
    else
        directoryPostfix = QDir::separator() + imagePrefix + QDir::separator() + frameTypeString;


    if ((frameType == FRAME_LIGHT || frameType == FRAME_FLAT || frameType == FRAME_NONE || isDarkFlat)
            &&  filterName.isEmpty() == false)
        directoryPostfix += QDir::separator() + filterName;

    job->setCoreProperty(SequenceJob::SJ_RemoteFormatDirectory, directoryPostfix);
    job->setCoreProperty(SequenceJob::SJ_RemoteFormatFilename, directoryPostfix);
}

QString PlaceholderPath::constructPrefix(const SequenceJob *job, const QString &imagePrefix)
{
    CCDFrameType frameType = job->getFrameType();
    auto placeholderFormat = job->getCoreProperty(SequenceJob::SJ_PlaceholderFormat).toString();
    auto filter = job->getCoreProperty(SequenceJob::SJ_Filter).toString();

    double exposure = job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble();

    QString tempImagePrefix = imagePrefix;
    if (tempImagePrefix.isEmpty() == false)
        tempImagePrefix += '_';

    const auto isDarkFlat = job->jobType() == SequenceJob::JOBTYPE_DARKFLAT;

    tempImagePrefix += isDarkFlat ? "DarkFlat" : CCDFrameTypeNames[frameType];

    if (isFilterEnabled(placeholderFormat) && filter.isEmpty() == false &&
            (frameType == FRAME_LIGHT ||
             frameType == FRAME_FLAT ||
             frameType == FRAME_NONE ||
             isDarkFlat))
    {
        tempImagePrefix += '_';
        tempImagePrefix += filter;
    }
    if (isExpEnabled(placeholderFormat))
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
    if (isTsEnabled(placeholderFormat))
    {
        tempImagePrefix += SequenceJob::ISOMarker;
    }

    return tempImagePrefix;
}

QString PlaceholderPath::generateSequenceFilename(const SequenceJob &job,
        const QString &targetName,
        bool local,
        const bool batch_mode,
        const int nextSequenceID,
        const QString &extension,
        const QString &filename,
        const bool glob,
        const bool gettingSignature) const
{
    QMap<PathProperty, QVariant> pathPropertyMap;
    pathPropertyMap[PP_TARGETNAME]  = QVariant(targetName);
    pathPropertyMap[PP_FILTER]      = job.getCoreProperty(SequenceJob::SJ_Filter);
    pathPropertyMap[PP_DARKFLAT]    = job.jobType() == SequenceJob::JOBTYPE_DARKFLAT;
    pathPropertyMap[PP_DIRECTORY]   = job.getCoreProperty(local ? SequenceJob::SJ_LocalDirectory :
                                      SequenceJob::SJ_RemoteDirectory);
    pathPropertyMap[PP_FORMAT]      = job.getCoreProperty(SequenceJob::SJ_PlaceholderFormat);
    pathPropertyMap[PP_SUFFIX]      = job.getCoreProperty(SequenceJob::SJ_PlaceholderSuffix);
    pathPropertyMap[PP_EXPOSURE]    = job.getCoreProperty(SequenceJob::SJ_Exposure);
    pathPropertyMap[PP_FRAMETYPE]   = QVariant(job.getFrameType());
    pathPropertyMap[PP_TEMPERATURE] = QVariant(job.getTargetTemperature());
    pathPropertyMap[PP_GAIN]        = job.getCoreProperty(SequenceJob::SJ_Gain);
    pathPropertyMap[PP_OFFSET]      = job.getCoreProperty(SequenceJob::SJ_Offset);
    pathPropertyMap[PP_PIERSIDE]    = QVariant(job.getPierSide());

    return generateFilenameInternal(pathPropertyMap, local, batch_mode, nextSequenceID, extension, filename, glob, gettingSignature);
}

QString PlaceholderPath::generateOutputFilename(const bool local, const bool batch_mode, const int nextSequenceID, const QString &extension,
        const QString &filename, const bool glob, const bool gettingSignature) const
{
    return generateFilenameInternal(m_PathPropertyMap, local, batch_mode, nextSequenceID, extension, filename, glob, gettingSignature);
}

QString PlaceholderPath::generateFilenameInternal(const QMap<PathProperty, QVariant> &pathPropertyMap,
        const bool local,
        const bool batch_mode,
        const int nextSequenceID,
        const QString &extension,
        const QString &filename,
        const bool glob,
        const bool gettingSignature) const
{
    QString targetNameSanitized = KSUtils::sanitize(pathPropertyMap[PP_TARGETNAME].toString());
    int i = 0;

    const QString format = pathPropertyMap[PP_FORMAT].toString();
    const bool isDarkFlat = pathPropertyMap[PP_DARKFLAT].isValid() && pathPropertyMap[PP_DARKFLAT].toBool();
    const CCDFrameType frameType = static_cast<CCDFrameType>(pathPropertyMap[PP_FRAMETYPE].toUInt());
    QString tempFilename = filename;
    QString currentDir;
    if (batch_mode)
        currentDir = pathPropertyMap[PP_DIRECTORY].toString();
    else
        currentDir = QDir::toNativeSeparators(KSPaths::writableLocation(QStandardPaths::TempLocation) + "/kstars/");

    // ensure, that there is exactly one separator is between non empty directory and format
    if(!currentDir.isEmpty() && !format.isEmpty())
    {
        if(!currentDir.endsWith(QDir::separator()) && !format.startsWith(QDir::separator()))
            currentDir.append(QDir::separator());
        if(currentDir.endsWith(QDir::separator()) && format.startsWith(QDir::separator()))
            currentDir = currentDir.left(currentDir.length() - 1);
    }

    QString tempFormat = currentDir + format + "_%s" + QString::number(pathPropertyMap[PP_SUFFIX].toUInt());

#if defined(Q_OS_WIN)
    tempFormat.replace("\\", "/");
#endif
    QRegularExpressionMatch match;
    QRegularExpression
    // This is the original regex with %p & %d tags - disabled for now to simply
    // re("(?<replace>\\%(?<name>(filename|f|Datetime|D|Type|T|exposure|e|Filter|F|target|t|temperature|C|gain|G|offset|O|pierside|P|sequence|s|directory|d|path|p))(?<level>\\d+)?)(?<sep>[_/])?");
#if defined(Q_OS_WIN)
    re("(?<replace>\\%(?<name>(filename|f|Datetime|D|Type|T|exposure|e|Filter|F|target|t|temperature|C|gain|G|offset|O|pierside|P|sequence|s))(?<level>\\d+)?)(?<sep>[_\\\\])?");
#else
    re("(?<replace>\\%(?<name>(filename|f|Datetime|D|Type|T|exposure|e|Filter|F|target|t|temperature|C|gain|G|offset|O|pierside|P|sequence|s))(?<level>\\d+)?)(?<sep>[_/])?");
#endif

    while ((i = tempFormat.indexOf(re, i, &match)) != -1)
    {
        QString replacement = "";
        if ((match.captured("name") == "filename") || (match.captured("name") == "f"))
            replacement = m_seqFilename.baseName();
        else if ((match.captured("name") == "Datetime") || (match.captured("name") == "D"))
        {
            if (glob || gettingSignature)
            {
                if (local)
                    replacement = "\\d\\d\\d\\d-\\d\\d-\\d\\dT\\d\\d-\\d\\d-\\d\\d";
                else
                    replacement = "ISO8601";

            }
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
            double exposure = pathPropertyMap[PP_EXPOSURE].toDouble();
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
            QString filter = pathPropertyMap[PP_FILTER].toString();
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
            replacement = targetNameSanitized;
        }
        else if (((match.captured("name") == "temperature") || (match.captured("name") == "C"))
                 && pathPropertyMap[PP_TEMPERATURE].isValid())
        {
            replacement = QString::number(pathPropertyMap[PP_TEMPERATURE].toDouble(), 'd', 0) + QString("C");
        }
        else if (((match.captured("name") == "gain") || (match.captured("name") == "G"))
                 && pathPropertyMap[PP_GAIN].isValid())
        {
            replacement = QString::number(pathPropertyMap[PP_GAIN].toDouble(), 'd', 0);
        }
        else if (((match.captured("name") == "offset") || (match.captured("name") == "O"))
                 && pathPropertyMap[PP_OFFSET].isValid())
        {
            replacement = QString::number(pathPropertyMap[PP_OFFSET].toDouble(), 'd', 0);
        }
        else if (((match.captured("name") == "pierside") || (match.captured("name") == "P"))
                 && pathPropertyMap[PP_PIERSIDE].isValid())
        {
            switch (static_cast<ISD::Mount::PierSide>(pathPropertyMap[PP_PIERSIDE].toInt()))
            {
                case ISD::Mount::PIER_EAST:
                    replacement =  "East";
                    break;
                case ISD::Mount::PIER_WEST:
                    replacement =  "West";
                    break;
                case ISD::Mount::PIER_UNKNOWN:
                    replacement =  "Unknown";
                    break;
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

void PlaceholderPath::setGenerateFilenameSettings(const SequenceJob &job, const QString &targetName)
{
    setPathProperty(PP_TARGETNAME, QVariant(targetName));
    setPathProperty(PP_FRAMETYPE, QVariant(job.getFrameType()));
    setPathProperty(PP_FILTER, job.getCoreProperty(SequenceJob::SJ_Filter));
    setPathProperty(PP_EXPOSURE, job.getCoreProperty(SequenceJob::SJ_Exposure));
    setPathProperty(PP_DIRECTORY, job.getCoreProperty(SequenceJob::SJ_LocalDirectory));
    setPathProperty(PP_FORMAT, job.getCoreProperty(SequenceJob::SJ_PlaceholderFormat));
    setPathProperty(PP_SUFFIX, job.getCoreProperty(SequenceJob::SJ_PlaceholderSuffix));
    setPathProperty(PP_DARKFLAT, job.jobType() == SequenceJob::JOBTYPE_DARKFLAT);
}

QStringList PlaceholderPath::remainingPlaceholders(const QString &filename)
{
    QList<QString> placeholders = {};
    QRegularExpressionMatch match;
#if defined(Q_OS_WIN)
    QRegularExpression re("(?<replace>\\%(?<name>[a-zA-Z])(?<level>\\d+)?)(?<sep>[_\\\\])+");
#else
    QRegularExpression re("(?<replace>%(?<name>[a-zA-Z])(?<level>\\d+)?)(?<sep>[_/])+");
#endif
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
    QString path = generateSequenceFilename(job, targetName, true, true, 0, ".*", "", true);
    auto sanitizedPath = path;

    // This is needed for Windows as the regular expression confuses path search
    QString idRE = "(?<id>\\d+).*";
    QString datetimeRE = "\\d\\d\\d\\d-\\d\\d-\\d\\dT\\d\\d-\\d\\d-\\d\\d";
    sanitizedPath.replace(idRE, "{IDRE}");
    sanitizedPath.replace(datetimeRE, "{DATETIMERE}");

    // Now we can get a proper directory
    QFileInfo path_info(sanitizedPath);
    QDir dir(path_info.dir());

    // e.g. Light_R_(?<id>\\d+).*
    auto filename = path_info.fileName();

    // Next replace back the problematic regular expressions
    filename.replace("{IDRE}", idRE);
    filename.replace("{DATETIMERE}", datetimeRE);

    QStringList matchingFiles = dir.entryList(QDir::Files);
    QRegularExpressionMatch match;
    QRegularExpression re("^" + filename + "$");
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

// An "emergency" method--the code should not be overwriting files,
// however, if we've detected an overwrite, we generate a new filename
// by looking for numbers at its end (before its extension) and incrementing
// that number, checking to make sure the new filename with the incremented number doesn't exist.
QString PlaceholderPath::repairFilename(const QString &filename)
{
    QRegularExpression re("^(.*[^\\d])(\\d+)\\.(\\w+)$");

    auto match = re.match(filename);
    if (match.hasMatch())
    {
        QString prefix = match.captured(1);
        int number = match.captured(2).toInt();
        int numberLength = match.captured(2).size();
        QString extension = match.captured(3);
        QString candidate = QString("%1%2.%3").arg(prefix).arg(number + 1, numberLength, 10, QLatin1Char('0')).arg(extension);
        int maxIterations = 2000;
        while (QFile::exists(candidate))
        {
            number = number + 1;
            candidate = QString("%1%2.%3").arg(prefix).arg(number, numberLength, 10, QLatin1Char('0')).arg(extension);
            if (--maxIterations <= 0)
                return filename;
        }
        return candidate;
    }
    return filename;;
}

}

