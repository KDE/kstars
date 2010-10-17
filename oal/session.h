/***************************************************************************
                          session.h  -  description

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
#ifndef SESSION_H_
#define SESSION_H_

#include "oal/oal.h"
#include "kstarsdatetime.h"

#include <QString>

class OAL::Session {
    public:
       Session ( QString name, QString site, KStarsDateTime begin, KStarsDateTime end, QString weather, QString equipment, QString comment, QString lang ) { setSession( name, site, begin, end, weather, equipment, comment, lang ); }
       QString id() { return m_Id; }
       QString site() { return m_Site; }
       KStarsDateTime begin() { return m_Begin; }
       KStarsDateTime end() { return m_End; }
       QString weather() { return m_Weather; }
       QString equipment() { return m_Equipment; }
       QString comments() { return m_Comment; }
       QString lang() { return m_Lang; }
       void setSession( QString _name, QString _site, KStarsDateTime _begin, KStarsDateTime _end, QString _weather, QString _equipment, QString _comment, QString _lang = "en" );
    private:
        QString m_Site, m_Weather, m_Equipment, m_Comment, m_Lang, m_Id;
        KStarsDateTime m_Begin, m_End;
};
#endif
