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

#include "scope.h"

using namespace OAL;

QString Scope::name() const
{
    return m_Vendor + ' ' + m_Model + ' ' + QString::number(m_Aperture) + "mm f/" +
            QString::number((m_FocalLength/m_Aperture), 'g', 1) + " (" + m_Id + ')';
}

void Scope::setScope(const QString &id, const QString &model, const QString &vendor, const QString &type, const double focalLength,
                     const double aperture, const double lightGrasp, const bool lightGraspDefined, const bool erect, const bool truesided,
                     const bool orientationDefined)
{
    m_Id = id;
    m_Model = model;
    m_Vendor = vendor;
    m_Type = type;
    m_FocalLength = focalLength;
    m_Aperture = aperture;
    m_LightGrasp = lightGrasp;
    m_LightGraspDefined = lightGraspDefined;
    m_OrientationErect = erect;
    m_OrientationTruesided = truesided;
    m_OrientationDefined = orientationDefined;
}
