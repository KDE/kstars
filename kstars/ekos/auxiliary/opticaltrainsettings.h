/*
    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QJsonArray>
#include <QObject>
#include <QVariantMap>

namespace Ekos
{

/**
 * @brief The OpticalTrainSettings class
 *
 * General-purpose class to encapsulate OpticalTrain-specific settings in the database.
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
class OpticalTrainSettings : public QObject
{
        Q_OBJECT

    public:

        static OpticalTrainSettings *Instance();
        static void release();

        // Settings
        typedef enum
        {
            Capture,
            Focus,
            Mount,
            Align,
            Guide,
            Observatory,
            Scheduler,
            Analyze
        } Settings;

        /**
         * @brief setOpticalTrainID This must be called before calling any settings functions below.
         * @param id ID of optical train
         */
        void setOpticalTrainID(uint32_t id);

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

        OpticalTrainSettings(QObject *parent = nullptr);
        static OpticalTrainSettings *m_Instance;

        uint32_t m_TrainID {0};
        QVariantMap m_Settings;
};

}
