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

#include "ksnumbers.h"
#include "skyobjects/ksmoon.h"
#include "skyobjects/kssun.h"

#include <QCalendarWidget>

MoonPhaseTool::MoonPhaseTool(QWidget *parent) : QDialog(parent, Qt::Dialog)
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif

    //KStarsDateTime dtStart ( KStarsDateTime::currentDateTime() );
    m_Moon.reset(new KSMoon);
    m_Sun.reset(new KSSun);
    mpc.reset(new MoonPhaseCalendar(*m_Moon, *m_Sun));
    //gcw = new GenericCalendarWidget( *mpc, this );

    gcw = new QCalendarWidget(this);
    //FIXME Need porting to KF5, can we use QCalendarWidget instead of GenericCalendarWidget?
    //setButtons( 0 );
    setFixedSize(gcw->size());
    setWindowTitle(xi18n("Moon Phase Calendar"));
}
