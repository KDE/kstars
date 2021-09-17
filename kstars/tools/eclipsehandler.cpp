/*
    SPDX-FileCopyrightText: 2018 Valentin Boettcher <valentin@boettcher.cf>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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
