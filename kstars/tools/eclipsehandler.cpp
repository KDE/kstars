/***************************************************************************
               eclipsehandler.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Tue 18/09/2018
    copyright            : (C) 2018 Valentin Boettcher
    email                : valentin@boettcher.cf
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "eclipsehandler.h"

EclipseEvent::EclipseEvent(long double jd, GeoLocation geoPlace, ECLIPSE_TYPE type) : QObject(),
  m_type { type }, m_geoPlace { geoPlace }, m_jd { jd }
{
    qRegisterMetaType<EclipseEvent_s>("EclipseEvent_s");
}

EclipseEvent::~EclipseEvent()
{

}

EclipseHandler::EclipseHandler(QObject * parent) : ApproachSolver (parent)
{
}

EclipseHandler::~EclipseHandler()
{

}
