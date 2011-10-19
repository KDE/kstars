/***************************************************************************
                          observation.h  -  description

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
#ifndef OBSERVATION_H_
#define OBSERVATION_H_

#include <QString>

#include "oal.h"
#include "kstarsdatetime.h"
#include "skyobjects/skyobject.h"

using namespace OAL;

class OAL::Observation {
public:
    Observation() : m_Seeing(0), m_FaintestStar(0)
    {}

    Observation( QString id, QString observer, QString site, QString session, QString target, KStarsDateTime begin, double faintestStar,
                 double seeing, QString scope, QString eyepiece, QString lens, QString filter,  QString result, QString lang )
    {
        setObservation( id, observer, site, session, target, begin, faintestStar, seeing, scope, eyepiece, lens, filter, result, lang );
    }

    Observation( QString id, Observer* observer, Session* session, ObservationTarget* target, KStarsDateTime begin, double faintestStar,
                 double seeing, Scope* scope, Eyepiece* eyepiece, Lens* lens, Filter* filter,  QString result, QString lang );

    QString id() const { return m_Name; }
    QString target() const { return m_Target; }
    QString observer() const { return m_Observer; }
    QString site() const { return m_Site; }
    QString session() const { return m_Session; }
    QString scope() const { return m_Scope; }
    QString eyepiece() const { return m_Eyepiece; }
    QString lens() const { return m_Lens; }
    QString filter() const { return m_Filter; }
    QString lang() const { return m_Lang; }
    QString result() const { return m_Result; }
    double seeing() const { return m_Seeing; }
    double faintestStar() const{ return m_FaintestStar; }
    KStarsDateTime begin() const { return m_Begin; }
    void setObservation( QString _id, QString _observer, QString _site, QString _session, QString _target, KStarsDateTime _begin,
                         double _faintestStar, double _seeing, QString _scope, QString _eyepiece, QString _lens, QString _filter,
                         QString _result, QString _lang =  "en" );

private:
    QString m_Name;
    QString m_Target;
    QString m_Observer;
    QString m_Site;
    QString m_Session;
    QString m_Scope;
    QString m_Result;
    QString m_Eyepiece;
    QString m_Lens;
    QString m_Filter;
    QString m_Lang;
    double m_Seeing;
    double m_FaintestStar;
    KStarsDateTime m_Begin;
};

#endif
