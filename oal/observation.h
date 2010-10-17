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

#include "oal/oal.h"
#include "kstarsdatetime.h"
#include "skyobjects/skyobject.h"
#include "oal/observer.h"
#include "oal/site.h"
#include "oal/session.h"
#include "oal/scope.h"
#include "oal/eyepiece.h"
#include "oal/filter.h"
#include "oal/lens.h"

#include <QString>

using namespace OAL;

class OAL::Observation {
    public:
        Observation( QString id, QString observer, QString site, QString session, QString target, KStarsDateTime begin, double faintestStar, double seeing, QString scope, QString eyepiece, QString lens, QString filter,  QString result, QString lang ) {
            setObservation( id, observer, site, session, target, begin, faintestStar, seeing, scope, eyepiece, lens, filter, result, lang );
        }
        Observation( QString id, Observer* observer, Session* session, SkyObject* target, KStarsDateTime begin, double faintestStar, double seeing, Scope* scope, Eyepiece* eyepiece, Lens* lens, Filter* filter,  QString result, QString lang );
        QString id() { return m_Name; }
        QString target() { return m_Target; }
        QString observer() { return m_Observer; }
        QString site() { return m_Site; }
        QString session() { return m_Session; }
        QString scope() { return m_Scope; }
        QString eyepiece() { return m_Eyepiece; }
        QString lens() { return m_Lens; }
        QString filter() { return m_Filter; }
        QString lang() { return m_Lang; }
        QString result() { return m_Result; }
        double seeing() { return m_Seeing; }
        double faintestStar() { return m_FaintestStar; }
        KStarsDateTime begin() { return m_Begin; }
        void setObservation( QString _id, QString _observer, QString _site, QString _session, QString _target, KStarsDateTime _begin, double _faintestStar, double _seeing, QString _scope, QString _eyepiece, QString _lens, QString _filter, QString _result, QString _lang =  "en" );
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
