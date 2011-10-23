/***************************************************************************
                          eyepiece.cpp  -  description

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

#include "oal/eyepiece.h"

using namespace OAL;

QString Eyepiece::name() const
{
    return m_Vendor + ' ' + m_Model + ' ' + QString::number(m_FocalLength) + "mm (" + m_Id +  ')';
}

void Eyepiece::setEyepiece(const QString &id, const QString &model, const QString &vendor, const double fov, const QString &fovUnit,
                           const bool fovDefined, const double focalLength, const double maxFocalLength, const bool maxFocalLengthDefined)
{
    m_Id = id;
    m_Model = model;
    m_Vendor = vendor;
    m_AppFov = fov;
    m_AppFovUnit = fovUnit;
    m_AppFovDefined = fovDefined;
    m_FocalLength = focalLength;
    m_MaxFocalLength = maxFocalLength;
    m_MaxFocalLengthDefined = maxFocalLengthDefined;
}
