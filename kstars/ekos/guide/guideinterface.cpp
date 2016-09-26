/*  Ekos
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "guide.h"

#include <QDateTime>

#include <KMessageBox>
#include <KLed>
#include <KLocalizedString>

#include <basedevice.h>

#include "Options.h"

#include "guideinterface.h"
#include "internalguide/gmath.h"

#include "ekos/auxiliary/darklibrary.h"

#include "indi/driverinfo.h"
#include "indi/clientmanager.h"

#include "fitsviewer/fitsviewer.h"
#include "fitsviewer/fitsview.h"

//#include "guide/rcalibration.h"
#include "guideadaptor.h"
#include "kspaths.h"

#define MAX_GUIDE_STARS 10

namespace Ekos
{

GuideInterface::GuideInterface()
{
    subX=subY=subW=subH=0;
    subBinX=subBinY=1;
    ccdPixelSizeX=ccdPixelSizeY=mountAperture=mountFocalLength=0;
}

bool GuideInterface::setGuiderParams(double ccdPixelSizeX, double ccdPixelSizeY, double mountAperture, double mountFocalLength)
{
    this->ccdPixelSizeX     = ccdPixelSizeX;
    this->ccdPixelSizeY     = ccdPixelSizeY;
    this->mountAperture     = mountAperture;
    this->mountFocalLength  = mountFocalLength;

    return true;
}

bool GuideInterface::setFrameParams(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t binX, uint8_t binY)
{
    if( w <= 0 || h <= 0 )
        return false;

    subX = x;
    subY = y;
    subW = w;
    subH = h;

    subBinX = binX;
    subBinY = binY;

    return true;
}
}


