/*
    SPDX-FileCopyrightText: 2015-2017 Pavel Mraz

    SPDX-FileCopyrightText: 2017 Jasem Mutlaq

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef URLFILEDOWNLOAD_H
#define URLFILEDOWNLOAD_H

#include "hips.h"

#include <QtNetwork>

class UrlFileDownload : public QObject
{
        Q_OBJECT
    public:
        explicit UrlFileDownload(QObject *parent, QNetworkDiskCache *cache);
        /**
         * @brief begin Start downloading a tile.
         * @param url Tile URL.
         * @param key Cache key of the tile, passed back with sigDownloadDone().
         * @param mirror Base URL of the server the tile is requested from, passed back with sigDownloadDone().
         */
        void begin(const QUrl &url, const pixCacheKey_t &key, const QString &mirror = QString());
        void abortAll();

    Q_SIGNALS:
        void sigDownloadDone(QNetworkReply::NetworkError error, QByteArray &data, pixCacheKey_t &key,
                             const QString &mirror);
        void sigAbort();

    public Q_SLOTS:

    private Q_SLOTS:
        void downloadFinished(QNetworkReply *reply);

    private:
        QNetworkAccessManager m_manager;
};

#endif // URLFILEDOWNLOAD_H
