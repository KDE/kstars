/***************************************************************************
                          author.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Fri Oct 7 2011
    copyright            : (C) 2011 by Łukasz Jaśkiewicz
    email                : lucas.jaskiewicz@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "author.h"

using namespace SkyGuides;

void Author::setAuthor(const QString &name, const QString &surname, const QStringList &contacts)
{
    m_Name = name;
    m_Surname = surname;
    m_Contacts = contacts;
}
