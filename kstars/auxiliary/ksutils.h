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

/** @class KSUtils
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
#include <QPointF>
#include "dms.h"

class QFile;
class QString;
class SkyPoint;
class SkyObject;
class StarObject;

namespace KSUtils {
    /** Attempt to open the data file named filename, using the QFile object "file".
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
    inline Eigen::Vector3d fromSperical(dms longitude, dms latitude) {
        double sinL, sinB;
        double cosL, cosB;
        longitude.SinCos( sinL, cosL );
        latitude.SinCos(  sinB, cosB );
        return Eigen::Vector3d(cosB*cosL, cosB*sinL, sinB);
    }

    /** Convert a vector to a point */
    inline QPointF vecToPoint(const Eigen::Vector2f& vec) {
        return QPointF( vec[0], vec[1] );
    }

    /** Convert a point to a vector */
    inline Eigen::Vector2f pointToVec(const QPointF& p) {
        return Eigen::Vector2f(p.x(),p.y());
    }

    /**
     *@short Create a URL to obtain a DSS image for a given SkyPoint
     *@note If SkyPoint is a DeepSkyObject, this method automatically
     *decides the image size required to fit the object.
     */
    QString getDSSURL( const SkyPoint * const p );

    /**
     *@short Create a URL to obtain a DSS image for a given RA, Dec
     *@param RA The J2000.0 Right Ascension of the point
     *@param Dec The J2000.0 Declination of the point
     *@param width The width of the image in arcminutes
     *@param height The height of the image in arcminutes
     *@param type The image type, either gif or fits.
     *@note This method resets height and width to fall within the range accepted by DSS
     */
    QString getDSSURL( const dms &ra, const dms &dec, float width = 0, float height = 0, const QString & type = "gif");

    /**
     *@short Return a string corresponding to an angle specifying direction
     *
     * The angle must measure direction from North, towards East. Both
     * the azimuth and position angle follow this convention, so this
     * method can be used to return a string corresponding to the
     * general heading of a given azimuth / position angle.
     *
     *@param angle angle as dms (measured from North, towards East)
     *@return A localized string corresponding to the approximate direction (eg: NNW)
     */
    QString toDirectionString( dms angle );
    /**
     *@short Converts StarObject list into SkyObject list
     *@param starObjList QList of StarObject pointers
     *@return Returns a pointer to QList of SkyObject pointers
     *@note Used for Star-Hopper
     */
    QList<SkyObject *> * castStarObjListToSkyObjList( QList<StarObject *> *starObjList );


    /**
     *@note Avoid using this method for the same reasons as QSharedPointer::data()
     */
    template <typename T> QList<T*> makeVanillaPointerList( const QList<QSharedPointer<T>> &spList ) {
        QList<T *> vpList;
        foreach( QSharedPointer<T> sp, spList )
            vpList.append( sp.data() );
        return vpList;
    }

    /**
     *@short Return the genetive form of constellation name, given the abbreviation
     *@param code Three-letter IAU abbreviation of the constellation
     *@return the genetive form of the constellation name
     */
    QString constGenetiveFromAbbrev( const QString &code );

    /**
     *@short Return the name of the constellation, given the abbreviation
     *@param code Three-letter IAU abbreviation of the constellation
     *@return the nominative form of the constellation name
     */
    QString constNameFromAbbrev( const QString &code );

    /**
     *@short Return the abbreviation of the constellation, given the full name
     *@param fullName_ Full name of the constellation
     *@return the three-letter IAU standard abbreviation of the constellation
     */
     QString constNameToAbbrev( const QString &fullName_ );

     /**
      *@short Return the abbreviation of the constellation, given the genetive form
      *@param genetive_ the genetive form of the constellation's name
      *@return the three-letter IAU standard abbreviation of the constellation
      */
     QString constGenetiveToAbbrev( const QString &genetive_ );


       /**
       * Interface into Qt's logging system
       * @Author: Yale Dedis 2011
       * Adapted from DeDiS project.
       */
      class Logging {
        public:
          /**
           * Store all logs into the specified file
           * @param filename the file in which to store logs
           */
          static void UseFile();

          /**
           * Output logs to stdout
           */
          static void UseStdout();

          /**
           * Output logs to stderr
           */
          static void UseStderr();

          /**
           * Use the default logging mechanism
           */
          static void UseDefault();

          /**
           * Disable logging
           */
          static void Disable();

        private:
          static QString _filename;

          static void Disabled(QtMsgType type, const QMessageLogContext &context,
              const QString &msg);
          static void File(QtMsgType type, const QMessageLogContext &context,
              const QString &msg);
          static void Stdout(QtMsgType type, const QMessageLogContext &context,
              const QString &msg);
          static void Stderr(QtMsgType type, const QMessageLogContext &context,
              const QString &msg);
          static void Write(QTextStream &stream, QtMsgType type, const QString &msg);

      };

    #ifdef Q_OS_OSX
    bool copyDataFolderFromAppBundleIfNeeded();//The boolean returns true if the data folders are good to go.
    void configureDefaultAstrometry();
    bool copyRecursively(QString sourceFolder, QString destFolder);
    #endif

}

#endif
