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

#include <QString>
#include <QList>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "kstars.h"
#include "oal.h"
#include "skypoint.h"

class OAL::Log {
public:
    QString writeLog( bool native = true );
    void writeBegin();
    void writeGeoDate();
    void writeObservers();
    void writeUsedObservers();
    void writeSites();
    void writeSessions();
    void writeTargets();
    void writeUsedTargets();
    void writeScopes();
    void writeUsedScopes();
    void writeEyepieces();
    void writeUsedEyepieces();
    void writeLenses();
    void writeUsedLenses();
    void writeFilters();
    void writeUsedFilters();
    void writeImagers();
    void writeObservations();
    inline QList<OAL::ObservationTarget *> *targetList() { return &m_targetList; }
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
    void writeTarget( OAL::ObservationTarget *o );
    void writeScope( OAL::Scope *s );
    void writeEyepiece( OAL::Eyepiece *ep );
    void writeLens( OAL::Lens *l );
    void writeFilter(OAL::Filter *f );
    void writeObservation( OAL::Observation *o );
    void writeEnd();
    void readBegin( const QString &input );
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
    void readObservation( const QString &id );
    void readTarget( const QString &id );
    void readObserver( const QString &id );
    void readSite( const QString &id );
    void readSession( const QString &id, const QString &lang );
    void readScope( const QString &id );
    void readEyepiece( const QString &id );
    void readLens( const QString &id );
    void readFilter( const QString &id );
    SkyPoint readPosition( bool &ok );
    void readGeoDate();
    QString readResult();
    OAL::Observer* findObserverByName( const QString &fullName );
    OAL::Observer* findObserverById( const QString &id );
    OAL::Site* findSiteByName( const QString &name );
    OAL::Site* findSiteById( const QString &id );
    OAL::ObservationTarget* findTargetByName( const QString &name );
    OAL::ObservationTarget* findTargetById( const QString &id );
    OAL::Session* findSessionByName( const QString &id );
    OAL::Scope* findScopeByName( const QString &name );
    OAL::Scope* findScopeById( const QString &id );
    OAL::Eyepiece* findEyepieceById( const QString &id );
    OAL::Lens* findLensById( const QString &id );
    OAL::Filter* findFilterById( const QString &id );
    OAL::Eyepiece* findEyepieceByName( const QString &name );
    OAL::Lens* findLensByName( const QString &name );
    OAL::Filter* findFilterByName( const QString &name );
    OAL::Observation* findObservationByName( const QString &name );
    QHash<QString, QTime> timeHash() { return TimeHash; }
    KStarsDateTime dateTime() { return dt; }
    GeoLocation* geoLocation() { return geo; }
    inline QString writtenOutput() { return output; }

    void removeScope( int idx );
    void removeEyepiece( int idx );
    void removeLens( int idx );
    void removeFilter( int idx );

    void loadObserversFromFile();
    void saveObserversToFile();
    void loadEquipmentFromFile();
    void saveEquipmentToFile();

private:
    void markUsedObservers();
    void markUsedEquipment();

    QList<OAL::ObservationTarget *> m_targetList;
    QList<OAL::Observer *> m_observerList;
    QList<OAL::Eyepiece *> m_eyepieceList;
    QList<OAL::Lens *> m_lensList;
    QList<OAL::Filter *> m_filterList;
    QList<OAL::Site *> m_siteList;
    QList<OAL::Session *> m_sessionList;
    QList<OAL::Scope *> m_scopeList;
    QList<OAL::Observation *> m_observationList;

    QSet<QString> m_usedObservers;
    QSet<QString> m_usedEyepieces;
    QSet<QString> m_usedLens;
    QSet<QString> m_usedFilters;
    QSet<QString> m_usedScopes;

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
