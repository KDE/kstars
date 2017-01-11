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

/**
 * @class OAL::Filter
 *
 * Information of user filters
 */
class OAL::Filter {
    public:
        Filter( const QString& id, const QString& model, const QString& vendor, const QString& type, const QString &offset, const QString& color ) { setFilter( id, model, vendor, type, offset, color ); }
        QString id() const { return m_Id; }
        QString name() const { return m_Name; }
        QString model() const { return m_Model; }
        QString vendor() const { return m_Vendor; }
        QString type() const { return m_Type; }
        QString color() const { return m_Color; }
        QString offset() const { return m_Offset; }
        void setOffset(const QString & _offset) { m_Offset = _offset; }
        void setFilter( const QString& _id, const QString &_model, const QString& _vendor, const QString &_type, const QString& _offset, const QString& _color );
    private:
        QString m_Id, m_Model, m_Vendor, m_Type, m_Color, m_Name, m_Offset;
};
#endif
