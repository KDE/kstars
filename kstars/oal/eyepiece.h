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

#include "oal/oal.h"

#include <QString>

class OAL::Eyepiece {
    public:
        Eyepiece( QString id, QString model, QString vendor, double fov, QString fovUnit, double focalLength ) { setEyepiece( id, model, vendor, fov, fovUnit, focalLength ); }
        QString id() { return m_Id; }
        QString name() { return m_Name; }
        QString model() { return m_Model; }
        QString vendor() { return m_Vendor; }
        QString fovUnit() { return m_AppFovUnit; }
        double appFov() { return m_AppFOV; }
        double focalLength() { return m_FocalLength; }
        void setEyepiece( QString _id, QString _model, QString _vendor, double _fov, QString _fovUnit, double _focalLength );
    private:
        QString m_Id, m_Model, m_AppFovUnit, m_Vendor, m_Name;
        double m_AppFOV, m_FocalLength;
};
#endif
