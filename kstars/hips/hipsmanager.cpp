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

#include "hipsmanager.h"

#include <QTime>
#include <QHash>
#include <QNetworkDiskCache>
#include <QPainter>

#include "auxiliary/kspaths.h"
#include "Options.h"

static QNetworkDiskCache *g_discCache = nullptr;
static UrlFileDownload *g_download = nullptr;

static int qHash(const pixCacheKey_t &key, uint seed)
{
  return qHash(QString("%1_%2_%3").arg(key.level).arg(key.pix).arg(key.uid), seed);
}


inline bool operator==(const pixCacheKey_t &k1, const pixCacheKey_t &k2)
{
  return (k1.uid == k2.uid) && (k1.level == k2.level) && (k1.pix == k2.pix);
}

HiPSManager::HiPSManager()
{
}

void HiPSManager::init()
{
  if (g_discCache == nullptr)
  {
    g_discCache = new QNetworkDiskCache();
  }

  if (g_download == nullptr)
  {
    g_download = new UrlFileDownload(this, g_discCache);

    connect(g_download, SIGNAL(sigDownloadDone(QNetworkReply::NetworkError,QByteArray&,pixCacheKey_t&)),
                  this, SLOT(slotDone(QNetworkReply::NetworkError,QByteArray&,pixCacheKey_t&)));
  }

  //g_discCache->setCacheDirectory(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/cache/hips");
  g_discCache->setCacheDirectory(KSPaths::writableLocation(QStandardPaths::GenericCacheLocation) + "hips");
  //g_discCache->setMaximumCacheSize(setting("hips_net_cache").toLongLong());
  //m_cache.setMaxCost(setting("hips_mem_cache").toInt());
  g_discCache->setMaximumCacheSize(Options::hips_net_cache()*1024*1024);
  m_cache.setMaxCost(Options::hips_mem_cache()*1024*1024);
}

/*QVariant HiPSManager::setting(const QString &name)
{
  QSettings set;

  if (name == "hips_mem_cache")
  {
    return set.value("hips_mem_cache", 300 * 1024 * 1024);
  }

  if (name == "hips_net_cache")
  {
    return set.value("hips_net_cache", 1000 * 1024 * 1024);
  }

  return QVariant();
}

void HiPSManager::writeSetting(const QString &name, const QVariant &value)
{
  QSettings set;

  qDebug() << name << value;


  if (name == "hips_mem_cache")
  {
    set.setValue("hips_mem_cache", value);
  }

  if (name == "hips_net_cache")
  {
    set.setValue("hips_net_cache", value);
  }
}
*/

void HiPSManager::setParam(const hipsParams_t &param)
{  
  m_param = param;
  m_uid = qHash(param.url);  
}

QImage *HiPSManager::getPix(bool allsky, int level, int pix, bool &freeImage)
{  
  int origPix = pix;  
  freeImage = false;

  if (allsky)
  {
    level = 0;
    pix = 0;
  }

  pixCacheKey_t key;

  key.level = level;
  key.pix = pix;
  key.uid = m_uid;

  pixCacheItem_t *item = getCacheItem(key);

  if (m_downloadMap.contains(key))
  { // downloading

    // try render (level - 1) while downloading
    key.level = level - 1;
    key.pix = pix / 4;
    pixCacheItem_t *item = getCacheItem(key);

    if (item)
    {
      QImage *cacheImage = item->image;
      int size = m_param.tileWidth >> 1;
      int offset = cacheImage->width() / size;
      QImage *image = cacheImage;

      int index[4] = {0, 2, 1, 3};

      int ox = index[pix % 4] % offset;
      int oy = index[pix % 4] / offset;

      QImage *newImage = new QImage(image->copy(ox * size, oy * size, size, size));
      freeImage = true;
      return newImage;
    }        
    return nullptr;
  }    

  if (item)
  {        
    QImage *cacheImage = item->image;

    Q_ASSERT(!item->image->isNull());

    if (allsky && cacheImage != nullptr)
    { // all sky
      int size = 64;
      int offset = cacheImage->width() / size;
      QImage *image = cacheImage;

      int ox = origPix % offset;
      int oy = origPix / offset;

      QImage *newImage = new QImage(image->copy(ox * size, oy * size, size, size));
      freeImage = true;
      return newImage;
    }

    return cacheImage;
  }

  QString path;          

  if (!allsky)
  {
    int dir = (pix / 10000) * 10000;
    path = "/Norder" + QString::number(level) + "/Dir" + QString::number(dir) + "/Npix" + QString::number(pix) + "." + m_param.imageExtension;
  }
  else
  {
    path = "/Norder3/Allsky." + m_param.imageExtension;
  } 

  g_download->begin(m_param.url + path, key);
  m_downloadMap.insert(key);    

  return nullptr; 
}

