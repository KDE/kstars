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

OAL::Filter::Filter(const QString &id, const QString &model, const QString &vendor, const QString &type, const QString &color,
                          double exposure, int offset, bool useAutoFocus, const QString &lockedFilter, int absFocusPosition)
{
    m_Id       = id;
    m_Model    = model;
    m_Vendor   = vendor;
    m_Type     = type;
    m_Color    = color;
    m_Name     = vendor + ' ' + model + " - " + type + ' ' + color + " (" + id + ')';
    m_Offset   = offset;
    m_Exposure = exposure;
    m_LockedFilter = lockedFilter;
    m_UseAutoFocus = useAutoFocus;
    m_AbsoluteFocusPosition = absFocusPosition;
}
