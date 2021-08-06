/*
  Copyright (C) 2015-2017, Pavel Mraz

  Copyright (C) 2017, Jasem Mutlaq

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
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

    signals:
        void remove(pixCacheKey_t &key);

    private slots:
        void done()
        {
            emit remove(m_key);
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

    public slots:
        bool setCurrentSource(const QString &title);
        void showSettings();

    signals:
        void sigRepaint();

    private slots:
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
};
