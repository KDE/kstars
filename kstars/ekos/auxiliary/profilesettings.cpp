/*
    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "profilesettings.h"
#include <kstars_debug.h>

#include "kstars.h"
#include "kstarsdata.h"
#include "ksuserdb.h"

#include <QJsonDocument>

namespace Ekos
{

ProfileSettings *ProfileSettings::m_Instance = nullptr;

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
ProfileSettings *ProfileSettings::Instance()
{
    if (m_Instance == nullptr)
        m_Instance = new ProfileSettings(KStars::Instance());

    return m_Instance;
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
void ProfileSettings::release()
{
    delete (m_Instance);
    m_Instance = nullptr;
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
ProfileSettings::ProfileSettings(QObject *parent) : QObject(parent)
{
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
void ProfileSettings::setProfile(const QSharedPointer<ProfileInfo> &profile)
{
    m_Profile = profile;
    // If not in database yet, create an empty entry.
    if (KStars::Instance()->data()->userdb()->GetProfileSettings(m_Profile->id, m_Settings) == false)
    {
        initSettings();
        KStars::Instance()->data()->userdb()->GetProfileSettings(m_Profile->id, m_Settings);
    }
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
void ProfileSettings::initSettings()
{
    auto object = QJsonObject::fromVariantMap(QVariantMap());
    KStars::Instance()->data()->userdb()->AddProfileSettings(m_Profile->id,
            QJsonDocument(object).toJson(QJsonDocument::Compact));
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
void ProfileSettings::setSettings(const QVariantMap &settings)
{
    m_Settings = settings;
    auto object = QJsonObject::fromVariantMap(m_Settings);
    KStars::Instance()->data()->userdb()->UpdateProfileSettings(m_Profile->id,
            QJsonDocument(object).toJson(QJsonDocument::Compact));
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
QVariant ProfileSettings::getOneSetting(Settings id)
{
    return m_Settings[QString::number(id)];
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
void ProfileSettings::setOneSetting(Settings id, const QVariant &value)
{
    m_Settings[QString::number(id)] = value;
    auto object = QJsonObject::fromVariantMap(m_Settings);
    KStars::Instance()->data()->userdb()->UpdateProfileSettings(m_Profile->id,
            QJsonDocument(object).toJson(QJsonDocument::Compact));
}
}
