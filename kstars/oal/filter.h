/***************************************************************************
                          filter.h  -  description

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
#ifndef FILTER_H_
#define FILTER_H_

#include "oal/oal.h"

#include <QString>

class OAL::Filter {
    public:
        Filter( QString id, QString model, QString vendor, QString type, QString color ) { setFilter( id, model, vendor, type, color ); }
        QString id() { return m_Id; }
        QString name() { return m_Name; }
        QString model() { return m_Model; }
        QString vendor() { return m_Vendor; }
        QString type() { return m_Type; }
        QString color() { return m_Color; }
        void setFilter( QString _id, QString _model, QString _vendor, QString _type, QString _color );
    private:
        QString m_Id, m_Model, m_Vendor, m_Type, m_Color, m_Name;
};
#endif