qint64 HiPSManager::getDiscCacheSize()
{
  return g_discCache->cacheSize();
}

bool HiPSManager::parseProperties(hipsParams_t *param, const QString &filename, const QString &url)
{
  QFile f(filename);

  if (!f.open(QFile::ReadOnly | QFile::Text))
  {
    qDebug() << "nf" << f.fileName();
    return false;
  }

  QMap <QString, QString> map;
  QTextStream in(&f);
  while (!in.atEnd())
  {
    QString line = in.readLine();

    int index = line.indexOf("=");
    if (index > 0)
    {
      map[line.left(index).simplified()] = line.mid(index + 1).simplified();
    }
  }

  param->url = url;
  qDebug() << url;

  int count = 0;
  QString tmp;

  if (map.contains("obs_collection"))
  {
    param->name = map["obs_collection"];
    count++;
  }

  if (map.contains("hips_tile_width"))
  {
    param->tileWidth = map["hips_tile_width"].toInt();
    count++;
  }

  if (map.contains("hips_order"))
  {
    param->max_level = map["hips_order"].toInt();
    count++;
  }

  if (map.contains("hips_tile_format"))
  {
    tmp = map["hips_tile_format"];

    QStringList list = tmp.split(" ");

    if (list.contains("jpeg"))
    {
      param->imageExtension = "jpg";
      count++;
    }
    else if (list.contains("png"))
    {
      param->imageExtension = "png";
      count++;
    }
  }

  if (map.contains("hips_frame") || map.contains("ohips_frame"))
  {
    if (map.contains("hips_frame"))
      tmp = map["hips_frame"];
    else
      tmp = map["ohips_frame"];

    if (tmp == "equatorial")
    {
      param->frame = HIPS_FRAME_EQT;
      count++;
    }
    else if (tmp == "galactic")
    {
      param->frame = HIPS_FRAME_GAL;
      count++;
    }
  }

  return count == 5; // all items have been loaded
}

void HiPSManager::cancelAll()
{
  g_download->abortAll();
}

void HiPSManager::clearDiscCache()
{
  g_discCache->clear();
}

void HiPSManager::slotDone(QNetworkReply::NetworkError error, QByteArray &data, pixCacheKey_t &key)
{    
  if (error == QNetworkReply::NoError)
  {
    m_downloadMap.remove(key);

    pixCacheItem_t *item = new pixCacheItem_t;

    item->image = new QImage();
    if (item->image->loadFromData(data))
    {      
      addToMemoryCache(key, item);

      emit sigRepaint();
    }
    else
    {
      qDebug() << "no image" << data;
    }
  }
  else
  {
    if (error == QNetworkReply::OperationCanceledError)
    {
      m_downloadMap.remove(key);
    }
    else
    {
      RemoveTimer *timer = new RemoveTimer();
      timer->setKey(key);
      connect(timer, SIGNAL(remove(pixCacheKey_t&)), this, SLOT(removeTimer(pixCacheKey_t&)));
    }
  }
}

void HiPSManager::removeTimer(pixCacheKey_t &key)
{  
  m_downloadMap.remove(key);
  sender()->deleteLater();
  emit sigRepaint();
}

hipsParams_t *HiPSManager::getParam()
{
  return &m_param;
}

PixCache *HiPSManager::getCache()
{
  return &m_cache;
}

void HiPSManager::addToMemoryCache(pixCacheKey_t &key, pixCacheItem_t *item)
{    
  Q_ASSERT(item);
  Q_ASSERT(item->image);

  int cost = item->image->byteCount();
  m_cache.add(key, item, cost);
}

pixCacheItem_t *HiPSManager::getCacheItem(pixCacheKey_t &key)
{  
  return m_cache.get(key);
}

void RemoveTimer::setKey(const pixCacheKey_t &key)
{
  m_key = key;
}
