/***************************************************************************
                          observer.cpp  -  description

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

#include "oal/observer.h"

void OAL::Observer::setObserver( QString _id, QString _name, QString _surname, QString _contact, bool _coobserving){
    m_Id = _id;
    m_Name = _name;
    m_Surname = _surname;
    m_Contact = _contact;
    m_Coobserving  = _coobserving;
}
