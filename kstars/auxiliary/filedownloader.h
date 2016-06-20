/*  KStars File Downloader
 *
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    Adapted from https://wiki.qt.io/Download_Data_from_URL

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef FILEDOWNLOADER_H
#define FILEDOWNLOADER_H

#include <QObject>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

class FileDownloader : public QObject
{
    Q_OBJECT
public:
    explicit FileDownloader(QObject *parent = 0);
    virtual ~FileDownloader();

    void get(const QUrl & fileUrl);
    void post(const QUrl & fileUrl, QByteArray & data);
    void post(const QUrl & fileUrl, QHttpMultiPart *parts);

    QByteArray downloadedData() const;

signals:
    void downloaded();
    void error(const QString &errorString);


private slots:
    void fileDownloaded(QNetworkReply* pReply);
    void slotError();

private:
    QNetworkAccessManager m_WebCtrl;
    QByteArray m_DownloadedData;
};

#endif // FILEDOWNLOADER_H
