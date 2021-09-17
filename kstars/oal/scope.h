/*
    SPDX-FileCopyrightText: 2009 Prakash Mohan <prakash.mohan@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "oal/oal.h"

#include <QString>

/**
 * @class OAL::Scope
 *
 * Information on telescope used in observation
 */
class OAL::Scope
{
    public:
        Scope(const QString &id, const QString &model, const QString &vendor, const QString &type, double focalLength,
              double aperture)
        {
            setScope(id, model, vendor, type, focalLength, aperture);
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
        QString type() const
        {
            return m_Type;
        }
        QString name() const
        {
            return m_Name;
        }
        QString driver() const
        {
            return m_INDIDriver;
        }
        double focalLength() const
        {
            return m_FocalLength;
        }
        double aperture() const
        {
            return m_Aperture;
        }
        void setScope(const QString &_id, const QString &_model, const QString &_vendor, const QString &_type,
                      double _focalLength, double _aperture);
        inline void setINDIDriver(const QString &driver)
        {
            m_INDIDriver = driver;
        }

        QJsonObject toJson() const;

    private:
        QString m_Id, m_Model, m_Vendor, m_Type, m_Name, m_INDIDriver;
        double m_FocalLength, m_Aperture;
};
