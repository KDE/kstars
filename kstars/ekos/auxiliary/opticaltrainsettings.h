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
            Analyze,
            DarkLibrary
        } Settings;

        /**
         * @brief setOpticalTrainID This must be called before calling any settings functions below.
         * @param id ID of optical train
         */
        void setOpticalTrainID(uint32_t id);

        /**
         * @brief setOneSetting Save the settings of a module for the optical train. The module's metadata is kept.
         */
        void setOneSetting(Settings id, const QVariant &value);
        /**
         * @brief getOneSetting The settings of a module for the optical train, without its metadata. Invalid if there
         * are none.
         */
        QVariant getOneSetting(Settings id);

        /**
         * Metadata of a module: what a module keeps for an optical train besides its settings, e.g. the tilt plate of
         * Focus, or the in-focus HFR of its recent runs. It is stored with the module's settings under METADATA_KEY,
         * which getOneSetting() leaves out and setOneSetting() keeps: a module saving its settings never loses it, it
         * never becomes a setting or an option, and copying the settings of one train to another leaves it behind.
         */
        static constexpr const char *METADATA_KEY = "_metadata";

        /** @brief getMetadata One entry of the metadata of a module for the optical train, invalid if there is none. */
        QVariant getMetadata(Settings id, const QString &name);
        /**
         * @brief setMetadata Save one entry of the metadata of a module for the optical train, at once.
         * @param value the entry, an invalid value removes it
         */
        void setMetadata(Settings id, const QString &name, const QVariant &value);

        /** The settings of a module without its metadata, invalid if nothing else is left. */
        static QVariant withoutMetadata(const QVariant &value);
        /** New settings of a module with the metadata of the stored ones, unless the new ones carry their own. */
        static QVariant withMetadata(const QVariant &value, const QVariant &stored);

        void initSettings();
        void setSettings(const QVariantMap &settings);
        const QVariantMap &getSettings() const
        {
            return m_Settings;
        }

    Q_SIGNALS:
        void updated();

    private:

        OpticalTrainSettings(QObject *parent = nullptr);
        static OpticalTrainSettings *m_Instance;

        void save();

        uint32_t m_TrainID {0};
        QVariantMap m_Settings;
};

}
