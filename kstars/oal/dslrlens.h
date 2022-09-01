/*
    SPDX-FileCopyrightText: 2021 Jasem Mutlaq

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "oal/oal.h"

#include <QString>

/**
 * @class OAL::DSLRLens
 *
 * Information on DSLR lens used in astrophotography
 */
class OAL::DSLRLens
{
    public:
        DSLRLens(const QString &id, const QString &model, const QString &vendor, double focalLength, double focalRatio)
        {
            setDSLRLens(id, model, vendor, focalLength, focalRatio);
        }
        QString id() const
        {
            return m_Id;
        }
        QString model() const
        {
            return m_Model;
        }
        QString vendor() const
        {
            return m_Vendor;
        }
        QString name() const
        {
            return m_Name;
        }
        double focalLength() const
        {
            return m_FocalLength;
        }
        double focalRatio() const
        {
            return m_FocalRatio;
        }
        void setDSLRLens(const QString &_id, const QString &_model, const QString &_vendor, double _focalLength, double _focalRatio);

        QJsonObject toJson() const;

    private:
        QString m_Id, m_Model, m_Vendor, m_Name;
        double m_FocalLength, m_FocalRatio;
};
