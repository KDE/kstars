/*
    SPDX-FileCopyrightText: 2009 Prakash Mohan <prakash.mohan@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "oal/oal.h"

#include <QString>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

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

/**
 * @class Log
 *
 * Implementation of <a href="https://code.google.com/p/openastronomylog/">Open Astronomy Log</a> (OAL) XML specifications to record observation logs.
 */
class OAL::Log
{
  public:
    ~Log();
    QString writeLog(bool native = true);
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
    inline QList<QSharedPointer<SkyObject>> &targetList() { return m_targetList; }
    inline QList<OAL::Scope *> *scopeList() { return &m_scopeList; }
    inline QList<OAL::Site *> *siteList() { return &m_siteList; }
    inline QList<OAL::Session *> *sessionList() { return &m_sessionList; }
    inline QList<OAL::Eyepiece *> *eyepieceList() { return &m_eyepieceList; }
    inline QList<OAL::Lens *> *lensList() { return &m_lensList; }
    inline QList<OAL::Filter *> *filterList() { return &m_filterList; }
    inline QList<OAL::Observation *> *observationList() { return &m_observationList; }
    inline QList<OAL::Observer *> *observerList() { return &m_observerList; }
    void writeObserver(OAL::Observer *o);
    void writeSite(OAL::Site *s);
    void writeSession(OAL::Session *s);
    void writeTarget(SkyObject *o);
    void writeScope(OAL::Scope *s);
    void writeEyepiece(OAL::Eyepiece *ep);
    void writeLens(OAL::Lens *l);
    void writeFilter(OAL::Filter *f);
    void writeObservation(OAL::Observation *o);
    //        void writeImager();
    void writeEnd();
    void readBegin(QString input);
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
    void readObservation(const QString &id);
    void readTarget();
    void readSite(const QString &id);
    void readSession(const QString &id, const QString &lang);
    void readAll();
    SkyPoint readPosition(bool &Ok);
    void readGeoDate();
    QString readResult();
    OAL::Observer *findObserverByName(const QString &name);
    OAL::Observer *findObserverById(const QString &id);
    OAL::Site *findSiteByName(const QString &name);
    OAL::Site *findSiteById(const QString &id);
    OAL::Session *findSessionByName(const QString &id);
    OAL::Scope *findScopeByName(const QString &name);
    OAL::Scope *findScopeById(const QString &id);
    OAL::Eyepiece *findEyepieceById(const QString &id);
    OAL::Lens *findLensById(const QString &id);
    OAL::Filter *findFilterById(const QString &id);
    OAL::Eyepiece *findEyepieceByName(const QString &name);
    OAL::Lens *findLensByName(const QString &name);
    OAL::Filter *findFilterByName(const QString &name);
    OAL::Observation *findObservationByName(const QString &name);
    QHash<QString, QTime> timeHash() const { return TimeHash; }
    KStarsDateTime dateTime() const { return dt; }
    GeoLocation *geoLocation() { return geo; }
    inline QString writtenOutput() const { return output; }

  private:
    QList<QSharedPointer<SkyObject>> m_targetList;
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
    bool native { false };
    dms ra, dec;
    QXmlStreamWriter *writer { nullptr };
    QXmlStreamReader *reader { nullptr };
    QHash<QString, QTime> TimeHash;
    KStarsDateTime dt;
    GeoLocation *geo { nullptr };
};
