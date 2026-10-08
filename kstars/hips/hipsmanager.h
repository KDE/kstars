/*
    SPDX-FileCopyrightText: 2015-2017 Pavel Mraz

    SPDX-FileCopyrightText: 2017 Jasem Mutlaq

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "hips.h"
#include "opships.h"
#include "pixcache.h"
#include "urlfiledownload.h"

#include <QObject>

#include <memory>

class RemoveTimer : public QTimer
{
        Q_OBJECT
    public:
        RemoveTimer()
        {
            connect(this, SIGNAL(timeout()), this, SLOT(done()));
            start(5000);
        }

        void setKey(const pixCacheKey_t &key);

        pixCacheKey_t m_key { 0, 0, 0 };

    Q_SIGNALS:
        void remove(pixCacheKey_t &key);

    private Q_SLOTS:
        void done()
        {
            Q_EMIT remove(m_key);
            deleteLater();
        }
};

class HIPSManager : public QObject
{
        Q_OBJECT

    public:
        static HIPSManager *Instance();

        typedef enum { HIPS_EQUATORIAL_FRAME, HIPS_GALACTIC_FRAME, HIPS_OTHER_FRAME } HIPSFrame;

        QImage *getPix(bool allsky, int level, int pix, bool &freeImage);

        void readSources();

        void cancelAll();
        void clearDiscCache();

        // Getters
        const QMap<QString, QString> &getCurrentSource() const
        {
            return m_currentSource;
        }
        const QList<QMap<QString, QString>> &getHIPSSources() const
        {
            return m_hipsSources;
        }
        int getUsableLevel(int level) const;
        int getUsableOfflineLevel(int level) const;
        PixCache *getCache();
        qint64 getDiscCacheSize() const;
        const QString &getCurrentFormat() const
        {
            return m_currentFormat;
        }
        HIPSFrame getCurrentFrame() const
        {
            return m_currentFrame;
        }
        const uint8_t &getCurrentOrder() const
        {
            return m_currentOrder;
        }
        const uint16_t &getCurrentTileWidth() const
        {
            return m_currentTileWidth;
        }
        const QUrl &getCurrentURL() const
        {
            return m_currentURL;
        }
        qint64 getUID() const
        {
            return m_uid;
        }
        /**
         * @brief setOfflineLevels Replace the offline level map from the given directory names.
         * @param value Directory entries of the offline storage path (e.g. "Norder3", "Norder5").
         */
        void setOfflineLevels(const QStringList &value);

        /**
         * @brief loadOfflineLevels Scan an offline HiPS storage directory and update the offline level map.
         * Logs a warning if the offline source is enabled but the directory is missing or has no Norder* levels.
         * @param path Offline HiPS storage path.
         */
        void loadOfflineLevels(const QString &path);

        /**
         * @brief computeOfflineLevels Map every HiPS order 0-20 to an order that is available offline.
         * A missing order is mapped to the next higher available one, or to the highest available one.
         * @param directories Directory entries of the offline storage path. Only "Norder<N>" entries are used.
         * @return Map of requested order to usable order. If no order is available, all orders map to 1.
         */
        static QMap<int, int> computeOfflineLevels(const QStringList &directories);

    public Q_SLOTS:
        bool setCurrentSource(const QString &title);
        void showSettings();

    Q_SIGNALS:
        void sigRepaint();

    private Q_SLOTS:
        void slotDone(QNetworkReply::NetworkError error, QByteArray &data, pixCacheKey_t &key);
        void slotApply();
        void removeTimer(pixCacheKey_t &key);

    private:
        HIPSManager();

        static HIPSManager * _HIPSManager;

        // Cache
        PixCache m_cache;
        QSet <pixCacheKey_t> m_downloadMap;

        void addToMemoryCache(pixCacheKey_t &key, pixCacheItem_t *item);
        pixCacheItem_t *getCacheItem(pixCacheKey_t &key);

        // List of all sources in the database
        QList<QMap<QString, QString>> m_hipsSources;

        // Current Active Source
        QMap<QString, QString> m_currentSource;

        std::unique_ptr<OpsHIPS> sourceSettings;
        std::unique_ptr<OpsHIPSCache> cacheSettings;
        std::unique_ptr<OpsHIPSDisplay> displaySettings;

        // Handy shortcuts
        qint64 m_uid { 0 };
        QString m_currentFormat;
        HIPSFrame m_currentFrame { HIPS_OTHER_FRAME };
        uint8_t m_currentOrder { 0 };
        uint16_t m_currentTileWidth { 0 };
        QUrl m_currentURL;
        QMap<int, int> m_OfflineLevelsMap;
};
