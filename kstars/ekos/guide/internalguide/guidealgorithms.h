/*  Ekos guide algorithms
    Copyright (C) 2012 Andrew Stepanenko

    Modified by Jasem Mutlaq <mutlaqja@ikarustech.com> for KStars.

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "vect.h"
#include <QPointer>
#include <cstdint>
#include "fitsviewer/fitsdata.h"

class Edge;

// Traditional guiding functions for star detection.
class GuideAlgorithms : public QObject
{
    public:
        static QList<Edge*> detectStars(const QSharedPointer<FITSData> &imageData,
                                        const QRect &trackingBox);

        static Vector findLocalStarPosition(QSharedPointer<FITSData> &imageData,
                                            const int algorithmIndex,
                                            const int videoWidth,
                                            const int videoHeight,
                                            const QRect &trackingBox);
    private:
        template <typename T>
        static Vector findLocalStarPosition(QSharedPointer<FITSData> &imageData,
                                            const int algorithmIndex,
                                            const int videoWidth,
                                            const int videoHeight,
                                            const QRect &trackingBox);
};

