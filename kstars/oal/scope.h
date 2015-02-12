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

#include "oal/oal.h"

#include <QString>

/**
 * @class OAL::Scope
 *
 * Information on telescope used in observation
 */
class OAL::Scope {
    public:
        Scope( const QString& id, const QString& model, const QString& vendor, const QString& type, double focalLength, double aperture ) { setScope( id, model, vendor, type, focalLength, aperture ); }
        QString id() const { return m_Id; }
        QString model() const { return m_Model; }
        QString vendor() const { return m_Vendor; }
        QString type() const { return m_Type; }
        QString name() const { return m_Name; }
        QString driver() const { return m_INDIDriver; }
        double focalLength() const { return m_FocalLength; }
        double aperture() const { return m_Aperture; }
        void setScope( const QString& _id, const QString& _model, const QString& _vendor, const QString& _type, double _focalLength, double _aperture );
        inline void setINDIDriver(const QString &driver) { m_INDIDriver = driver; }
    private:
        QString m_Id, m_Model, m_Vendor, m_Type, m_Name, m_INDIDriver;
        double m_FocalLength, m_Aperture;
};
#endif
