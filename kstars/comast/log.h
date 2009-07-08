/***************************************************************************
                          log.h  -  description

                             -------------------
    begin                : Friday June 19, 2009
    copyright            : (C) 2009 by Prakash Mohan
    email                : prak902000@gmail.com
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

#include "dms.h"
#include "skyobjects/skyobject.h"

class Comast::Log {
    public:
        QString writeLog( bool native = true );
        void writeBegin();
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
//        void writeObserver();
//        void writeSite();
//        void writeSession();
          void writeTarget( SkyObject *o );
//        void writeScope();
//        void writeEyePiece();
//        void writeLense();
//        void writeFilter();
//        void writeImager();
        void writeEnd();
        void readBegin( QString input );
        void readLog();
        void readUnknownElement();
        void readTargets();
        void readTarget();
        void readPosition();
    private:
        QList<SkyObject *> m_targetList;
//        QList<Comast::Observer *> m_observerList;
//        QList<Comast::Eyepiece *> m_eyepieceList; 
//        QList<Comast::Filter *> m_filterList;
//        QList<Comast::Equipment *> m_equipmentList;
//        QList<Comast::Imager *> m_imagerList;
//        QList<Comast::ObservingSite *> m_siteList;
//        QList<Comast::ObservingSession *> m_sessionList;
//        QList<Comast::Telescope *> m_scopeList;
        QString output;
        bool native;
        dms ra, dec;
        QXmlStreamWriter *writer;
        QXmlStreamReader *reader;
};
#endif
