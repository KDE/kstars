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

class KStars;

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
        void writeEyepieces();
        void writeLenses();
        void writeFilters();
        void writeImagers();
        void writeObservations();
        inline QList<SkyObject *> *targetList() { return &m_targetList; }
        inline QList<Comast::Scope *> *scopeList() { return &m_scopeList; }
        inline QList<Comast::Site *> *siteList() { return &m_siteList; }
        inline QList<Comast::Session *> *sessionList() { return &m_sessionList; }
        inline QList<Comast::Eyepiece *> *eyepieceList() { return &m_eyepieceList; }
        inline QList<Comast::Lens *> *lensList() { return &m_lensList; }
        inline QList<Comast::Filter *> *filterList() { return &m_filterList; }
        inline QList<Comast::Observation *> *observationList() { return &m_observationList; }
        inline QList<Comast::Observer *> *observerList() { return &m_observerList; }
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
        void readObservers();
        void readSites();
        void readSessions();
        void readScopes();
        void readEyepieces();
        void readLenses();
        void readFilters();
        void readObservation( QString id );
        void readTarget();
        void readObserver( QString id );
        void readSite( QString id );
        void readSession( QString id, QString lang );
        void readScope( QString id );
        void readEyepiece( QString id );
        void readLens( QString id );
        void readFilter( QString id );
        void readPosition();
        void readGeoDate();
        QString readResult();
        Comast::Observer* findObserverByName( QString fullName );
        Comast::Observer* findObserverById( QString id );
        Comast::Site* findSiteByName( QString name );
        Comast::Site* findSiteById( QString id );
        Comast::Session* findSessionByName( QString id );
        Comast::Scope* findScopeByName( QString name );
        Comast::Scope* findScopeById( QString id );
        Comast::Eyepiece* findEyepieceByName( QString id );
        Comast::Lens* findLensByName( QString id );
        Comast::Filter* findFilterByName( QString id );
        Comast::Observation* findObservationByName( QString id );
        QHash<QString, QTime> timeHash() { return TimeHash; }
        KStarsDateTime dateTime() { return dt; }
        GeoLocation* geoLocation() { return geo; }
        inline QString writtenOutput() { return output; }
    private:
        QList<SkyObject *> m_targetList;
        QList<Comast::Observer *> m_observerList;
        QList<Comast::Eyepiece *> m_eyepieceList; 
        QList<Comast::Lens *> m_lensList; 
        QList<Comast::Filter *> m_filterList;
//        QList<Comast::Equipment *> m_equipmentList;
//        QList<Comast::Imager *> m_imagerList;
        QList<Comast::Site *> m_siteList;
        QList<Comast::Session *> m_sessionList;
        QList<Comast::Scope *> m_scopeList;
        QList<Comast::Observation *> m_observationList;
        QString output;
        bool native;
        dms ra, dec;
        KStars *ks;
        QXmlStreamWriter *writer;
        QXmlStreamReader *reader;
        QHash<QString, QTime> TimeHash;
        KStarsDateTime dt;
        GeoLocation *geo;
};
#endif
