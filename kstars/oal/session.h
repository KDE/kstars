/*
    SPDX-FileCopyrightText: 2009 Prakash Mohan <prakash.mohan@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "oal/oal.h"
#include "kstarsdatetime.h"

#include <QString>

/**
 * @class OAL::Session
 *
 * Information on session details.
 */
class OAL::Session
{
    public:
        Session(const QString &name, const QString &site, const KStarsDateTime &begin, const KStarsDateTime &end,
                const QString &weather, const QString &equipment, const QString &comment, const QString &lang)
        {
            setSession(name, site, begin, end, weather, equipment, comment, lang);
        }
        QString id() const
        {
            return m_Id;
        }
        QString site() const
        {
            return m_Site;
        }
        KStarsDateTime begin() const
        {
            return m_Begin;
        }
        KStarsDateTime end() const
        {
            return m_End;
        }
        QString weather() const
        {
            return m_Weather;
        }
        QString equipment() const
        {
            return m_Equipment;
        }
        QString comments() const
        {
            return m_Comment;
        }
        QString lang() const
        {
            return m_Lang;
        }
        void setSession(const QString &_name, const QString &_site, const KStarsDateTime &_begin,
                        const KStarsDateTime &_end, const QString &_weather, const QString &_equipment,
                        const QString &_comment, const QString &_lang = "en");

    private:
        QString m_Site, m_Weather, m_Equipment, m_Comment, m_Lang, m_Id;
        KStarsDateTime m_Begin, m_End;
};
