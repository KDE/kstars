/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "guideinterface.h"

#include "guide.h"

#define MAX_GUIDE_STARS 10

namespace Ekos
{
bool GuideInterface::setGuiderParams(double ccdPixelSizeX, double ccdPixelSizeY, double mountAperture,
                                     double mountFocalLength)
{
    this->ccdPixelSizeX    = ccdPixelSizeX;
    this->ccdPixelSizeY    = ccdPixelSizeY;
    this->mountAperture    = mountAperture;
    this->mountFocalLength = mountFocalLength;

    return true;
}

bool GuideInterface::getGuiderParams(double *ccdPixelSizeX, double *ccdPixelSizeY, double *mountAperture,
                                     double *mountFocalLength)
{
    *ccdPixelSizeX    = this->ccdPixelSizeX;
    *ccdPixelSizeY    = this->ccdPixelSizeY;
    *mountAperture    = this->mountAperture;
    *mountFocalLength = this->mountFocalLength;

    return true;
}

bool GuideInterface::setFrameParams(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t binX, uint16_t binY)
{
    if (w <= 0 || h <= 0)
        return false;

    subX = x;
    subY = y;
    subW = w;
    subH = h;

    subBinX = binX;
    subBinY = binY;

    return true;
}

bool GuideInterface::getFrameParams(uint16_t *x, uint16_t *y, uint16_t *w, uint16_t *h, uint16_t *binX, uint16_t *binY)
{
    *x = subX;
    *y = subY;
    *w = subW;
    *h = subH;

    *binX = subBinX;
    *binY = subBinY;

    return true;
}

void GuideInterface::setStarPosition(QVector3D &starCenter)
{
    INDI_UNUSED(starCenter);
}

void GuideInterface::setMountCoords(const SkyPoint &position, ISD::Mount::PierSide side)
{
    mountRA = position.ra();
    mountDEC = position.dec();
    mountAzimuth = position.az();
    mountAltitude = position.alt();
    pierSide = side;
}

void GuideInterface::setPierSide(ISD::Mount::PierSide newSide)
{
    pierSide = newSide;
}

}
