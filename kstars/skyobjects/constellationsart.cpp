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


ConstellationsArt::ConstellationsArt(int midX, int midY, dms midra, dms middec, int X,int Y,dms ra,dms dec,QString abbreviation,QString filename)
{
    midx = midX;
    midy = midY;
    x = X;
    y = Y;
    abbrev = abbreviation;
    imageFileName = filename;
    loadImage();

    //This sets both current and J2000 RA/DEC to the values ra and dec.
    constellationMidPoint = new SkyPoint(midra,middec);
    star = new SkyPoint(ra,dec);
}

ConstellationsArt::~ConstellationsArt()
{
    delete constellationMidPoint;
    delete star;
}
/*
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
*/
void ConstellationsArt::loadImage()
{
        constellationArtImage = TextureManager::getImage( imageFileName );
}
