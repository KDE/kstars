/*
    SPDX-FileCopyrightText: 2009 Prakash Mohan <prakash.mohan@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "oal/session.h"

void OAL::Session::setSession(const QString &_name, const QString &_site, const KStarsDateTime &_begin,
                              const KStarsDateTime &_end, const QString &_weather, const QString &_equipment,
                              const QString &_comment, const QString &_lang)
{
    m_Site      = _site;
    m_Begin     = _begin;
    m_End       = _end;
    m_Weather   = _weather;
    m_Equipment = _equipment;
    m_Comment   = _comment;
    m_Lang      = _lang;
    m_Id        = _name;
}
