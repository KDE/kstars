/*
  Copyright (C) 2021, Hy Murveit

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

// Example usage:
//
// double az1 = ..., alt1 = ..., az2 = ..., alt2 = ...;
// GreatCircle gc(az1, alt1, az2, alt2);
// double az, alt;
// gc.waypoint(0.75, &az, &alt);
//
// az and alt will contain the coordinates for a waypoint 75% of the way
// between az1,alt1 and az2,alt2 along a great circle path.
// See https://en.wikipedia.org/wiki/Great-circle_navigation

#pragma once

#include <QObject>

/**
 * @brief A class to compute points along a great circle from one az/alt to another.
 * @author Hy Murveit
 * @version 1.0
 */
class GreatCircle
{
    public:
        /**
         * @brief Construct a GreatCircle object for a path between az1,alt1 to az2,alt2
         * @param az1  starting azimuth  value (degrees).
         * @param alt1 starting altitude value (degrees).
         * @param az2    ending azimuth  value (degrees).
         * @param alt2   ending altitude value (degrees).
         */
        GreatCircle(double az1, double alt1, double az2, double alt2);

        /**
         * @brief Return the azimuth and altitude for a waypoint
         * @param fraction the desired fraction of the total path
         * @param az the returned azimuth value (degrees)
         * @param alt the returned altitude value (degrees)
         */
        void waypoint(double fraction, double *az, double *alt);

    private:
        // These are values computed in the constructor needed by all waypoints.
        double sigma01, sigma02, lambda0;
        double cosAlpha0, sinAlpha0;
};
