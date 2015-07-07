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

class ConstellationsArt{

private:
    QString abbrev, imageFileName;
    int midx,midy,x,y;
    QImage constellationArtImage;

public:

    SkyPoint* constellationMidPoint;
    SkyPoint* star;

    /**
     *Constructor. Set SkyObject data according to arguments.
     *@param t Type of object
     *@param serial Serial number from constellationsart.txt
     *@param n Primary name
     */
    explicit ConstellationsArt(int midX, int midY, dms midra, dms middec, int X,int Y,dms ra,dms dec,QString abbreviation,QString filename);

    /** @short Copy constructor.
     *  @param o ConstellationsArt object from which to copy data
     */
    ConstellationsArt (const ConstellationsArt &o );

    //Destructor
     ~ConstellationsArt();

    /** @return an object's image */
    const QImage& image() const { return constellationArtImage; }

    /** Load the object's image */
    void loadImage();

    /** @return an object image's width */
    inline int imageWidth() const{ return constellationArtImage.width(); }

    /** @return an object image's height */
    inline int imageHeight() const{ return constellationArtImage.height(); }

    /** @return an object's abbreviation */
    inline QString getAbbrev() const { return abbrev;}

    /** @return an object's image file name*/
    inline QString getImageFileName() const {return imageFileName;}

   /** @return an object's midx */
    inline int getmidx() const { return midx; }

   /** @return an object's midy */
    inline int getmidy() const { return midy; }

   /** @return an object's x */
    inline int getx() const { return x; }

   /** @return an object's y */
    inline int gety() const { return y; }

};

#endif // CONSTELLATIONSART_H
