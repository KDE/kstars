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

ConstellationsArt::ConstellationsArt(dms midpointra, dms midpointdec, dms topleftra, dms topleftdec, dms toprightra, dms toprightdec, dms bottomleftra, dms bottomleftdec, dms bottomrightra, dms bottomrightdec, double pa, double w, double h, QString abbreviation, QString filename)
{
    positionAngle = pa;
    abbrev = abbreviation;
    imageFileName = filename;

    width = w;
    height = h;
    loadImage();

    //This sets both current and J2000 RA/DEC to the values ra and dec.
    constellationmidpoint = new SkyPoint(midpointra,midpointdec);
    topleft = new SkyPoint(topleftra,topleftdec);
    topright = new SkyPoint(toprightra,toprightdec);
    bottomleft = new SkyPoint(bottomleftra,bottomleftdec);
    bottomright = new SkyPoint(bottomrightra,bottomrightdec);
}

ConstellationsArt::~ConstellationsArt()
{
    delete constellationmidpoint;
    delete topleft;
    delete topright;
    delete bottomleft;
    delete bottomright;
}

void ConstellationsArt::loadImage()
{
        unscaled = TextureManager::getImage( imageFileName );
        constellationArtImage = unscaled.scaled(1024,1024,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
        qDebug()<<"The Unscaled image is"<<unscaled.width()<<unscaled.height();
        qDebug()<<"The scaled image is"<<constellationArtImage.width(),constellationArtImage.height();
}
