/*
    SPDX-FileCopyrightText: 2009 Prakash Mohan <prakash.mohan@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
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
