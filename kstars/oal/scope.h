/***************************************************************************
                          scope.h  -  description

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
#ifndef SCOPE_H_
#define SCOPE_H_

#include "oal.h"

#include <QString>

class OAL::Scope
{
public:
    Scope(const QString &id, const QString &model, const QString &vendor, const QString &type, const double focalLength, const double aperture,
          const double lightGrasp, const bool lightGraspDefined, const bool erect, const bool truesided, const bool orientationDefined)
    {
        setScope(id, model, vendor, type, focalLength, aperture, lightGrasp, lightGraspDefined, erect, truesided, orientationDefined);
    }

    QString id() const { return m_Id; }
    QString name() const;
    QString model() const { return m_Model; }
    QString vendor() const { return m_Vendor; }
    QString type() const { return m_Type; }
    QString driver() const { return m_INDIDriver; }
    double focalLength() const { return m_FocalLength; }
    double aperture() const { return m_Aperture; }
    double lightGrasp() const { return m_LightGrasp; }
    bool orientationErect() const { return m_OrientationErect; }
    bool orientationTruesided() const { return m_OrientationTruesided; }

    bool isLightGraspDefined() const { return m_LightGraspDefined; }
    bool isOrientationDefined() const { return m_OrientationDefined; }

    void setScope(const QString &id, const QString &model, const QString &vendor, const QString &type, const double focalLength,
                  const double aperture, const double lightGrasp, const bool lightGraspDefined, const bool erect, const bool truesided,
                  const bool orientationDefined);

    void setINDIDriver(const QString &driver) { m_INDIDriver = driver; }

private:
    QString m_Id, m_Model, m_Vendor, m_Type, m_INDIDriver;
    double m_FocalLength, m_Aperture;
    double m_LightGrasp;
    bool m_OrientationErect;
    bool m_OrientationTruesided;

    bool m_LightGraspDefined;
    bool m_OrientationDefined;
};

#endif
