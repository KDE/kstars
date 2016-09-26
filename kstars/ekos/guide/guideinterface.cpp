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
    ccdFrameWidth=ccdFrameHeight=0;
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

bool GuideInterface::setFrameParams(uint16_t width, uint16_t height)
{
    if( width <= 0 || height <= 0 )
        return false;

    ccdFrameWidth  = width;
    ccdFrameHeight = height;

    return true;
}
}


