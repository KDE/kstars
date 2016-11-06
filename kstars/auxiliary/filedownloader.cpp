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
#include <QProgressDialog>

#include <KLocalizedString>

#include "filedownloader.h"

#ifndef KSTARS_LITE
#include "kstars.h"
#endif

FileDownloader::FileDownloader(QObject *parent) :  QObject(parent)
{
    connect(&m_WebCtrl, SIGNAL (finished(QNetworkReply*)), this, SLOT (dataFinished(QNetworkReply*)));
}

FileDownloader::~FileDownloader()
{    
}

void FileDownloader::get(const QUrl & fileUrl)
{
    QNetworkRequest request(fileUrl);
    m_DownloadedData.clear();
    isCancelled = false;
    m_Reply = m_WebCtrl.get(request);

    connect(m_Reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(slotError()));
    connect(m_Reply, SIGNAL(downloadProgress(qint64,qint64)), this, SIGNAL(downloadProgress(qint64,qint64)));
    connect(m_Reply, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(setDownloadProgress(qint64,qint64)));
    connect(m_Reply, SIGNAL(readyRead()), this, SLOT(dataReady()));
}

void FileDownloader::post(const QUrl &fileUrl, QByteArray & data)
{
    QNetworkRequest request(fileUrl);
    m_DownloadedData.clear();
    isCancelled = false;
    m_Reply = m_WebCtrl.post(request, data);

    connect(m_Reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(slotError()));
    connect(m_Reply, SIGNAL(downloadProgress(qint64,qint64)), this, SIGNAL(downloadProgress(qint64,qint64)));
    connect(m_Reply, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(setDownloadProgress(qint64,qint64)));
    connect(m_Reply, SIGNAL(readyRead()), this, SLOT(dataReady()));
}

void FileDownloader::post(const QUrl & fileUrl, QHttpMultiPart *parts)
{
    QNetworkRequest request(fileUrl);
    m_DownloadedData.clear();
    isCancelled = false;
    m_Reply = m_WebCtrl.post(request, parts);

    connect(m_Reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(slotError()));
    connect(m_Reply, SIGNAL(downloadProgress(qint64,qint64)), this, SIGNAL(downloadProgress(qint64,qint64)));
    connect(m_Reply, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(setDownloadProgress(qint64,qint64)));
    connect(m_Reply, SIGNAL(readyRead()), this, SLOT(dataReady()));
}

void FileDownloader::dataReady()
{
   if (m_DownloadedFile.isOpen())
       m_DownloadedFile.write(m_Reply->readAll());
   else
       m_DownloadedData += m_Reply->readAll();
}

void FileDownloader::dataFinished(QNetworkReply* pReply)
{
    if (isCancelled == false)
        emit downloaded();

    pReply->deleteLater();
}

void FileDownloader::slotError()
{
    m_Reply->deleteLater();
    emit error(m_Reply->errorString());
}

void FileDownloader::setProgressDialogEnabled(bool ShowProgressDialog, const QString& textTitle, const QString &textLabel)
{
    m_ShowProgressDialog = ShowProgressDialog;

    if (title.isEmpty())
        title = i18n("Downloading");
    else
        title = textTitle;

    if (textLabel.isEmpty())
       label = i18n("Downloading Data...");
    else
        label = textLabel;
}

QUrl FileDownloader::getDownloadedFileURL() const
{
    return m_DownloadedFileURL;
}

bool FileDownloader::setDownloadedFileURL(const QUrl &DownloadedFile)
{
    m_DownloadedFileURL = DownloadedFile;

    if (m_DownloadedFileURL.isEmpty() == false)
    {
        m_DownloadedFile.setFileName(m_DownloadedFileURL.toLocalFile());
        bool rc = m_DownloadedFile.open( QIODevice::WriteOnly|QIODevice::Truncate|QIODevice::Text );

        if (rc == false)
            qWarning() << m_DownloadedFile.errorString();

        return rc;
    }

    m_DownloadedFile.close();
    return true;
}

void FileDownloader::setDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
#ifndef KSTARS_LITE
    if (m_ShowProgressDialog)
    {
        if (progressDialog == NULL)
        {
            isCancelled = false;
            progressDialog = new QProgressDialog(KStars::Instance());
            progressDialog->setWindowTitle(title);
            progressDialog->setLabelText(label);
            connect(progressDialog, SIGNAL(canceled()), this, SIGNAL(canceled()));
            connect(progressDialog, &QProgressDialog::canceled, this, [&]() { isCancelled = true; m_Reply->abort(); });
            progressDialog->setMinimum(0);
            progressDialog->show();
        }

        progressDialog->setMaximum(bytesTotal);
        progressDialog->setValue(bytesReceived);
    }
#endif
}

QByteArray FileDownloader::downloadedData() const
{
    return m_DownloadedData;
}
