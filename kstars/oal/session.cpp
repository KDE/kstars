/***************************************************************************
                          session.cpp  -  description

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

#include "oal/session.h"

void OAL::Session::setSession( QString _name, QString _site, KStarsDateTime _begin, KStarsDateTime _end, QString _weather, QString _equipment, QString _comment, QString _lang ) {
    m_Site = _site;
    m_Begin = _begin;
    m_End = _end;
    m_Weather = _weather;
    m_Equipment = _equipment;
    m_Comment = _comment;
    m_Lang = _lang;
    m_Id = _name;
}
