/*
    SPDX-FileCopyrightText: 2009 Prakash Mohan <prakash.mohan@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "oal/observation.h"

void OAL::Observation::setObservation(QString _id, QString _observer, QString _site, QString _session, QString _target,
                                      KStarsDateTime _begin, double _faintestStar, double _seeing, QString _scope,
                                      QString _eyepiece, QString _lens, QString _filter, QString _result, QString _lang)
{
    m_Name         = _id;
    m_Observer     = _observer;
    m_Site         = _site;
    m_Session      = _session;
    m_Target       = _target;
    m_Begin        = _begin;
    m_FaintestStar = _faintestStar;
    m_Seeing       = _seeing;
    m_Scope        = _scope;
    m_Eyepiece     = _eyepiece;
    m_Lens         = _lens;
    m_Filter       = _filter;
    m_Result       = _result;
    m_Lang         = _lang;
}

Observation::Observation(QString id, Observer *observer, Session *session, SkyObject *target, KStarsDateTime begin,
                         double faintestStar, double seeing, Scope *scope, Eyepiece *eyepiece, Lens *lens,
                         Filter *filter, QString result, QString lang)
{
    if (observer)
        m_Observer = observer->id();
    if (target)
        m_Target = target->name();
    if (session)
    {
        m_Session = session->id();
        m_Site    = session->site();
    }
    if (scope)
        m_Scope = scope->id();
    if (lens)
        m_Lens = lens->id();
    if (filter)
        m_Filter = filter->id();
    if (eyepiece)
        m_Eyepiece = eyepiece->id();
    m_Name         = id;
    m_Begin        = begin;
    m_FaintestStar = faintestStar;
    m_Seeing       = seeing;
    m_Result       = result;
    m_Lang         = lang;
}
