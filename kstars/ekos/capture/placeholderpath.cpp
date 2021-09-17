/*
    SPDX-FileCopyrightText: 2021 Kwon-Young Choi <kwon-young.choi@hotmail.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "placeholderpath.h"

#include "ekos/scheduler/schedulerjob.h"
#include "sequencejob.h"

#include <QString>

#include <cmath>

namespace Ekos
{

PlaceholderPath::PlaceholderPath() {}

PlaceholderPath::~PlaceholderPath()
{
}

void PlaceholderPath::processJobInfo(SequenceJob *job, QString targetName)
{
    QString frameType = CCDFrameTypeNames[job->getFrameType()];
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

    // JM 2021.08.21 For flat frames with specific ADU, the exposure duration is only advisory
    // and the final exposure time would depend on how many seconds are needed to arrive at the
    // target ADU. Therefore we should add duration to the signature.
    if (expEnabled && !(job->getFrameType() == FRAME_FLAT && job->getFlatFieldDuration() == DURATION_ADU))
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

    job->setFullPrefix(imagePrefix);

    // Directory postfix
    QString directoryPostfix;

    /* FIXME: Refactor directoryPostfix assignment, whose code is duplicated in capture.cpp */
    if (targetName.isEmpty())
        directoryPostfix = QLatin1String("/") + frameType;
    else
        directoryPostfix = QLatin1String("/") + targetName + QLatin1String("/") + frameType;
    if ((job->getFrameType() == FRAME_LIGHT || job->getFrameType() == FRAME_FLAT || job->getFrameType() == FRAME_NONE)
            && filterType.isEmpty() == false)
        directoryPostfix += QLatin1String("/") + filterType;

    job->setDirectoryPostfix(directoryPostfix);
}

void PlaceholderPath::addJob(SequenceJob *job, QString targetName)
{
    CCDFrameType frameType = job->getFrameType();
    QString frameTypeStr = CCDFrameTypeNames[frameType];
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
    if ((frameType == FRAME_LIGHT || frameType == FRAME_FLAT || frameType == FRAME_NONE)
            &&  job->getFilterName().isEmpty() == false)
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

    imagePrefix += CCDFrameTypeNames[frameType];

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

}
