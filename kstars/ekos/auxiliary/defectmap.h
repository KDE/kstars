/*
    SPDX-FileCopyrightText: 2021 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <set>
#include <QJsonObject>
#include <QJsonArray>

#include "fitsviewer/fitsdata.h"

class BadPixel
{
    public:
        BadPixel() {};
        explicit BadPixel(uint16_t _x, uint16_t _y, double _value) : x(_x), y(_y), value(_value) {}
        bool operator<(const BadPixel &rhs) const
        {
            return value < rhs.value;
        }
        QJsonObject json() const;
        uint16_t x{0}, y{0};
        double value {0};
};

typedef std::multiset<BadPixel> BadPixelSet;

class DefectMap : public QObject
{
        Q_OBJECT

        // Hot Pixels Aggressiveness
        Q_PROPERTY(int HotPixelAggressiveness MEMBER m_HotPixelsAggressiveness)
        // Cold Pixels Aggressiveness
        Q_PROPERTY(int ColdPixelAggressiveness MEMBER m_ColdPixelsAggressiveness)
        // Hot Pixels Enabled
        Q_PROPERTY(bool HotEnabled READ hotEnabled WRITE setHotEnabled)
        // Cold Pixels Enabled
        Q_PROPERTY(bool ColdEnabled READ coldEnabled WRITE setColdEnabled)

    public:
        DefectMap();

        void setHotEnabled(bool enabled);
        void setColdEnabled(bool enabled);
        void setDarkData(const QSharedPointer<FITSData> &data);
        bool load(const QString &filename);
        bool save(const QString &filename, const QString &camera);
        void initBadPixels();

        const BadPixelSet &hotPixels() const
        {
            return m_HotPixels;
        }
        const BadPixelSet &coldPixels() const
        {
            return m_ColdPixels;
        }

        bool hotEnabled() const
        {
            return m_HotEnabled;
        }
        bool coldEnabled() const
        {
            return m_ColdEnabled;
        }
        const QString &filename() const
        {
            return m_Filename;
        }

        const BadPixelSet::const_iterator hotThreshold() const
        {
            return (m_HotEnabled ? m_HotPixelsThreshold : m_HotPixels.end());
        }
        const BadPixelSet::const_iterator coldThreshold() const
        {
            return (m_ColdEnabled ? m_ColdPixelsThreshold : m_ColdPixels.begin());
        }
        uint32_t hotCount() const
        {
            return m_HotPixelsCount;
        }
        uint32_t coldCount() const
        {
            return m_ColdPixelsCount;
        }

        void filterPixels();
    signals:
        //        void hotPixelsUpdated(const BadPixelSet::const_iterator &start, const BadPixelSet::const_iterator &end);
        //        void coldPixelsUpdated(const BadPixelSet::const_iterator &start, const BadPixelSet::const_iterator &end);
        void pixelsUpdated(uint32_t hotPixelsCount, uint32_t coldPixelsCount);

    private:
        double getHotThreshold(uint8_t aggressiveness);
        double getColdThreshold(uint8_t aggressiveness);
        double calculateSigma(uint8_t aggressiveness);
        template <typename T>
        void initBadPixelsInternal(double hotPixelThreshold, double coldPixelThreshold);

        BadPixelSet m_ColdPixels, m_HotPixels;
        BadPixelSet::const_iterator m_ColdPixelsThreshold, m_HotPixelsThreshold;
        uint8_t m_HotPixelsAggressiveness {75}, m_ColdPixelsAggressiveness {75};
        uint32_t m_HotPixelsCount {0}, m_ColdPixelsCount {0};
        double m_HotSigma {0}, m_ColdSigma {0};
        double m_Median {0}, m_StandardDeviation {0};
        bool m_HotEnabled {true}, m_ColdEnabled {true};
        QString m_Filename, m_Camera;

        QSharedPointer<FITSData> m_DarkData;

};

