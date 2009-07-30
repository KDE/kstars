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

#include "comast/comast.h"

#include <QString>

class Comast::Scope {
    public:
        Scope( QString name, QString model, QString vendor, QString type, double focalLength, double aperture ) { setScope( name, model, vendor, type, focalLength, aperture ); }
        QString id() { return m_Name; }
        QString model() { return m_Model; }
        QString vendor() { return m_Vendor; }
        QString type() { return m_Type; }
        double focalLength() { return m_FocalLength; }
        double aperture() { return m_Aperture; }
        void setScope( QString _name, QString _model, QString _vendor, QString _type, double _focalLength, double _aperture );
    private:
        QString m_Name, m_Model, m_Vendor, m_Type;
        double m_FocalLength, m_Aperture;
};
#endif
