/***************************************************************************
                          filter.cpp  -  description

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

#include "oal/filter.h"

void OAL::Filter::setFilter( QString _id, QString _model, QString _vendor, QString _type, QString _color ){ 
    m_Id = _id;
    m_Model = _model;
    m_Vendor = _vendor;
    m_Type = _type;
    m_Color = _color;
    m_Name = _vendor + ' ' + _model + " - " + _type + ' ' + _color + " (" + _id + ')';  
}
