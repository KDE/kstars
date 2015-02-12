/***************************************************************************
                          observer.h  -  description

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
#ifndef OBSERVER_H_
#define OBSERVER_H_

#include "oal/oal.h"

#include <QString>

/// FIXME: why not just use a QHash?

/**
 * @class OAL::Observer
 *
 * Information on user who created or contributed to the observation.
 */
class OAL::Observer {
    public:
       QString id() const { return m_Id; }
       QString name() const { return m_Name; }
       QString surname() const { return m_Surname; }
       QString contact() const { return m_Contact; }
       Observer( QString _id,  QString _name ="", QString _surname = "", QString _contact = "" ) { setObserver( _id, _name, _surname, _contact ); }
       void setObserver( QString _id, QString _name = "", QString _surname= "", QString _contact = "" );
    private:
        QString m_Name, m_Surname, m_Contact, m_Id;
};
#endif
