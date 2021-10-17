/*
    SPDX-FileCopyrightText: 2021 Hy Murveit <hy-1@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QList>
#include "../fitsviewer/fitsstardetector.h"

namespace Ekos
{

// This class helps compare HFRs computed from two images. The issue is that the two images
// may have different stars detected, and thus when comparing HFRs one may have smaller
// stars than the other, and be at an advantage wrt HFR computation.
// If we only use stars that the two star lists have in common, then comparisons are more fair.

// FocusStars is a object with a set of stars: star x,y positions and the computed HFRs for each star.
// getHFR() computes an overall HFR value using all stars in the FocusStars object.
// FocusStars.commonHFR(focusStars2, hfr1, hfr2) computes HFRs only using stars in common
// with focusStars2.
// FocusStars.relativeHFR(focusStars2, givenHFRValue) returns an hfr which is a scaled
// version of givenHFRValue, scaled by the ratio of the commonHFR
// between the two focusStars lists.

class FocusStars
{
    public:
        // The list of stars is not modified after the constructor.
        // Stars whose positions are further apart than maxDistance (after
        // overall image translation) cannot be considered the same stars.
        FocusStars(const QList<Edge *> edges, double maDistance = 10.0);
        FocusStars(const QList<Edge> edges, double maxDistance = 10.0);
        FocusStars() {}

        // Returns the median HFR for all the stars in this object.
        // Uses all the stars in the object.
        double getHFR() const;

        // Computes the HFRs of this list of stars and focusStars2's list.
        // It just uses the common stars in both lists.
        bool commonHFR(const FocusStars &focusStars2, double *hfr, double *hfr2) const;

        // Given an HFR value for focusStars2, come up with an HFR value for this
        // object. The number it computes will be relatively correct (e.g. if this
        // FocusStars is more in focus than FocusStars2, then it will return a
        // lower HFR value than hfrForList2, and if it is worse, the hfr will be higher.
        // It will return something proportionally correct, however the number
        // returned likely won't be the standard HFR value computed for all stars.
        double relativeHFR(const FocusStars &focusStars2, double hfrForStars2) const;

        // Returns a reference to all the stars.
        const QList<Edge> &getStars() const
        {
            return stars;
        };

        // debug
        void printStars(const QString &label) const;

    private:

        QList<Edge> stars;

        // Cached HFR computed on all the stars.
        mutable double computedHFR = -1;

        // Default max distance to consider two star positions the same star.
        double maxDistance = 5;
        bool initialized = false;
};
}
