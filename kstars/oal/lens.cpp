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

QString Lens::name() const
{
    if(m_Factor > 1)
        return m_Vendor + ' ' + m_Model + " - " + QString::number(m_Factor) + "x Barlow (" + m_Id + ')';
    else
        return m_Vendor + ' ' + m_Model + " - " + QString::number(m_Factor) + "x Focal Reducer (" + m_Id + ')';
}

void Lens::setLens(const QString &id, const QString &model, const QString &vendor, const double factor)
{
    m_Id = id;
    m_Model = model;
    m_Vendor = vendor;
    m_Factor = factor;
}
