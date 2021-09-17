/*
    SPDX-FileCopyrightText: 2020 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QList>
#include <QVector>
#include <QVector2D>

#include "fitsviewer/fitsdata.h"
#include "vect.h"

/*
 * This class is useful to track a guide star by noting its position relative to other stars.
 * It is intended to be resilient to translation, a bit of positional noise, and slight field rotation.
 * The class is initialized with a set of reference (x,y) positions from stars, where one is
 * designated a guide star. Then, given a set of new input star positions, it determines a mapping
 * of the new stars to the references. Some reference stars may not be in the new star group, and
 * there may be new stars that don't appear in the references. However, the guide star must appear
 * in both sets for this to be successful.
 */

class StarCorrespondence
{
    public:
        // Initializes with reference stars.
        // One of the stars with index guideStar is a special "guide star".
        StarCorrespondence(const QList<Edge> &stars, int guideStar);
        StarCorrespondence();
        ~StarCorrespondence() {}

        // If the default constructor was called, then initialize must be called with the
        // reference star positions and the index in stars of the guideStar.
        void initialize(const QList<Edge> &stars, int guideStar);

        // Clears the references.
        void reset();

        // Associate the input stars with the reference stars.
        // StarMap[i] will contain the index of a reference star that corresponds to the ith star.
        // Some input stars may have no reference (starMap->at(i) == -1), and some references may
        // not correspond to any input stars. There will be no star-reference mapping with
        // distance longer than maxDistance. If adapt is true, the input positions are used
        // to incrementally adapt the reference positions.
        GuiderUtils::Vector find(const QList<Edge> &stars, double maxDistance, QVector<int> *starMap, bool adapt = true,
                                 double minFraction = 0.5);

        // Returns the number of reference stars.
        int size() const
        {
            return references.size();
        }

        // Return a reference to the ith reference star. Caller's responsiblity
        // to make sure i >= 0 && i < references.size();
        // Recompute the reference coordinates as we may adapt them.
        Edge reference(int i) const
        {
            Edge star = references[i];
            star.x = references[guideStarIndex].x + guideStarOffsets[i].x;
            star.y = references[guideStarIndex].y + guideStarOffsets[i].y;
            return star;
        }
        QVector2D offset(int i) const
        {
            QVector2D offset;
            offset.setX(guideStarOffsets[i].x);
            offset.setY(guideStarOffsets[i].y);
            return offset;
        }

        int guideStar() const
        {
            return guideStarIndex;
        }

        void setAllowMissingGuideStar(bool value = true)
        {
            allowMissingGuideStar = value;
        }

        void setImageSize(int width, int height)
        {
            imageWidth = width;
            imageHeight = height;
        }

    private:
        // The Offsets structure is used to keep the positions of the reference stars
        // relative to the guide star.
        struct Offsets
        {
            double x;  // The x position of the star (in pixels), relative to the guide star.
            double y;  // The y position of the star (in pixels), relative to the guide star.

            Offsets(double x_, double y_) : x(x_), y(y_) {}
            Offsets() : x(0), y(0) {}  // RPi compiler required this constructor.
        };

        // Update the reference-star offsets given the new star positions.
        // The adaption is similar to a 25-sample moving average.
        void initializeAdaptation();
        void adaptOffsets(const QList<Edge> &stars, const QVector<int> &starMap);

        // Utility used by find. Useful for iterating when the guide star is missing.
        int findInternal(const QList<Edge> &stars, double maxDistance, QVector<int> *starMap,
                         int guideStarIndex, const QVector<Offsets> &offsets,
                         int *numFound, int *numNotFound, double minFraction) const;

        // Used to when guide star is missing. Creates offsets as if other stars were the guide star.
        void makeOffsets(const QVector<Offsets> &offsets, QVector<Offsets> *targetOffsets, int targetStar) const;

        // When the guide star is missing, but star correspondence was successful, use the positions
        // of the stars that were found to create a virtual guide star--inferring a guide star position
        // using the offsets from the (unfound) guide star to the stars that were found.
        // Offsets are offsets that were created for a new "substitude guide star".
        // StarMap is the map made for that substitude by findInternal().
        // Offset is the offset from the original guide star to that substitute guide star.
        GuiderUtils::Vector inventStarPosition(const QList<Edge> &stars, QVector<int> &starMap,
                                               QVector<Offsets> offsets, Offsets offset) const;

        // Finds the star closest to x,y. Returns the index in sortedStars.
        // sortedStars should be sorted in x, which allows for a speedup in search.
        int findClosestStar(double x, double y, const QList<Edge> sortedStars,
                            double maxDistance, double *distance) const;

        // The offsets of the reference stars relative to the guide star.
        QVector<Offsets> guideStarOffsets;

        QList<Edge> references;    // The original reference stars.
        int guideStarIndex;        // The index of the guide star in references.
        bool initialized = false;  // Set to true once the references and guideStar index has been set.

        // If this is true, it will attempt star correspondence even if the guide star is missing.
        bool allowMissingGuideStar { false };

        // IIR filter parameter used to adapt offsets.
        double alpha;

        // Setting imageWidth and height can speed up searching for stars in an image
        // by eliminating positions far outside the image.
        int imageWidth { 100000000 };
        int imageHeight { 100000000 };

        // A copy of the original reference offsets used so that the values don't move too far.
        QVector<Offsets> originalGuideStarOffsets;
};

