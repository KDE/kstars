/***************************************************************************
                          lens.cpp  -  description

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

#include "lens.h"

using namespace OAL;

void Lens::setLens( const QString& _id, const QString& _model, const QString& _vendor, double _factor ){
    m_Id = _id;
    m_Model = _model;
    m_Vendor = _vendor;
    m_Factor = _factor;
    if( _factor > 1 )
        m_Name = _vendor + ' ' + _model + " - " + QString::number( _factor ) + "x Barlow (" + _id + ')'; 
    else
        m_Name = _vendor + ' ' + _model + " - " + QString::number( _factor ) + "x Focal Reducer (" + _id + ')'; 
}
