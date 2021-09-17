/*
    SPDX-FileCopyrightText: 2004 Jasem Mutlaq
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later

    Some code fragments were adapted from Peter Kirchgessner's FITS plugin
    SPDX-FileCopyrightText: Peter Kirchgessner <http://members.aol.com/pkirchg>
*/

#pragma once

#include <QObject>
#include <QHash>
#include <QStandardItem>
#include <QFuture>

#include "fitsdata.h"

class FITSData;

class Edge
{
    public:
        virtual ~Edge() = default;
        float x {0};
        float y {0};
        int val {0};
        int scanned {0};
        float width {0};
        float HFR {-1};
        float sum {0};
        float numPixels {0};
        float ellipticity {0};
};

class BahtinovEdge : public Edge
{
    public:
        virtual ~BahtinovEdge() = default;
        QVector<QLineF> line;
        QPointF offset;
};

class FITSStarDetector : public QObject
{
        Q_OBJECT

    public:
        /** @brief Instantiate a detector for a FITS data file.
         */
        explicit FITSStarDetector(FITSData *data): QObject(), m_ImageData(data) {};

        /** @brief Find sources in the parent FITS data file.
         * @param starCenters is the list of sources to append to.
         * @param boundary is the rectangle in which to find sources, by default the full frame.
         * @return The number of sources detected by the procedure.
         */
        virtual QFuture<bool> findSources(QRect const &boundary = QRect()) = 0;

        /** @brief Configure the detection method.
         * @param setting is the name of a detection setting.
         * @param value is the value of the detection setting identified by 'setting'.
         * @return The detector as a chain to call the overridden findSources.
         */
        virtual void configure(const QString &key, const QVariant &value);

        void setSettings(const QVariantMap &settings)
        {
            m_Settings = settings;
        }
        QVariant getValue(const QString &key, QVariant defaultValue = QVariant()) const;

        /** @brief Helper to configure the detection method from a data model.
         * @param settings is the list of key/value pairs for the method to use settings from.
         * @note Data model 'settings' is considered a key/value list, using column 1 text as case-insensitive keys and column 2 data as values.
         * @return The detector as a chain to call the overridden findSources.
         */
        //void configure(QStandardItemModel const &settings);

    protected:
        FITSData *m_ImageData {nullptr};
        QVariantMap m_Settings;
};

