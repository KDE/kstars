/***************************************************************************
                          constellationsart.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2015-05-27
    copyright            : (C) 2015 by M.S.Adityan
    email                : msadityan@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef CONSTELLATIONSART_H
#define CONSTELLATIONSART_H

#include <QDebug>
#include <QString>
#include <QImage>
#include "skycomponent.h"
#include "culturelist.h"
#include "kstars/skypainter.h"
#include "kstars/skyobjects/skyobject.h"
#include "kstars/skycomponents/constellationartcomponent.h"
#include "skypoint.h"
#include "kstars/auxiliary/dms.h"

class QImage;
class SkyPoint;
class dms;

/** @class ConstellationsArt
 * @short Represents images for sky cultures
 * @author M.S.Adityan
 */

class ConstellationsArt: public SkyObject{

private:
    QString abbrev, imageFileName;
    QImage constellationArtImage;
    double positionAngle, width, height;

public:

    /**
     *Constructor. Set SkyObject data according to arguments.
     *@param t Type of object
     *@param serial Serial number from constellationsart.txt
     *@param n Primary name
     */
    explicit ConstellationsArt(dms midpointra, dms midpointdec, double pa, double w, double h, QString abbreviation,QString filename);

    //Destructor
     ~ConstellationsArt();

    /** @return an object's image */
    const QImage& image() const { return constellationArtImage; }

    /** Load the object's image */
    void loadImage();


    /** @return an object's abbreviation */
    inline QString getAbbrev() const { return abbrev;}

    /** @return an object's image file name*/
    inline QString getImageFileName() const {return imageFileName;}

   /** @return an object's position angle */
    inline double pa() const { return positionAngle; }

    inline void setPositionAngle(double pa) { positionAngle = pa; }

    inline double getWidth() {return width; }
    inline double getHeight() {return height;}

};

#endif // CONSTELLATIONSART_H
