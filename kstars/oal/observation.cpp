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

#include "observation.h"
#include "observer.h"
#include "observationtarget.h"
#include "site.h"
#include "session.h"
#include "scope.h"
#include "eyepiece.h"
#include "filter.h"
#include "lens.h"

using namespace OAL;

void Observation::setObservation( QString _id, QString _observer, QString _site, QString _session, QString _target, KStarsDateTime _begin,
                                  double _faintestStar, double _seeing, QString _scope, QString _eyepiece, QString _lens, QString _filter,
                                  QString _result, QString _lang )
{
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
    m_Lens = _lens;
    m_Filter = _filter;
    m_Result = _result;
    m_Lang = _lang;
}

Observation::Observation( QString id, Observer* observer, Session* session, ObservationTarget* target, KStarsDateTime begin, double faintestStar,
                          double seeing, Scope* scope, Eyepiece* eyepiece, Lens *lens, Filter* filter,  QString result, QString lang )
{
    if( observer )
        m_Observer = observer->id();
    if( target )
        m_Target = target->id();
    if( session ) {
        m_Session = session->id();
        m_Site = session->site();
    }
    if( scope )
        m_Scope = scope->id();
    if( lens )
        m_Lens = lens->id();
    if( filter )
        m_Filter = filter->id();
    if( eyepiece )
        m_Eyepiece = eyepiece->id();
    m_Name = id;
    m_Begin = begin;
    m_FaintestStar = faintestStar;
    m_Seeing = seeing;
    m_Result = result;
    m_Lang = lang;
}
