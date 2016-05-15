/*  KStars File Downloader
 *
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    Adapted from https://wiki.qt.io/Download_Data_from_URL

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <QFile>

#include "filedownloader.h"

FileDownloader::FileDownloader(QObject *parent) :  QObject(parent)
{
    connect(&m_WebCtrl, SIGNAL (finished(QNetworkReply*)), this, SLOT (fileDownloaded(QNetworkReply*)));
}

FileDownloader::~FileDownloader()
{    
}

void FileDownloader::get(const QUrl & fileUrl)
{
    QNetworkRequest request(fileUrl);
    QNetworkReply *reply = m_WebCtrl.get(request);

    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(slotError()));
}

void FileDownloader::post(const QUrl &fileUrl, QByteArray & data)
{
    QNetworkRequest request(fileUrl);
    QNetworkReply *reply = m_WebCtrl.post(request, data);

    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(slotError()));
}

void FileDownloader::post(const QUrl & fileUrl, QHttpMultiPart *parts)
{
    QNetworkRequest request(fileUrl);
    QNetworkReply *reply = m_WebCtrl.post(request, parts);

    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(slotError()));
}


void FileDownloader::fileDownloaded(QNetworkReply* pReply)
{
    m_DownloadedData = pReply->readAll();
    pReply->deleteLater();
    //emit a signal    
    emit downloaded();
}

void FileDownloader::slotError()
{
    QNetworkReply *pReply = static_cast<QNetworkReply *>(sender());
    pReply->deleteLater();
    emit error(pReply->errorString());
}

QByteArray FileDownloader::downloadedData() const
{
    return m_DownloadedData;
}
