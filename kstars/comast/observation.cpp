/***************************************************************************
                          observation.cpp  -  description

                             -------------------
    begin                : Wednesday July 8, 2009
    copyright            : (C) 2009 by Prakash Mohan
    email                : prakash.mohan@kdemail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "comast/observation.h"

void Comast::Observation::setObservation( QString _id, QString _observer, QString _site, QString _session, QString _target, KStarsDateTime _begin, double _faintestStar, double _seeing, QString _scope, QString _eyepiece,  QString _result, QString _lang ) {
    m_Name = _id;
    m_Observer = _observer;
    m_Site = _site;
    m_Session = _session;
    m_Target = _target;
    m_Begin = _begin;
    m_FaintestStar = _faintestStar;
    m_Seeing = _seeing;
    m_Scope = _scope;
    m_Eyepiece = _eyepiece;
    m_Result = _result;
    m_Lang = _lang;
}
