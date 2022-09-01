/*
    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "profileinfo.h"

#include <QJsonArray>
#include <QObject>
#include <QSharedPointer>
#include <QVariantMap>

namespace Ekos
{

/**
 * @brief The ProfileSettings class
 *
 * General-purpose class to encapsulate profile-specific settings in the database.
 *
 * The settings are stored as QVariant Map. Each element in the array should have the following format.
 * {
 *     "id1": payload1,
 *     "id2", payload2,
 *     ...
 * }
 *
 * 1. The ID is a unique number from the Settings enum.
 * 2. The Payload is QVariant.
 *
 * @author Jasem Mutlaq
 * @version 1.0
 */
class ProfileSettings : public QObject
{
        Q_OBJECT

    public:

        static ProfileSettings *Instance();
        static void release();

        // Settings
        typedef enum
        {
            PrimaryOpticalTrain,

            CaptureOpticalTrain,
            FocusOpticalTrain,
            MountOpticalTrain,
            GuideOpticalTrain,
            AlignOpticalTrain,
            DarkLibraryOpticalTrain,
        } Settings;

        void setProfile(const QSharedPointer<ProfileInfo> &profile);

        void setOneSetting(Settings id, const QVariant &value);
        QVariant getOneSetting(Settings id);
        void initSettings();
        void setSettings(const QVariantMap &settings);
        const QVariantMap &getSettings() const
        {
            return m_Settings;
        }

    signals:
        void updated();

    private:

        ProfileSettings(QObject *parent = nullptr);
        static ProfileSettings *m_Instance;

        QSharedPointer<ProfileInfo> m_Profile;
        QVariantMap m_Settings;
};

}
