/***************************************************************************
                          kstars.h  -  K Desktop Planetarium
                             -------------------
    begin                : Mon Jan 7 2002
    copyright            : (C) 2002 by Mark Hollomon
    email                : mhh@mindspring.com
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/**@class KSUtils
	*@short KStars utility functions
	*@author Mark Hollomon
	*@version 1.0
	*Static functions for various purposes.
	*The openDataFile() function searches the standard KDE directories 
	*for the data filename given as an argument.
	*(it is impossible to instantiate a KSUtils object; just use the static functions).
	*/

#ifndef KSTARS_KSUTILS_H__
#define KSTARS_KSUTILS_H__

#include <cstddef>
#include <Eigen/Core>
USING_PART_OF_NAMESPACE_EIGEN
#include <QPointF>
#include "dms.h"

class QFile;
class QString;

namespace KSUtils {
    /**Attempt to open the data file named filename, using the QFile object "file".
    	*First look in the standard KDE directories, then look in a local "data"
    	*subdirectory.  If the data file cannot be found or opened, display a warning
    	*messagebox.
    	*@short Open a data file.
    	*@param &file The QFile object to be opened
    	*@param filename The name of the data file.
    	*@returns bool Returns true if data file was opened successfully.
    	*@returns a reference to the opened file.
    	*/
    bool openDataFile( QFile &file, const QString &filename );

    /** Clamp value into range.
     *  @p x  value to clamp.
     *  @p min  minimal allowed value.
     *  @p max  maximum allowed value.
     */
    template<typename T>
    inline T clamp(T x, T min, T max) {
        if( x < min )
            return min;
        if( x > max )
            return max;
        return x;
    }

    /** Put angle into range. Period is equal to max-min.
     *
     *  @p x angle value
     *  @p min minimal angle
     *  @p max maximal angle
     */
    template<typename T>
    inline T reduceAngle(T x, T min, T max) {
        T delta = max - min;
        return x - delta*floor( (x-min)/delta );
    }

    /** Convert from spherical to cartesian coordiate system.
     *  Resulting vector have unit length
     */
    inline Vector3d fromSperical(dms longitude, dms latitude) {
        double sinL, sinB;
        double cosL, cosB;
        longitude.SinCos( sinL, cosL );
        latitude.SinCos(  sinB, cosB );
        return Vector3d(cosB*cosL, cosB*sinL, sinB);
    }

    /** Convert a vector to a point */
    inline QPointF vecToPoint(const Vector2f& vec) {
        return QPointF( vec[0], vec[1] );
    }

    /** Convert a point to a vector */
    inline Vector2f pointToVec(const QPointF& p) {
        return Vector2f(p.x(),p.y());
    }
}

#endif
