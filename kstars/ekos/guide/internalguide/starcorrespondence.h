/*  Correspondence class.
    Copyright (C) 2020 Hy Murveit

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include <QObject>
#include <QList>
#include <QVector>
#include <QVector2D>

#include "fitsviewer/fitsdata.h"

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

        // Associate the input stars with the reference stars.
        // StarMap[i] will contain the index of a reference star that corresponds to the ith star.
        // Some input stars may have no reference (starMap->at(i) == -1), and some references may
        // not correspond to any input stars. There will be no star-reference mapping with
        // distance longer than maxDistance. If adapt is true, the input positions are used
        // to incrementally adapt the reference positions.
        void find(const QList<Edge> &stars, double maxDistance, QVector<int> *starMap, bool adapt = true);

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
            Edge star = references[guideStarIndex];
            star.x += guideStarOffsets[i].x;
            star.y += guideStarOffsets[i].y;
            return star;
        }
        QVector2D offset(int i) const
        {
            QVector2D offset;
            offset.setX(guideStarOffsets[i].x);
            offset.setY(guideStarOffsets[i].y);
            return offset;
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

        // The offsets of the reference stars relative to the guide star.
        QVector<Offsets> guideStarOffsets;


        QList<Edge> references;    // The original reference stars.
        int guideStarIndex;        // The index of the guide star in references.
        bool initialized = false;  // Set to true once the references and guideStar index has been set.

        // IIR filter parameter used to adapt offsets.
        double alpha;

        // A copy of the original reference offsets used so that the values don't move too far.
        QVector<Offsets> originalGuideStarOffsets;
};

