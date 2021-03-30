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

#include <QString>

#include <cmath>

namespace Ekos {

PlaceholderPath::PlaceholderPath()
{
}

PlaceholderPath::~PlaceholderPath()
{
}

void PlaceholderPath::processJobInfo(SequenceJob *job, QString targetName)
{
    const QMap<CCDFrameType, QString> frameTypes =
    {
        { FRAME_LIGHT, "Light" }, { FRAME_DARK, "Dark" }, { FRAME_BIAS, "Bias" }, { FRAME_FLAT, "Flat" }, {FRAME_NONE, ""},
    };

    QString frameType = "";
    if (frameTypes.contains(job->getFrameType())) {
        frameType = frameTypes[job->getFrameType()];
    } else {
        qWarning() << job->getFrameType() << " not in " << frameTypes.keys();
    }
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

}
