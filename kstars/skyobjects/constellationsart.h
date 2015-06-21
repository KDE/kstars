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
#include "constartcomponent.h"

class QImage;

/** @class ConstellationsArt
 * @short Represents images for sky cultures
 * @author M.S.Adityan
 */

class ConstellationsArt: public SkyObject{

    friend class ConstArtComponent;

private:
    QString abbrev, imageFileName;
    int x1,y1,x2,y2,x3,y3,hd1,hd2,hd3,rank;
    QImage constart_image;

public:

    /**
     *Constructor. Set SkyObject data according to arguments.
     *@param t Type of object
     *@param serial Serial number from constellationsart.txt
     *@param n Primary name
     */
    explicit ConstellationsArt(int t=SkyObject::CONSTELLATION, int serial=0, const QString &n=QString());

    /** @short Copy constructor.
     *  @param o SkyObject from which to copy data
     */
    ConstellationsArt (const ConstellationsArt &o );

    //Destructor
    virtual ~ConstellationsArt();

    /** @return an object's image */
    const QImage& image() const { return constart_image; }

    /** Load the object's image */
    void loadImage();

    /** @return an object image's width */
    inline int imageWidth() const{ return constart_image.width(); }

    /** @return an object image's height */
    inline int imageHeight() const{ return constart_image.height(); }

    /** @return an object's abbreviation */
    inline QString getAbbrev() const { return abbrev;}

    /** @return an object's image file name*/
    inline QString getImageFileName() const {return imageFileName;}

   /** @return an object's x1 */
    inline int getx1() const { return x1; }

   /** @return an object's y1 */
    inline int gety1() const { return y1; }

   /** @return an object's x2 */
    inline int getx2() const { return x2; }

   /** @return an object's y2 */
    inline int gety2() const { return y2; }

   /** @return an object's x3 */
    inline int getx3() const { return x3; }

   /** @return an object's y3 */
    inline int gety3() const { return y3; }

   /** @return an object's hd1 */
    inline int gethd1() const { return hd1; }

   /** @return an object's hd2 */
    inline int gethd2() const { return hd2; }

   /** @return an object's hd3 */
    inline int gethd3() const { return hd3; }

    //UpdateID which would be compared with the global updateID to know when to redraw a skyobject in a draw cycle
    quint64 updateID;
};

#endif // CONSTELLATIONSART_H
