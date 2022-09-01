/*
    SPDX-FileCopyrightText: 2009 Prakash Mohan <prakash.mohan@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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

    m_Name = QString("%1 %2 %3@F/%4").arg(m_Vendor, m_Model, QString::number(m_FocalLength, 'f', 0),
                                          QString::number(m_FocalLength / m_Aperture, 'f', 1));
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
