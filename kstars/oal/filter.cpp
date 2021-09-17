/*
    SPDX-FileCopyrightText: 2009 Prakash Mohan <prakash.mohan@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "oal/filter.h"

OAL::Filter::Filter(const QString &id, const QString &model, const QString &vendor, const QString &type, const QString &color,
                          double exposure, int offset, bool useAutoFocus, const QString &lockedFilter, int absFocusPosition)
{
    m_Id       = id;
    m_Model    = model;
    m_Vendor   = vendor;
    m_Type     = type;
    m_Color    = color;
    m_Name     = vendor + ' ' + model + " - " + type + ' ' + color + " (" + id + ')';
    m_Offset   = offset;
    m_Exposure = exposure;
    m_LockedFilter = lockedFilter;
    m_UseAutoFocus = useAutoFocus;
    m_AbsoluteFocusPosition = absFocusPosition;
}
