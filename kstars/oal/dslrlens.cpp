/*
    SPDX-FileCopyrightText: 2021 Jasem Mutlaq

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "oal/dslrlens.h"
#include <QJsonObject>

void OAL::DSLRLens::setDSLRLens(const QString &_id, const QString &_model, const QString &_vendor, double _focalLength, double _focalRatio)
{
    m_Id          = _id;
    m_Model       = _model;
    m_Vendor      = _vendor;
    m_FocalLength = _focalLength;
    m_FocalRatio  = _focalRatio;

    m_Name = QString("%1 %2 %3@F/%4").arg(m_Vendor, m_Model, QString::number(m_FocalLength, 'f', 0), QString::number(m_FocalRatio, 'f', 1));
}

QJsonObject OAL::DSLRLens::toJson() const
{
    return
    {
        {"id", m_Id},
        {"model", m_Model},
        {"vendor", m_Vendor},
        {"name", m_Name},
        {"focal_length", m_FocalLength},
        {"focal_ratio", m_FocalRatio},
    };
}
