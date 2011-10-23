/***************************************************************************
                          eyepiece.h  -  description

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

#ifndef EYEPIECE_H_
#define EYEPIECE_H_

#include "oal.h"

#include <QString>

class OAL::Eyepiece
{
public:
    Eyepiece(const QString &id, const QString &model, const QString &vendor, const double fov, const QString &fovUnit,
             const bool fovDefined, const double focalLength, const double maxFocalLength, const bool maxFocalLengthDefined)
    {
        setEyepiece(id, model, vendor, fov, fovUnit, fovDefined, focalLength, maxFocalLength, maxFocalLengthDefined);
    }

    QString id() const { return m_Id; }
    QString name() const;
    QString model() const { return m_Model; }
    QString vendor() const { return m_Vendor; }
    QString fovUnit() const { return m_AppFovUnit; }
    double appFov() const { return m_AppFov; }
    double focalLength() const { return m_FocalLength; }
    double maxFocalLength() const { return m_MaxFocalLength; }
    void setEyepiece(const QString &id, const QString &model, const QString &vendor, const double fov, const QString &fovUnit,
                     const bool fovDefined, const double focalLength, const double maxFocalLength, const bool maxFocalLengthDefined);

    bool isMaxFocalLengthDefined() const { return m_MaxFocalLengthDefined; }
    bool isFovDefined() const { return m_AppFovDefined; }

private:
    QString m_Id, m_Model, m_AppFovUnit, m_Vendor;
    double m_AppFov, m_FocalLength, m_MaxFocalLength;
    bool m_MaxFocalLengthDefined, m_AppFovDefined;
};

#endif
