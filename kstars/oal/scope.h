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

class OAL::Scope {
    public:
        Scope( QString id, QString model, QString vendor, QString type, double focalLength, double aperture ) { setScope( id, model, vendor, type, focalLength, aperture ); }
        QString id() { return m_Id; }
        QString model() { return m_Model; }
        QString vendor() { return m_Vendor; }
        QString type() { return m_Type; }
        QString name() { return m_Name; }
        QString driver() { return m_INDIDriver; }
        double focalLength() { return m_FocalLength; }
        double aperture() { return m_Aperture; }
        void setScope( QString _id, QString _model, QString _vendor, QString _type, double _focalLength, double _aperture );
        inline void setINDIDriver(const QString &driver) { m_INDIDriver = driver; }
    private:
        QString m_Id, m_Model, m_Vendor, m_Type, m_Name, m_INDIDriver;
        double m_FocalLength, m_Aperture;
};
#endif
