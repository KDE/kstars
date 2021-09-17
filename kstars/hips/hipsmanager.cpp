/*
    SPDX-FileCopyrightText: 2015-2017 Pavel Mraz

    SPDX-FileCopyrightText: 2017 Jasem Mutlaq

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "hipsmanager.h"

#include "auxiliary/kspaths.h"
#include "auxiliary/ksuserdb.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "kstars_debug.h"
#include "Options.h"
#include "skymap.h"

#include <KConfigDialog>

#include <QTime>
#include <QHash>
#include <QNetworkDiskCache>
#include <QPainter>

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

HIPSManager * HIPSManager::_HIPSManager = nullptr;

HIPSManager *HIPSManager::Instance()
{
    if (_HIPSManager == nullptr)
        _HIPSManager = new HIPSManager();

    return _HIPSManager;
}

HIPSManager::HIPSManager() : QObject(KStars::Instance())
{
    if (g_discCache == nullptr)
    {
        g_discCache = new QNetworkDiskCache();
    }

    if (g_download == nullptr)
    {
        g_download = new UrlFileDownload(this, g_discCache);

        connect(g_download, SIGNAL(sigDownloadDone(QNetworkReply::NetworkError, QByteArray &, pixCacheKey_t &)),
                this, SLOT(slotDone(QNetworkReply::NetworkError, QByteArray &, pixCacheKey_t &)));
    }

    g_discCache->setCacheDirectory(QDir(KSPaths::writableLocation(QStandardPaths::CacheLocation)).filePath("hips"));
    qint64 net = Options::hIPSNetCache();
    qint64 value = net * 1024 * 1024;
    g_discCache->setMaximumCacheSize(Options::hIPSNetCache() * 1024 * 1024);
    value = Options::hIPSMemoryCache() * 1024 * 1024;
    m_cache.setMaxCost(Options::hIPSMemoryCache() * 1024 * 1024);

}

void HIPSManager::showSettings()
{
    KConfigDialog *dialog = KConfigDialog::exists("hipssettings");
    if (dialog == nullptr)
    {
        dialog = new KConfigDialog(KStars::Instance(), "hipssettings", Options::self());
        connect(dialog->button(QDialogButtonBox::Apply), SIGNAL(clicked()), SLOT(slotApply()));
        connect(dialog->button(QDialogButtonBox::Ok), SIGNAL(clicked()), SLOT(slotApply()));

        displaySettings.reset(new OpsHIPSDisplay());
        KPageWidgetItem *page = dialog->addPage(displaySettings.get(), i18n("Display"));
        page->setIcon(QIcon::fromTheme("computer"));

        cacheSettings.reset(new OpsHIPSCache());
        page = dialog->addPage(cacheSettings.get(), i18n("Cache"));
        page->setIcon(QIcon::fromTheme("preferences-web-browser-cache"));

        sourceSettings.reset(new OpsHIPS());
        page = dialog->addPage(sourceSettings.get(), i18n("Sources"));
        page->setIcon(QIcon::fromTheme("view-preview"));

        dialog->resize(800, 600);
    }

    dialog->show();
}

void HIPSManager::slotApply()
{
    readSources();
    KStars::Instance()->repopulateHIPS();
    SkyMap::Instance()->forceUpdate();
}

qint64 HIPSManager::getDiscCacheSize() const
{
    return g_discCache->cacheSize();
}

void HIPSManager::readSources()
{
    KStarsData::Instance()->userdb()->GetAllHIPSSources(m_hipsSources);

    QString currentSourceTitle = Options::hIPSSource();

    setCurrentSource(currentSourceTitle);
}

/*void HIPSManager::setParam(const hipsParams_t &param)
{
  m_param = param;
  m_uid = qHash(param.url);
}*/

