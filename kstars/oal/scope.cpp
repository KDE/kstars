/***************************************************************************
                          scope.cpp  -  description

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

#include "oal/scope.h"
#include <QJsonObject>
void OAL::Scope::setScope(const QString &_id, const QString &_model, const QString &_vendor, const QString &_type,
                          double _focalLength, double _aperture)
{
    m_Id          = _id;
    m_Model       = _model;
    m_Vendor      = _vendor;
    m_Type        = _type;
    m_FocalLength = _focalLength;
    m_Aperture    = _aperture;
    //m_Name.append ( _vendor + ' ' + _model + ' ' + QString::number( _aperture ) + "mm f/" + QString::number( (_focalLength/_aperture), 'g', 1 ) + " (" + _id + ')' ) ;

    m_Name = _vendor + ' ' + _model + " (" + _id + ')';
}

QJsonObject OAL::Scope::toJson() const
{
    return
    {
        {"id", m_Id},
        {"model", m_Model},
        {"vendor", m_Vendor},
        {"type", m_Type},
        {"name", m_Name},
        {"focal_length", m_FocalLength},
        {"aperture", m_Aperture},
    };
}
