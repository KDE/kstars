/*  Ekos Internal Guider Class
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>.

    Based on lin_guider

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "internalguider.h"

#include "Options.h"

namespace Ekos
{

InternalGuider::InternalGuider()
{

    // TODO must develop a parent abstract GuideProcess class that is then inherited by both the internal guider and PHD2 and any additional future guiders
    // It should provide the interface to hook its actions and results here instead of this mess.
    pmath = new cgmath();

    connect(pmath, SIGNAL(newAxisDelta(double,double)), this, SIGNAL(newAxisDelta(double,double)));
    connect(pmath, SIGNAL(newAxisDelta(double,double)), this, SLOT(updateGuideDriver(double,double)));
    connect(pmath, SIGNAL(newStarPosition(QVector3D,bool)), this, SLOT(setStarPosition(QVector3D,bool)));

    calibration = new internalCalibration(pmath, this);

    connect(calibration, SIGNAL(newStatus(Ekos::GuideState)), this, SLOT(setStatus(Ekos::GuideState)));

    guider = new internalGuider(pmath, this);

}

InternalGuider::~InternalGuider()
{
}

}
