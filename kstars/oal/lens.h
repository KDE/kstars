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
