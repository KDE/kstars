/*
    SPDX-FileCopyrightText: 2009 Prakash Mohan <prakash.mohan@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "oal/oal.h"

#include <QString>

/**
 * @class Eyepiece
 *
 * Information on user eye pieces
 */
class OAL::Eyepiece
{
    public:
        Eyepiece(const QString &id, const QString &model, const QString &vendor, double fov, const QString &fovUnit,
                 double focalLength)
        {
            setEyepiece(id, model, vendor, fov, fovUnit, focalLength);
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
        QString fovUnit() const
        {
            return m_AppFovUnit;
        }
        double appFov() const
        {
            return m_AppFOV;
        }
        double focalLength() const
        {
            return m_FocalLength;
        }
        void setEyepiece(const QString &_id, const QString &_model, const QString &_vendor, double _fov,
                         const QString &_fovUnit, double _focalLength);

    private:
        QString m_Id, m_Model, m_AppFovUnit, m_Vendor, m_Name;
        double m_AppFOV, m_FocalLength;
};
