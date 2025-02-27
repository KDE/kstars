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

namespace
{
const QString PierSideStr = "(East|West|Unknown)";
}

namespace Ekos
{

QMap<CCDFrameType, QString> PlaceholderPath::m_frameTypes =
{
    {FRAME_LIGHT, "Light"},
    {FRAME_DARK, "Dark"},
    {FRAME_BIAS, "Bias"},
    {FRAME_FLAT, "Flat"},
    {FRAME_VIDEO, "Video"},
    {FRAME_NONE, ""},
};

PlaceholderPath::PlaceholderPath(const QString &seqFilename)
    : m_seqFilename(seqFilename)
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

void PlaceholderPath::processJobInfo(SequenceJob *job)
{
    QString jobTargetName = job->getCoreProperty(SequenceJob::SJ_TargetName).toString();
    auto frameType = getFrameType(job->getFrameType());
    auto filterType = job->getCoreProperty(SequenceJob::SJ_Filter).toString();
    auto exposure    = job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble();
    const auto isDarkFlat = job->jobType() == SequenceJob::JOBTYPE_DARKFLAT;

    if (isDarkFlat)
        frameType = "DarkFlat";

    // Sanitize name
    QString tempTargetName = KSUtils::sanitize(jobTargetName);

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

    QString signature = generateSequenceFilename(*job, true, true, 1, ".fits", "", false, true);
    job->setCoreProperty(SequenceJob::SJ_Signature, signature);
}

void PlaceholderPath::updateFullPrefix(const QSharedPointer <SequenceJob> &job, const QString &targetName)
{
    QString imagePrefix = KSUtils::sanitize(targetName);
    QString fullPrefix = constructPrefix(job, imagePrefix);

    job->setCoreProperty(SequenceJob::SJ_FullPrefix, fullPrefix);
}

QString PlaceholderPath::constructPrefix(const QSharedPointer<SequenceJob> &job, const QString &imagePrefix)
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
        bool local,
        const bool batch_mode,
        const int nextSequenceID,
        const QString &extension,
        const QString &filename,
        const bool glob,
        const bool gettingSignature)
{
    QMap<PathProperty, QVariant> pathPropertyMap;
    setGenerateFilenameSettings(job, pathPropertyMap, local, gettingSignature);

    return generateFilenameInternal(pathPropertyMap, local, batch_mode, nextSequenceID, extension, filename, glob,
                                    gettingSignature, job.isVideo());
}

QString PlaceholderPath::generateOutputFilename(const bool local, const bool batch_mode, const int nextSequenceID,
        const QString &extension,
        const QString &filename, const bool glob, const bool gettingSignature) const
{
    return generateFilenameInternal(m_PathPropertyMap, local, batch_mode, nextSequenceID, extension, filename, glob,
                                    gettingSignature);
}

QString PlaceholderPath::generateReplacement(const QMap<PathProperty, QVariant> &pathPropertyMap, PathProperty property,
        bool usePattern) const
{
    if (usePattern)
    {
        switch (propertyType(property))
        {
            case PP_TYPE_UINT:
            case PP_TYPE_DOUBLE:
                return "-?\\d+";
            case PP_TYPE_BOOL:
                return "(true|false)";
            case PP_TYPE_POINT:
                return "\\d+x\\d+";
            default:
                if (property == PP_PIERSIDE)
                    return PierSideStr;
                else
                    return "\\w+";
        }
    }
    else if (pathPropertyMap[property].isValid())
    {
        switch (propertyType(property))
        {
            case PP_TYPE_DOUBLE:
                return QString::number(pathPropertyMap[property].toDouble(), 'd', 0);
            case PP_TYPE_UINT:
                return QString::number(pathPropertyMap[property].toUInt());
            case PP_TYPE_POINT:
                return QString("%1x%2").arg(pathPropertyMap[PP_BIN].toPoint().x()).arg(pathPropertyMap[PP_BIN].toPoint().y());
            case PP_TYPE_STRING:
                if (property == PP_PIERSIDE)
                {
                    switch (static_cast<ISD::Mount::PierSide>(pathPropertyMap[property].toInt()))
                    {
                        case ISD::Mount::PIER_EAST:
                            return "East";
                        case ISD::Mount::PIER_WEST:
                            return "West";
                        default:
                            return "Unknown";
                    }
                }
                else
                    return pathPropertyMap[property].toString();
            default:
                return pathPropertyMap[property].toString();
        }
    }
    else
    {
        switch (propertyType(property))
        {
            case PP_TYPE_DOUBLE:
            case PP_TYPE_UINT:
                return "-1";
            case PP_TYPE_POINT:
                return "0x0";
            case PP_TYPE_BOOL:
                return "false";
            default:
                return "Unknown";
        }
    }
}

