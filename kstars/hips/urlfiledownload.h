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
  void begin(const QUrl &url, const pixCacheKey_t &key);
  void abortAll();

signals:
  void sigDownloadDone(QNetworkReply::NetworkError error, QByteArray &data, pixCacheKey_t &key);
  void sigAbort();

public slots:    

private slots:
  void downloadFinished(QNetworkReply *reply);

private:    
  QNetworkAccessManager m_manager;
};

#endif // URLFILEDOWNLOAD_H
