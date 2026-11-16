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
#include <QJsonObject>

namespace Ekos
{

namespace
{
// Module settings are maps: as the module passed them, or as read back from the database
bool isMap(const QVariant &value)
{
    return value.typeId() == QMetaType::QVariantMap || value.typeId() == QMetaType::QJsonObject;
}

QVariantMap toMap(const QVariant &value)
{
    return value.typeId() == QMetaType::QJsonObject ? value.toJsonObject().toVariantMap() : value.toMap();
}
}

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
    save();
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
QVariant OpticalTrainSettings::getOneSetting(Settings id)
{
    return withoutMetadata(m_Settings[QString::number(id)]);
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
void OpticalTrainSettings::setOneSetting(Settings id, const QVariant &value)
{
    const QString key = QString::number(id);
    m_Settings[key] = withMetadata(value, m_Settings.value(key));
    save();
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
QVariant OpticalTrainSettings::getMetadata(Settings id, const QString &name)
{
    return toMap(toMap(m_Settings.value(QString::number(id))).value(METADATA_KEY)).value(name);
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
void OpticalTrainSettings::setMetadata(Settings id, const QString &name, const QVariant &value)
{
    const QString key = QString::number(id);
    QVariantMap settings = toMap(m_Settings.value(key));
    QVariantMap metadata = toMap(settings.value(METADATA_KEY));
    if (value.isValid())
        metadata[name] = value;
    else
        metadata.remove(name);

    if (metadata.isEmpty())
        settings.remove(METADATA_KEY);
    else
        settings[METADATA_KEY] = metadata;

    if (settings.isEmpty())
        m_Settings.remove(key);
    else
        m_Settings[key] = settings;
    save();
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
QVariant OpticalTrainSettings::withoutMetadata(const QVariant &value)
{
    if (!isMap(value))
        return value;
    QVariantMap settings = toMap(value);
    if (!settings.contains(METADATA_KEY))
        return value;
    settings.remove(METADATA_KEY);
    return settings.isEmpty() ? QVariant() : QVariant(settings);
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
QVariant OpticalTrainSettings::withMetadata(const QVariant &value, const QVariant &stored)
{
    const QVariant metadata = isMap(stored) ? toMap(stored).value(METADATA_KEY) : QVariant();
    if (!metadata.isValid())
        return value;

    // Settings that carry their own metadata keep it; anything else than settings cannot carry it
    QVariantMap settings = isMap(value) ? toMap(value) : QVariantMap();
    if (settings.contains(METADATA_KEY) || (value.isValid() && !isMap(value)))
        return value;
    settings[METADATA_KEY] = metadata;
    return settings;
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
void OpticalTrainSettings::save()
{
    auto object = QJsonObject::fromVariantMap(m_Settings);
    KStars::Instance()->data()->userdb()->UpdateOpticalTrainSettings(m_TrainID,
            QJsonDocument(object).toJson(QJsonDocument::Compact));
}
}
