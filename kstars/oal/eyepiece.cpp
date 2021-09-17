/*
    SPDX-FileCopyrightText: 2009 Prakash Mohan <prakash.mohan@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "oal/eyepiece.h"

void OAL::Eyepiece::setEyepiece(const QString &_id, const QString &_model, const QString &_vendor, double _fov,
                                const QString &_fovUnit, double _focalLength)
{
    m_Id          = _id;
    m_Model       = _model;
    m_Vendor      = _vendor;
    m_AppFovUnit  = _fovUnit;
    m_AppFOV      = _fov;
    m_FocalLength = _focalLength;
    m_Name        = _vendor + ' ' + _model + ' ' + QString::number(_focalLength) + "mm (" + _id + ')';
}
