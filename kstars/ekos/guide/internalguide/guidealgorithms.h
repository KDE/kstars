/*  Ekos guide algorithms
    Copyright (C) 2012 Andrew Stepanenko

    Modified by Jasem Mutlaq for KStars.
    SPDX-FileCopyrightText: Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "vect.h"
#include <QPointer>
#include <cstdint>

class FITSData;
class Edge;

// Traditional guiding functions for star detection.
class GuideAlgorithms : public QObject
{
    public:
        static QList<Edge*> detectStars(const QSharedPointer<FITSData> &imageData,
                                        const QRect &trackingBox);

        static GuiderUtils::Vector findLocalStarPosition(QSharedPointer<FITSData> &imageData,
                const int algorithmIndex,
                const int videoWidth,
                const int videoHeight,
                const QRect &trackingBox);
    private:
        template <typename T>
        static GuiderUtils::Vector findLocalStarPosition(QSharedPointer<FITSData> &imageData,
                const int algorithmIndex,
                const int videoWidth,
                const int videoHeight,
                const QRect &trackingBox);
};

