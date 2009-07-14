/***************************************************************************
                          lens.h  -  description

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
#ifndef LENS_H_
#define LENS_H_

#include "comast/comast.h"

#include <QString>

class Comast::Lens {
    public:
        QString id() { return m_Name; }
        QString model() { return m_Model; }
        QString vendor() { return m_Vendor; }
        double factor() { return m_Factor; }
        void setLens( QString _name, QString _model, QString _vendor, double _factor );
    private:
        QString m_Name, m_Model, m_Vendor;
        double m_Factor;
};
#endif
