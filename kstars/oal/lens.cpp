/*
    SPDX-FileCopyrightText: 2009 Prakash Mohan <prakash.mohan@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "oal/lens.h"

void OAL::Lens::setLens(const QString &_id, const QString &_model, const QString &_vendor, double _factor)
{
    m_Id     = _id;
    m_Model  = _model;
    m_Vendor = _vendor;
    m_Factor = _factor;
    if (_factor > 1)
        m_Name = _vendor + ' ' + _model + " - " + QString::number(_factor) + "x Barlow (" + _id + ')';
    else
        m_Name = _vendor + ' ' + _model + " - " + QString::number(_factor) + "x Focal Reducer (" + _id + ')';
}
