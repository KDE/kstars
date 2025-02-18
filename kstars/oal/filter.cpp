/*
    SPDX-FileCopyrightText: 2009 Prakash Mohan <prakash.mohan@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "oal/filter.h"

OAL::Filter::Filter(const QString &id, const filterProperties *fp)
{
    m_Id       = id;
    m_Model    = fp->model;
    m_Vendor   = fp->vendor;
    m_Type     = fp->type;
    m_Color    = fp->color;
    m_Name     = fp->vendor + ' ' + fp->model + " - " + fp->type + ' ' + fp->color + " (" + id + ')';
    m_Offset   = fp->offset;
    m_Exposure = fp->exposure;
    m_LockedFilter = fp->lockedFilter;
    m_UseAutoFocus = fp->useAutoFocus;
    m_AbsoluteFocusPosition = fp->absFocusPos;
    m_FocusTemperature = fp->focusTemperature;
    m_FocusAltitude = fp->focusAltitude;
    m_FocusDatetime = fp->focusDatetime;
    m_FocusTicksPerTemp = fp->focusTicksPerTemp;
    m_FocusTicksPerAlt = fp->focusTicksPerAlt;
    m_Wavelength = fp->wavelength;
}
