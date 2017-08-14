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

#ifndef HIPSMANAGER_H
#define HIPSMANAGER_H

#include "urlfiledownload.h"
#include "hips.h"
#include "pixcache.h"

#include <QObject>

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

  pixCacheKey_t m_key;

signals:
  void remove(pixCacheKey_t &key);

private slots:
  void done()
  {
    emit remove(m_key);
  }
};

class HIPSManager : public QObject
{
  Q_OBJECT

public:
  static HIPSManager *Instance();

  QImage *getPix(bool allsky, int level, int pix, bool &freeImage);

  qint64 getDiscCacheSize();

  //bool parseProperties(hipsParams_t *param, const QString &filename, const QString &url = "");

  void readSources();

  void cancelAll();
  void clearDiscCache();

  PixCache *getCache();

  const QVariantMap & getCurrentSource() const { return m_currentSource; }

  QList<QVariantMap> getHIPSSources() const { return m_hipsSources; }

public slots:
    bool setCurrentSource(const QString &title);

signals:
  void sigRepaint();

private slots:
  void slotDone(QNetworkReply::NetworkError error, QByteArray &data, pixCacheKey_t &key);
  void removeTimer(pixCacheKey_t &key);

private:
  explicit HIPSManager();

  static HIPSManager * _HIPSManager;

  // Cache
  PixCache       m_cache;
  QSet <pixCacheKey_t> m_downloadMap;

  void addToMemoryCache(pixCacheKey_t &key, pixCacheItem_t *item);
  pixCacheItem_t *getCacheItem(pixCacheKey_t &key);

  // List of all sources in the database
  QList<QVariantMap> m_hipsSources;

  // Current Active Source
  QVariantMap m_currentSource;

  // Handy shortcuts
  qint64 m_uid;
  QString m_currentFormat;
  QString m_currentFrame;
  uint8_t m_currentOrder;
  uint16_t m_currentTileWidth;
  QUrl m_currentURL;
};

#endif // HIPSMANAGER_H
