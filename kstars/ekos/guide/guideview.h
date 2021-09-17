/*  Ekos GuideView
    Child of FITSView with few additions necessary for Internal Guider

    SPDX-FileCopyrightText: 2020 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "fitsviewer/fitsview.h"

#include <QList>

class QPainter;

/**
 * The main change relative to fitsview is to add the capability of displaying
 * the 'neighbor guide stars' for the SEP Multi Star guide algorithm.
 */
class GuideView : public FITSView
{
        Q_OBJECT
    public:
        explicit GuideView(QWidget *parent = nullptr, FITSMode mode = FITS_NORMAL, FITSScale filter = FITS_NONE);

        // Calls the parent drawOverlay, then draws circles around the guide-star
        // neighbors and lines between the guide star and the neighbors.
        void drawOverlay(QPainter *, double scale) override;

        // Adds a neighbor at x,y. Set found to true if the neighbor was associated
        // with a detected star. Coordinates of the detected star are optional.
        void addGuideStarNeighbor(double targetX, double targetY, bool found,
                                  double detectedX = 0, double detectedY = 0,
                                  bool isGuideStar = false);

        // Remove all the neighbors.
        void clearNeighbors();

    protected:

    private:
        struct Neighbor
        {
            // x and y input-image coordinates for the guide star neighbor target position.
            double targetX;
            double targetY;

            // Was the neighbor at the above location was associated with a detected star.
            bool found;

            // x and y input-image coordinates for the guide star neighbor that was detected.
            double detectedX;
            double detectedY;

            bool isGuideStar;
        };

        void drawNeighbor(QPainter *painter, const Neighbor &neighbor);
        QList<Neighbor> neighbors;
    signals:
};