QImage *HIPSManager::getPix(bool allsky, int level, int pix, bool &freeImage)
{
    if (m_currentSource.isEmpty())
    {
        qCWarning(KSTARS) << "HIPS source not available!";
        return nullptr;
    }

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
    {
        // downloading

        // try render (level - 1) while downloading
        key.level = level - 1;
        key.pix = pix / 4;
        pixCacheItem_t *item = getCacheItem(key);

        if (item != nullptr)
        {
            QImage *cacheImage = item->image;
            int size = m_currentTileWidth >> 1;
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

    if (item != nullptr)
    {
        QImage *cacheImage = item->image;

        Q_ASSERT(!item->image->isNull());

        if (allsky && cacheImage != nullptr)
        {
            // all sky
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

        path = "/Norder" + QString::number(level) + "/Dir" + QString::number(dir) + "/Npix" + QString::number(pix) +
               '.' + m_currentFormat;
    }
    else
    {
        path = "/Norder3/Allsky." + m_currentFormat;
    }

    QUrl downloadURL(m_currentURL);
    downloadURL.setPath(downloadURL.path() + path);
    g_download->begin(downloadURL, key);
    m_downloadMap.insert(key);

    return nullptr;
}


#if 0
bool HIPSManager::parseProperties(hipsParams_t *param, const QString &filename, const QString &url)
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
#endif

void HIPSManager::cancelAll()
{
    g_download->abortAll();
}

void HIPSManager::clearDiscCache()
{
    g_discCache->clear();
}

void HIPSManager::slotDone(QNetworkReply::NetworkError error, QByteArray &data, pixCacheKey_t &key)
{
    if (error == QNetworkReply::NoError)
    {
        m_downloadMap.remove(key);

        auto *item = new pixCacheItem_t;

        item->image = new QImage();
        if (item->image->loadFromData(data))
        {
            addToMemoryCache(key, item);

            //SkyMap::Instance()->forceUpdate();
        }
        else
        {
            delete item;
            qCWarning(KSTARS) << "no image" << data;
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
            auto *timer = new RemoveTimer();
            timer->setKey(key);
            connect(timer, SIGNAL(remove(pixCacheKey_t &)), this, SLOT(removeTimer(pixCacheKey_t &)));
        }
    }
}

void HIPSManager::removeTimer(pixCacheKey_t &key)
{
    m_downloadMap.remove(key);
    sender()->deleteLater();
    emit sigRepaint();
}

PixCache *HIPSManager::getCache()
{
    return &m_cache;
}

void HIPSManager::addToMemoryCache(pixCacheKey_t &key, pixCacheItem_t *item)
{
    Q_ASSERT(item);
    Q_ASSERT(item->image);

#if QT_VERSION >= QT_VERSION_CHECK(5,10,0)
    int cost = item->image->sizeInBytes();
#else
    int cost = item->image->byteCount();
#endif

    m_cache.add(key, item, cost);
}

pixCacheItem_t *HIPSManager::getCacheItem(pixCacheKey_t &key)
{
    return m_cache.get(key);
}

bool HIPSManager::setCurrentSource(const QString &title)
{
    if (title == "None")
    {
        Options::setShowHIPS(false);
        Options::setHIPSSource(title);
        m_currentSource.clear();
        m_currentFormat.clear();
        m_currentFrame = HIPS_OTHER_FRAME;
        m_currentURL.clear();
        m_currentOrder = 0;
        m_currentTileWidth = 0;
        m_uid = 0;
        return true;
    }

    for (QMap<QString, QString> &source : m_hipsSources)
    {
        if (source.value("obs_title") == title)
        {
            m_currentSource = source;
            m_currentFormat = source.value("hips_tile_format");
            if (m_currentFormat.contains("jpeg"))
                m_currentFormat = "jpg";
            else if (m_currentFormat.contains("png"))
                m_currentFormat = "png";
            else
            {
                qCWarning(KSTARS) << "FITS HIPS images are not currently supported.";
                return false;
            }

            m_currentOrder = source.value("hips_order").toInt();
            m_currentTileWidth = source.value("hips_tile_width").toInt();

            if (source.value("hips_frame") == "equatorial")
                m_currentFrame = HIPS_EQUATORIAL_FRAME;
            else if (source.value("hips_frame") == "galactic")
                m_currentFrame = HIPS_GALACTIC_FRAME;
            else
                m_currentFrame = HIPS_OTHER_FRAME;

            m_currentURL = QUrl(source.value("hips_service_url"));
            m_uid = qHash(m_currentURL);

            Options::setHIPSSource(title);
            Options::setShowHIPS(true);

            return true;
        }
    }

    return false;
}

void RemoveTimer::setKey(const pixCacheKey_t &key)
{
    m_key = key;
}
