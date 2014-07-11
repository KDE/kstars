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
    : QDialog( parent, Qt::Dialog )
{

    //KStarsDateTime dtStart ( KStarsDateTime::currentDateTime() );
    m_Moon = new KSMoon;
    m_Sun = new KSSun;
    mpc = new MoonPhaseCalendar( *m_Moon, *m_Sun );
    //gcw = new GenericCalendarWidget( *mpc, this );

    gcw = new QCalendarWidget(this);
    //FIXME Need porting to KF5, can we use QCalenderWidget instead of GenericCalenderWidget?
    //setButtons( 0 );
    setFixedSize( gcw->size() );
    setWindowTitle( xi18n("Moon Phase Calendar") );
}


MoonPhaseTool::~MoonPhaseTool() {
    delete m_Moon;
    delete m_Sun;
    delete mpc;
}
