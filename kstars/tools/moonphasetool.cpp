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
#include "skyobjects/ksmoon.h"
#include "skyobjects/kssun.h"

MoonPhaseTool::MoonPhaseTool(QWidget *parent)
    : KDialog( parent, Qt::Dialog )
{
    setButtons( 0 );
    KStarsDateTime dtStart ( KStarsDateTime::currentDateTime() );
    m_Moon = new KSMoon;
    m_Sun = new KSSun;
    mpc = new MoonPhaseCalendar( *m_Moon, *m_Sun );
    gcw = new GenericCalendarWidget( *mpc, this );
    setFixedSize( gcw->size() );
    setCaption( i18n("Moon Phase Calendar") );
}


MoonPhaseTool::~MoonPhaseTool() {
    delete m_Moon;
    delete m_Sun;
    delete mpc;
}
