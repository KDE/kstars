/*
    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "opticaltrainsettings.h"
#include <kstars_debug.h>

#include "kstars.h"
#include "kstarsdata.h"
#include "ksuserdb.h"

#include <QJsonDocument>

namespace Ekos
{

OpticalTrainSettings *OpticalTrainSettings::m_Instance = nullptr;

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
OpticalTrainSettings *OpticalTrainSettings::Instance()
{
    if (m_Instance == nullptr)
        m_Instance = new OpticalTrainSettings(KStars::Instance());

    return m_Instance;
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
void OpticalTrainSettings::release()
{
    delete (m_Instance);
    m_Instance = nullptr;
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
OpticalTrainSettings::OpticalTrainSettings(QObject *parent) : QObject(parent)
{
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
void OpticalTrainSettings::setOpticalTrainID(uint32_t id)
{
    m_TrainID = id;
    // If not in database yet, create an empty entry.
    if (KStars::Instance()->data()->userdb()->GetOpticalTrainSettings(m_TrainID, m_Settings) == false)
    {
        initSettings();
        KStars::Instance()->data()->userdb()->GetOpticalTrainSettings(m_TrainID, m_Settings);
    }
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
void OpticalTrainSettings::initSettings()
{
    auto object = QJsonObject::fromVariantMap(QVariantMap());
    KStars::Instance()->data()->userdb()->AddOpticalTrainSettings(m_TrainID,
            QJsonDocument(object).toJson(QJsonDocument::Compact));
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
void OpticalTrainSettings::setSettings(const QVariantMap &settings)
{
    m_Settings = settings;
    auto object = QJsonObject::fromVariantMap(m_Settings);
    KStars::Instance()->data()->userdb()->UpdateOpticalTrainSettings(m_TrainID,
            QJsonDocument(object).toJson(QJsonDocument::Compact));
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
QVariant OpticalTrainSettings::getOneSetting(Settings id)
{
    return m_Settings[QString::number(id)];
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
void OpticalTrainSettings::setOneSetting(Settings id, const QVariant &value)
{
    m_Settings[QString::number(id)] = value;
    auto object = QJsonObject::fromVariantMap(m_Settings);
    KStars::Instance()->data()->userdb()->UpdateOpticalTrainSettings(m_TrainID,
            QJsonDocument(object).toJson(QJsonDocument::Compact));
}
}
