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


ConstellationsArt::ConstellationsArt(int X1, int Y1, dms ra1, dms dec1, int X2, int Y2, dms ra2, dms dec2, QString abbreviation, QString filename)
{
    x1 = X1;
    y1 = Y1;
    x2 = X2;
    y2 = Y2;
    abbrev = abbreviation;
    imageFileName = filename;
    loadImage();

    //This sets both current and J2000 RA/DEC to the values ra and dec.
    //We need to update the star positions later on before attempting to draw the constellation image
    //The star positions are updated in SkyQPainter::drawConstellationArtImage() function
    star1 = new SkyPoint(ra1,dec1);
    star2 = new SkyPoint(ra2,dec2);
}

ConstellationsArt::~ConstellationsArt()
{
    delete star1;
    delete star2;
}

ConstellationsArt::ConstellationsArt(const ConstellationsArt &o){

    x1 = o.getx1();
    x2 = o.getx2();
    y1 = o.gety1();
    y2 = o.gety2();
    hd1 = o.gethd1();
    hd2 = o.gethd2();
    abbrev = o.getAbbrev();
    imageFileName = o.getImageFileName();
    constellationArtImage = o.image();
}

void ConstellationsArt::loadImage()
{
        constellationArtImage = TextureManager::getImage( imageFileName );
}
