/*
    SPDX-FileCopyrightText: 2010 Akarsh Simha <akarshsimha@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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
