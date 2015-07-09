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


ConstellationsArt::ConstellationsArt(dms midpointra, dms midpointdec, float pa, float sf, QString abbreviation, QString filename)
{
    positionAngle = pa;
    scaleFactor = sf;
    abbrev = abbreviation;
    imageFileName = filename;
    loadImage();

    //This sets both current and J2000 RA/DEC to the values ra and dec.
    constellationMidPoint = new SkyPoint(midpointra,midpointdec);
}

ConstellationsArt::~ConstellationsArt()
{
    delete constellationMidPoint;
}

void ConstellationsArt::loadImage()
{
        constellationArtImage = TextureManager::getImage( imageFileName );
}
