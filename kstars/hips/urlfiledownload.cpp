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

#include "urlfiledownload.h"
#include <QDebug>

UrlFileDownload::UrlFileDownload(QObject *parent, QNetworkDiskCache *cache) : QObject(parent)
{
    connect(&m_manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(downloadFinished(QNetworkReply*)));

    m_manager.setCache(cache);
}

void UrlFileDownload::begin(const QUrl &url, const pixCacheKey_t &key)
{
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);

    QNetworkReply *reply = m_manager.get(request);

    connect(this, SIGNAL(sigAbort()), reply, SLOT(abort()));

    QVariant val;
    val.setValue(key);
    reply->setProperty("user_data0", val);
}

void UrlFileDownload::abortAll()
{
    emit sigAbort();
}

void UrlFileDownload::downloadFinished(QNetworkReply *reply)
{
    pixCacheKey_t key = reply->property("user_data0").value<pixCacheKey_t>();

    //QVariant fromCache = reply->attribute(QNetworkRequest::SourceIsFromCacheAttribute);

    if (reply->error() == QNetworkReply::NoError)
    {
        QByteArray data = reply->readAll();

        emit sigDownloadDone(reply->error(), data, key);
    }
    else
    {
        QByteArray empty;
        emit sigDownloadDone(reply->error(), empty, key);
    }

    reply->deleteLater();
}
