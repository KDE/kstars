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
#include "skypainter.h"
#include "skyobjects/skyobject.h"
#include "skycomponents/constellationartcomponent.h"
#include "skypoint.h"
#include "auxiliary/dms.h"

class QImage;
class SkyPoint;
class dms;

/**
  *@class ConstellationsArt
    *Provides all necessary information about a constellationsart object
    *data inherited from SkyObject includes RA/DEC coordinate pairs.
    *Data specific to ConstellationsArt objects includes the abbreviation
    *filename, constellation image, position angle, width and height.
    *@short Information about a ConstellationsArt object. This class represents a constellation image.
    *@author M.S.Adityan
    *@version 0.1
    */
class ConstellationsArt: public SkyObject{

private:
    QString abbrev, imageFileName;
    QImage constellationArtImage;
    double positionAngle, width, height;
    bool imageLoaded=false;

public:

    /**
     *Constructor. Set ConstellationsArt data according to parameters.
     *@param midpointra RA of the midpoint of the constellation
     *@param midpointdec DEC of the midpoint of the constellation
     *@param pa position angle
     *@param w width of the constellation image
     *@param h height of the constellation image
     *@param abbreviation abbreviation of the constellation
     *@param filename the file name of the image of the constellation.
     */
    explicit ConstellationsArt(dms midpointra, dms midpointdec, double pa, double w, double h, QString abbreviation,QString filename);

    /** *Destructor */
     ~ConstellationsArt();

    /** @return an object's image */
    const QImage& image() { if (imageLoaded) return constellationArtImage; else { loadImage(); return constellationArtImage;}}

    /** Load the object's image. This also scales the object's image to 1024x1024 pixels. */
    void loadImage();

    /** @return an object's abbreviation */
    inline QString getAbbrev() const { return abbrev;}

    /** @return an object's image file name*/
    inline QString getImageFileName() const {return imageFileName;}

    /** @return an object's position angle */
    inline double pa() const { return positionAngle; }

    /** Set the position angle */
    inline void setPositionAngle(double pa) { positionAngle = pa; }

    /** @return an object's width */
    inline double getWidth() {return width; }

    /** @return an object's height*/
    inline double getHeight() {return height;}

};

#endif // CONSTELLATIONSART_H
