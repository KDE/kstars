/*
    SPDX-FileCopyrightText: 2015-2017 Pavel Mraz

    SPDX-FileCopyrightText: 2017 Jasem Mutlaq

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "urlfiledownload.h"
#include "auxiliary/cdsmirrors.h"

#include <QDebug>

namespace
{
// Give up on a tile after this time, so that a server that accepts connections but does not answer
// counts as failed and HIPSManager can switch to a mirror.
constexpr int TileTimeoutMs = 30000;
}

UrlFileDownload::UrlFileDownload(QObject *parent, QNetworkDiskCache *cache) : QObject(parent)
{
    connect(&m_manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(downloadFinished(QNetworkReply*)));

    m_manager.setCache(cache);
}

void UrlFileDownload::begin(const QUrl &url, const pixCacheKey_t &key, const QString &mirror)
{
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);

    QNetworkReply *reply = m_manager.get(request);

    connect(this, SIGNAL(sigAbort()), reply, SLOT(abort()));

    QVariant val;
    val.setValue(key);
    reply->setProperty("user_data0", val);
    reply->setProperty("hips_mirror", mirror);

    if (!url.isLocalFile())
        CDSMirrors::abortAfter(reply, TileTimeoutMs);
}

void UrlFileDownload::abortAll()
{
    Q_EMIT sigAbort();
}

void UrlFileDownload::downloadFinished(QNetworkReply *reply)
{
    pixCacheKey_t key = reply->property("user_data0").value<pixCacheKey_t>();
    const QString mirror = reply->property("hips_mirror").toString();

    //QVariant fromCache = reply->attribute(QNetworkRequest::SourceIsFromCacheAttribute);

    if (reply->error() == QNetworkReply::NoError)
    {
        QByteArray data = reply->readAll();

        Q_EMIT sigDownloadDone(reply->error(), data, key, mirror);
    }
    else
    {
        QByteArray empty;
        Q_EMIT sigDownloadDone(reply->error(), empty, key, mirror);
    }

    reply->deleteLater();
}
