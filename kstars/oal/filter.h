/***************************************************************************
                          filter.h  -  description

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
 * @class OAL::Filter
 *
 * Information of user filters
 */
class OAL::Filter
{
    public:
        Filter(const QString &id, const QString &model, const QString &vendor, const QString &type, const QString &color,
               double exposure, int offset, bool useAutoFocus, const QString &lockedFilter, int absFocusPosition);

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
        QString type() const
        {
            return m_Type;
        }
        QString color() const
        {
            return m_Color;
        }

        // Additional fields used by Ekos
        int offset() const
        {
            return m_Offset;
        }
        void setOffset(int _offset)
        {
            m_Offset = _offset;
        }

        double exposure() const
        {
            return m_Exposure;
        }
        void setExposure(double _exposure)
        {
            m_Exposure = _exposure;
        }

        QString lockedFilter() const
        {
            return m_LockedFilter;
        }
        void setLockedFilter(const QString &_filter)
        {
            m_LockedFilter = _filter;
        }

        bool useAutoFocus() const
        {
            return m_UseAutoFocus;
        }
        void setUseAutoFocus(bool enabled)
        {
            m_UseAutoFocus = enabled;
        }

        int absoluteFocusPosition()
        {
            return m_AbsoluteFocusPosition;
        }
        void setAbsoluteFocusPosition(int newAbsFocusPos)
        {
            m_AbsoluteFocusPosition = newAbsFocusPos;
        }

    private:
        QString m_Id, m_Model, m_Vendor, m_Type, m_Color, m_Name, m_LockedFilter;
        int m_Offset { 0 };
        int m_AbsoluteFocusPosition { 0 };
        double m_Exposure { 1.0 };
        bool m_UseAutoFocus { false };
};
