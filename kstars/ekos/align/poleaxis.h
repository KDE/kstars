/*  poleaxis.h  determines the mount polar axis position

    Copyright (C) 2020 Chris Rowland <chris.rowland@cherryfield.me.uk>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include <dms.h>
#include <skypoint.h>

/**
 *@class PoleAxis
 *@short PoleAxis class handles determining the mount Ha axis position given three positions taken with the same mount declination.
 *
 *@author Chris Rowland
 *@version 1.0
 */
class PoleAxis
{
    public:
        // Like QVector3D but double precision.
        class V3
        {
            public:
                V3(double x, double y, double z) : X(x), Y(y), Z(z) {};
                V3() : X(0.0), Y(0.0), Z(0.0) {};
                double x() const
                {
                    return X;
                }
                double y() const
                {
                    return Y;
                }
                double z() const
                {
                    return Z;
                }
                static V3 normal(const V3 &v1, const V3 &v2, const V3 &v3);
                double length();
            private:
                double X, Y, Z;
        };

        ///
        /// \brief dirCos converts primary and secondary angles to a directional cosine
        /// \param primary angle, can be Ra, Ha, Azimuth or the corresponding axis values
        /// \param secondary angle, can be Dec, Altitude.  90 deg is the pole
        /// \return V3 containing the directional cosine.
        static V3 dirCos(const dms primary, const dms secondary);

        ///
        /// \brief dirCos converts a SkyPoint to a directional cosine
        /// \param sp SkyPoint with the position
        /// \return  V3 containing the directional cosine.
        ///
        static V3 dirCos(const SkyPoint sp);

        ///
        /// \brief primary returns the primary dms value in the directional cosine
        /// \param dirCos
        /// \return primary angle, Ra, Ha, Azimuth etc.
        ///
        static dms primary(V3 dirCos);

        ///
        /// \brief secondary returns the secondary dms angle in the directional cosine
        /// \param dirCos
        /// \return
        ///
        static dms secondary(V3 dirCos);

        ///
        /// \brief skyPoint returns a skypoint derived from the directional cosine vector
        /// \param dc
        /// \return
        ///
        static SkyPoint skyPoint(V3 dc);

        ///
        /// \brief poleAxis returns the pole axis vector given three SkyPoints with the same mount declination
        /// \param p1
        /// \param p2
        /// \param p3
        /// \return vector giving the direction of the pole. The rotation between the three points determines which pole
        /// the other pole can be determined either by reversing the sign of the declination and adding 12 hrs to the Ha or
        /// by negating the vector
        ///
        static V3 poleAxis(SkyPoint p1, SkyPoint p2, SkyPoint p3);

};
