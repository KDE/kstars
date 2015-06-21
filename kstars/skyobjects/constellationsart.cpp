/***************************************************************************
                          constellationsart.cpp  -  K Desktop Planetarium
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
#include "constellationsart.h"
#include "kstars/texturemanager.h"

ConstellationsArt::ConstellationsArt(int t, int serial, const QString &n){

    setType(t);
    setName(n);
}

ConstellationsArt::~ConstellationsArt()
{
}

ConstellationsArt::ConstellationsArt(const ConstellationsArt &o){

    x1 = o.getx1();
    x2 = o.getx2();
    x3 = o.getx3();
    y1 = o.gety1();
    y2 = o.gety2();
    y3 = o.gety3();
    hd1 = o.gethd1();
    hd2 = o.gethd2();
    hd3 = o.gethd3();
    abbrev = o.getAbbrev();
    imageFileName = o.getImageFileName();
    constart_image = o.image();
}

void ConstellationsArt::loadImage()
{
        constart_image = TextureManager::getImage( imageFileName );
}
