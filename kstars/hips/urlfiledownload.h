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

#ifndef URLFILEDOWNLOAD_H
#define URLFILEDOWNLOAD_H

#include "hips.h"

#include <QtNetwork>

class UrlFileDownload : public QObject
{
  Q_OBJECT
public:
  explicit UrlFileDownload(QObject *parent, QNetworkDiskCache *cache);
  void begin(const QString &urlName, const pixCacheKey_t &key);
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
