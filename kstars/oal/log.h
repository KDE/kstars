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

#include "oal/oal.h"

#include <QString>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "kstars.h"
#include "dms.h"
#include "skyobjects/skyobject.h"
#include "oal/observer.h"
#include "oal/site.h"
#include "oal/session.h"
#include "oal/scope.h"
#include "oal/eyepiece.h"
#include "oal/filter.h"
#include "oal/lens.h"
#include "oal/observation.h"

class KStars;

class OAL::Log {
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
        inline QList<OAL::Scope *> *scopeList() { return &m_scopeList; }
        inline QList<OAL::Site *> *siteList() { return &m_siteList; }
        inline QList<OAL::Session *> *sessionList() { return &m_sessionList; }
        inline QList<OAL::Eyepiece *> *eyepieceList() { return &m_eyepieceList; }
        inline QList<OAL::Lens *> *lensList() { return &m_lensList; }
        inline QList<OAL::Filter *> *filterList() { return &m_filterList; }
        inline QList<OAL::Observation *> *observationList() { return &m_observationList; }
        inline QList<OAL::Observer *> *observerList() { return &m_observerList; }
        void writeObserver( OAL::Observer *o );
        void writeSite( OAL::Site *s );
        void writeSession( OAL::Session *s );
        void writeTarget( SkyObject *o );
        void writeScope( OAL::Scope *s );
        void writeEyepiece( OAL::Eyepiece *ep );
        void writeLens( OAL::Lens *l );
        void writeFilter(OAL::Filter *f );
        void writeObservation( OAL::Observation *o );
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
        void readSite( QString id );
        void readSession( QString id, QString lang );
        void readScope( QString id );
        void readEyepiece( QString id );
        void readLens( QString id );
        void readFilter( QString id );
        void readPosition();
        void readGeoDate();
        QString readResult();
        OAL::Observer* findObserverByName( QString fullName );
        OAL::Observer* findObserverById( QString id );
        OAL::Site* findSiteByName( QString name );
        OAL::Site* findSiteById( QString id );
        OAL::Session* findSessionByName( QString id );
        OAL::Scope* findScopeByName( QString name );
        OAL::Scope* findScopeById( QString id );
        OAL::Eyepiece* findEyepieceById( QString id );
        OAL::Lens* findLensById( QString id );
        OAL::Filter* findFilterById( QString id );
        OAL::Eyepiece* findEyepieceByName( QString name );
        OAL::Lens* findLensByName( QString name );
        OAL::Filter* findFilterByName( QString name );
        OAL::Observation* findObservationByName( QString name );
        QHash<QString, QTime> timeHash() { return TimeHash; }
        KStarsDateTime dateTime() { return dt; }
        GeoLocation* geoLocation() { return geo; }
        inline QString writtenOutput() { return output; }
    private:
        QList<SkyObject *> m_targetList;
        QList<OAL::Observer *> m_observerList;
        QList<OAL::Eyepiece *> m_eyepieceList; 
        QList<OAL::Lens *> m_lensList; 
        QList<OAL::Filter *> m_filterList;
//        QList<OAL::Equipment *> m_equipmentList;
//        QList<OAL::Imager *> m_imagerList;
        QList<OAL::Site *> m_siteList;
        QList<OAL::Session *> m_sessionList;
        QList<OAL::Scope *> m_scopeList;
        QList<OAL::Observation *> m_observationList;
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