QString PlaceholderPath::generateFilenameInternal(const QMap<PathProperty, QVariant> &pathPropertyMap,
        const bool local,
        const bool batch_mode,
        const int nextSequenceID,
        const QString &extension,
        const QString &filename,
        const bool glob,
        const bool gettingSignature,
        const bool isVideo) const
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

    QString tempFormat = currentDir + format + (isVideo ? QString() : "_%s" + QString::number(
                             pathPropertyMap[PP_SUFFIX].toUInt()));

#if defined(Q_OS_WIN)
    tempFormat.replace("\\", "/");
#endif
    QRegularExpressionMatch match;
    QRegularExpression
#if defined(Q_OS_WIN)
    re("(?<replace>\\%(?<name>(filename|f|Datetime|D|Type|T|exposure|e|exp|E|Filter|F|target|t|temperature|C|bin|B|gain|G|offset|O|iso|I|pierside|P|sequence|s))(?<level>\\d+)?)(?<sep>[_\\\\])?");
#else
    re("(?<replace>\\%(?<name>(filename|f|Datetime|D|Type|T|exposure|e|exp|E|Filter|F|target|t|temperature|C|bin|B|gain|G|offset|O|iso|I|pierside|P|sequence|s))(?<level>\\d+)?)(?<sep>[_/])?");
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
        else if ((match.captured("name") == "exposure") || (match.captured("name") == "e") ||
                 (match.captured("name") == "exp") || (match.captured("name") == "E"))
        {
            double fractpart, intpart;
            double exposure = pathPropertyMap[PP_EXPOSURE].toDouble();
            fractpart = std::modf(exposure, &intpart);
            if (fractpart == 0)
                replacement = QString::number(exposure, 'd', 0);
            else if (exposure >= 1e-3)
                replacement = QString::number(exposure, 'f', 3);
            else
                replacement = QString::number(exposure, 'f', 6);
            // append _secs for placeholders "exposure" and "e"
            if ((match.captured("name") == "exposure") || (match.captured("name") == "e"))
                replacement += QString("_secs");
        }
        else if ((match.captured("name") == "Filter") || (match.captured("name") == "F"))
        {
            QString filter = pathPropertyMap[PP_FILTER].toString();
            if (filter.isEmpty() == false
                    && (frameType == FRAME_LIGHT
                        || frameType == FRAME_FLAT
                        || frameType == FRAME_VIDEO
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
        else if (((match.captured("name") == "temperature") || (match.captured("name") == "C")))
        {
            replacement = generateReplacement(pathPropertyMap, PP_TEMPERATURE,
                                              (glob || gettingSignature) && pathPropertyMap[PP_TEMPERATURE].isValid() == false);
        }
        else if (((match.captured("name") == "bin") || (match.captured("name") == "B")))
        {
            replacement = generateReplacement(pathPropertyMap, PP_BIN,
                                              (glob || gettingSignature) && pathPropertyMap[PP_BIN].isValid() == false);
        }
        else if (((match.captured("name") == "gain") || (match.captured("name") == "G")))
        {
            replacement = generateReplacement(pathPropertyMap, PP_GAIN,
                                              (glob || gettingSignature) && pathPropertyMap[PP_GAIN].isValid() == false);
        }
        else if (((match.captured("name") == "offset") || (match.captured("name") == "O")))
        {
            replacement = generateReplacement(pathPropertyMap, PP_OFFSET,
                                              (glob || gettingSignature) && pathPropertyMap[PP_OFFSET].isValid() == false);
        }
        else if (((match.captured("name") == "iso") || (match.captured("name") == "I"))
                 && pathPropertyMap[PP_ISO].isValid())
        {
            replacement = generateReplacement(pathPropertyMap, PP_ISO,
                                              (glob || gettingSignature) && pathPropertyMap[PP_ISO].isValid() == false);
        }
        else if (((match.captured("name") == "pierside") || (match.captured("name") == "P")))
        {
            replacement = generateReplacement(pathPropertyMap, PP_PIERSIDE, glob || gettingSignature);
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
            else if (local)
            {
                int level = 0;
                if (!match.captured("level").isEmpty())
                    level = match.captured("level").toInt();
                replacement = QString("%1").arg(nextSequenceID, level, 10, QChar('0'));
            }
            else
            {
                // fix string for remote, ID is set remotely
                replacement = isVideo ? "" : "XXX";
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

void PlaceholderPath::setGenerateFilenameSettings(const SequenceJob &job, QMap<PathProperty, QVariant> &pathPropertyMap,
        const bool local, const bool gettingSignature)
{
    setPathProperty(pathPropertyMap, PP_TARGETNAME, job.getCoreProperty(SequenceJob::SJ_TargetName));
    setPathProperty(pathPropertyMap, PP_FRAMETYPE, QVariant(job.getFrameType()));
    setPathProperty(pathPropertyMap, PP_FILTER, job.getCoreProperty(SequenceJob::SJ_Filter));
    setPathProperty(pathPropertyMap, PP_EXPOSURE, job.getCoreProperty(SequenceJob::SJ_Exposure));
    setPathProperty(pathPropertyMap, PP_DIRECTORY,
                    local ? job.getCoreProperty(SequenceJob::SJ_LocalDirectory) : job.getRemoteDirectory());
    setPathProperty(pathPropertyMap, PP_FORMAT, job.getCoreProperty(SequenceJob::SJ_PlaceholderFormat));
    setPathProperty(pathPropertyMap, PP_SUFFIX, job.getCoreProperty(SequenceJob::SJ_PlaceholderSuffix));
    setPathProperty(pathPropertyMap, PP_DARKFLAT, job.jobType() == SequenceJob::JOBTYPE_DARKFLAT);
    setPathProperty(pathPropertyMap, PP_BIN, job.getCoreProperty(SequenceJob::SJ_Binning));
    setPathProperty(pathPropertyMap, PP_PIERSIDE, QVariant(job.getPierSide()));
    setPathProperty(pathPropertyMap, PP_ISO, job.getCoreProperty(SequenceJob::SJ_ISO));

    // handle optional parameters
    if (job.getCoreProperty(SequenceJob::SJ_EnforceTemperature).toBool())
        setPathProperty(pathPropertyMap, PP_TEMPERATURE, QVariant(job.getTargetTemperature()));
    else if (job.currentTemperature() != Ekos::INVALID_VALUE && !gettingSignature)
        setPathProperty(pathPropertyMap, PP_TEMPERATURE, QVariant(job.currentTemperature()));
    else
        pathPropertyMap.remove(PP_TEMPERATURE);

    if (job.getCoreProperty(SequenceJob::SequenceJob::SJ_Gain).toInt() >= 0)
        setPathProperty(pathPropertyMap, PP_GAIN, job.getCoreProperty(SequenceJob::SJ_Gain));
    else if (job.currentGain() >= 0 && !gettingSignature)
        setPathProperty(pathPropertyMap, PP_GAIN, job.currentGain());
    else
        pathPropertyMap.remove(PP_GAIN);

    if (job.getCoreProperty(SequenceJob::SequenceJob::SJ_Offset).toInt() >= 0)
        setPathProperty(pathPropertyMap, PP_OFFSET, job.getCoreProperty(SequenceJob::SJ_Offset));
    else if (job.currentOffset() >= 0 && !gettingSignature)
        setPathProperty(pathPropertyMap, PP_OFFSET, job.currentOffset());
    else
        pathPropertyMap.remove(PP_OFFSET);
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

QList<int> PlaceholderPath::getCompletedFileIds(const SequenceJob &job)
{
    QString path = generateSequenceFilename(job, true, true, 0, ".*", "", true);
    auto sanitizedPath = path;

    // This is needed for Windows as the regular expression confuses path search
    QString idRE = "(?<id>\\d+).*";
    QString datetimeRE = "\\d\\d\\d\\d-\\d\\d-\\d\\dT\\d\\d-\\d\\d-\\d\\d";
    sanitizedPath.replace(idRE, "{IDRE}");
    sanitizedPath.replace(datetimeRE, "{DATETIMERE}");

    // Now we can get a proper directory
    QFileInfo path_info(sanitizedPath);
    QDir dir(path_info.dir());

    QString const sig_dir(path_info.dir().path());
    if (sig_dir.contains(PierSideStr))
    {
        QString side;
        switch (static_cast<ISD::Mount::PierSide>(job.getPierSide()))
        {
            case ISD::Mount::PIER_EAST:
                side = "East";
                break;
            case ISD::Mount::PIER_WEST:
                side = "West";
                break;
            default:
                side = "Unknown";
        }
        QString tempPath = sig_dir;
        tempPath.replace(PierSideStr, side);
        dir = QDir(tempPath);
    }

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

int PlaceholderPath::getCompletedFiles(const SequenceJob &job)
{
    return getCompletedFileIds(job).length();
}

int PlaceholderPath::getCompletedFiles(const QString &path)
{
    int seqFileCount = 0;
#ifdef Q_OS_WIN
    // Splitting directory and baseName in QFileInfo does not distinguish regular expression backslash from directory separator on Windows.
    // So do not use QFileInfo for the code that separates directory and basename for Windows.
    // Conditions for calling this function:
    // - Directory separators must always be "/".
    // - Directory separators must not contain backslash.
    QString sig_dir;
    QString sig_file;
    int index = path.lastIndexOf('/');
    if (0 <= index)
    {
        // found '/'. path has both dir and filename
        sig_dir = path.left(index);
        sig_file = path.mid(index + 1);
    } // not found '/'. path has only filename
    else
    {
        sig_file = path;
    }
    // remove extension
    index = sig_file.lastIndexOf('.');
    if (0 <= index)
    {
        // found '.', then remove extension
        sig_file = sig_file.left(index);
    }
    qCDebug(KSTARS_EKOS_CAPTURE) << "Scheduler::PlaceholderPath path:" << path << " sig_dir:" << sig_dir << " sig_file:" <<
                                 sig_file;
#else
    QFileInfo const path_info(path);
    QString const sig_dir(path_info.dir().path());
    QString const sig_file(path_info.completeBaseName());
#endif
    QRegularExpression re(sig_file);

    QDirIterator it(sig_dir, QDir::Files);

    if (sig_dir.contains(PierSideStr))
    {
        QString tempPath = sig_dir;
        tempPath.replace(PierSideStr, "East");
        QString newPath = tempPath + QDir::separator() + sig_file;
        int count = getCompletedFiles(newPath);
        tempPath = sig_dir;
        tempPath.replace(PierSideStr, "West");
        newPath = tempPath + QDir::separator() + sig_file;
        count += getCompletedFiles(newPath);
        tempPath = sig_dir;
        tempPath.replace(PierSideStr, "Unknown");
        newPath = tempPath + QDir::separator() + sig_file;
        count += getCompletedFiles(newPath);
        return count;
    }
    /* FIXME: this counts all files with prefix in the storage location, not just captures. DSS analysis files are counted in, for instance. */
    while (it.hasNext())
    {
        QString const fileName = QFileInfo(it.next()).completeBaseName();

        QRegularExpressionMatch match = re.match(fileName);
        if (match.hasMatch())
            seqFileCount++;
    }

    return seqFileCount;
}

int PlaceholderPath::checkSeqBoundary(const SequenceJob &job)
{
    auto ids = getCompletedFileIds(job);
    if (ids.length() > 0)
        return *std::max_element(ids.begin(), ids.end()) + 1;
    else
        return 1;
}

PlaceholderPath::PathPropertyType PlaceholderPath::propertyType(PathProperty property)
{
    switch (property)
    {
        case PP_FORMAT:
        case PP_DIRECTORY:
        case PP_TARGETNAME:
        case PP_FILTER:
        case PP_PIERSIDE:
            return PP_TYPE_STRING;

        case PP_DARKFLAT:
            return PP_TYPE_BOOL;

        case PP_SUFFIX:
        case PP_FRAMETYPE:
        case PP_ISO:
            return PP_TYPE_UINT;

        case PP_EXPOSURE:
        case PP_GAIN:
        case PP_OFFSET:
        case PP_TEMPERATURE:
            return PP_TYPE_DOUBLE;

        case PP_BIN:
            return PP_TYPE_POINT;

        default:
            return PP_TYPE_NONE;
    }
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

