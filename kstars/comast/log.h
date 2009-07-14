/***************************************************************************
                          log.h  -  description

                             -------------------
    begin                : Friday June 19, 2009
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
#ifndef LOG_H_
#define LOG_H_

#include "comast/comast.h"

#include <QString>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "kstars.h"
#include "dms.h"
#include "skyobjects/skyobject.h"
#include "comast/observer.h"
#include "comast/site.h"
#include "comast/session.h"
#include "comast/scope.h"
#include "comast/eyepiece.h"
#include "comast/filter.h"
#include "comast/lens.h"
#include "comast/observation.h"

class Comast::Log {
    public:
        QString writeLog( bool native = true );
        void writeBegin();
        void writeGeoDate();
        void writeObservers();
        void writeSites();
        void writeSessions();
        void writeTargets();
        void writeScopes();
        void writeEyePieces();
        void writeLenses();
        void writeFilters();
        void writeImagers();
        inline QList<SkyObject *> targetList() { return m_targetList; }
        void writeObserver( Comast::Observer *o );
        void writeSite( Comast::Site *s );
        void writeSession( Comast::Session *s );
        void writeTarget( SkyObject *o );
        void writeScope( Comast::Scope *s );
        void writeEyepiece( Comast::Eyepiece *ep );
        void writeLens( Comast::Lens *l );
        void writeFilter(Comast::Filter *f );
        void writeObservation( Comast::Observation *o );
//        void writeImager();
        void writeEnd();
        void readBegin( QString input );
        void readLog();
        void readUnknownElement();
        void readTargets();
        void readTarget();
        void readPosition();
        void readGeoDate();
    private:
        QList<SkyObject *> m_targetList;
        QList<Comast::Observer *> m_observerList;
        QList<Comast::Eyepiece *> m_eyepieceList; 
        QList<Comast::Filter *> m_filterList;
//        QList<Comast::Equipment *> m_equipmentList;
//        QList<Comast::Imager *> m_imagerList;
        QList<Comast::Site *> m_siteList;
        QList<Comast::Session *> m_sessionList;
        QList<Comast::Scope *> m_scopeList;
        QString output;
        bool native;
        dms ra, dec;
        KStars *ks;
        QXmlStreamWriter *writer;
        QXmlStreamReader *reader;
};
#endif
