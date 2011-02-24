/***************************************************************************
                  moonphasetool.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sat Jun 26 2010
    copyright            : (C) 2010 by Akarsh Simha
    email                : akarshsimha@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "moonphasetool.h"

#include "kstarsdata.h"
#include "skyobjects/ksplanetbase.h"

MoonPhaseTool::MoonPhaseTool(QWidget *parentSplit)
    : QFrame(parentSplit)
{
    KStarsDateTime dtStart ( KStarsDateTime::currentDateTime() );
    m_Moon = (KSMoon *) KSPlanetBase::createPlanet( KSPlanetBase::MOON );
    mpc = new MoonPhaseCalendar( *m_Moon );
    gcw = new GenericCalendarWidget( *mpc, this );
}


MoonPhaseTool::~MoonPhaseTool() {
    delete m_Moon;
    delete mpc;
}
