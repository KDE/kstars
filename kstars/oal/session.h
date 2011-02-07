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
       Session ( const QString &name, const QString& site, const KStarsDateTime& begin, const KStarsDateTime& end, const QString& weather, const QString& equipment, const QString& comment, const QString& lang ) { setSession( name, site, begin, end, weather, equipment, comment, lang ); }
       QString id() const { return m_Id; }
       QString site() const { return m_Site; }
       KStarsDateTime begin() const { return m_Begin; }
       KStarsDateTime end() const { return m_End; }
       QString weather() const { return m_Weather; }
       QString equipment() const { return m_Equipment; }
       QString comments() const { return m_Comment; }
       QString lang() const { return m_Lang; }
       void setSession( const QString& _name, const QString& _site, const KStarsDateTime& _begin, const KStarsDateTime& _end, const QString& _weather, const QString& _equipment, const QString& _comment, const QString& _lang = "en" );
    private:
        QString m_Site, m_Weather, m_Equipment, m_Comment, m_Lang, m_Id;
        KStarsDateTime m_Begin, m_End;
};
#endif
