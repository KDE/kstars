/*
    SPDX-FileCopyrightText: 2015 M.S.Adityan <msadityan@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "constellationsart.h"

#include "texturemanager.h"

ConstellationsArt::ConstellationsArt(dms &midpointra, dms &midpointdec, double pa, double w, double h,
                                     const QString &abbreviation, const QString &filename)
{
    positionAngle = pa;
    abbrev        = abbreviation;
    imageFileName = filename;

    width  = w;
    height = h;

    //loadImage();

    //This sets both current and J2000 RA/DEC to the values ra and dec.
    setRA(midpointra);
    setDec(midpointdec);
}

void ConstellationsArt::loadImage()
{
    constellationArtImage = TextureManager::getImage(imageFileName);
    imageLoaded           = true;
}
