/*
    SPDX-FileCopyrightText: 2009 Prakash Mohan <prakash.mohan@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "oal/oal.h"

#include <QString>

/**
 * @class OAL::Lens
 *
 * Information of lens utilized in the observation
 */
class OAL::Lens
{
    public:
        Lens(const QString &id, const QString &model, const QString &vendor, double factor)
        {
            setLens(id, model, vendor, factor);
        }
        QString id() const
        {
            return m_Id;
        }
        QString name() const
        {
            return m_Name;
        }
        QString model() const
        {
            return m_Model;
        }
        QString vendor() const
        {
            return m_Vendor;
        }
        double factor() const
        {
            return m_Factor;
        }
        void setLens(const QString &_id, const QString &_model, const QString &_vendor, double _factor);

    private:
        QString m_Id, m_Model, m_Vendor, m_Name;
        double m_Factor;
};
