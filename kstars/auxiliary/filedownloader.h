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
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

class QProgressDialog;

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

    QUrl getDownloadedFileURL() const;
    bool setDownloadedFileURL(const QUrl &DownloadedFile);

    void setProgressDialogEnabled(bool ShowProgressDialog, const QString &textTitle = QString(), const QString &textLabel = QString());

signals:
    void downloaded();
    void canceled();
    void error(const QString &errorString);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);

private slots:    
    void dataFinished(QNetworkReply* pReply);
    void dataReady();
    void slotError();
    void setDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);

private:
    QNetworkAccessManager m_WebCtrl;
    QByteArray m_DownloadedData;

    // Downloaded file
    QUrl m_DownloadedFileURL;
    QFile m_DownloadedFile;

    // Network reply
    QNetworkReply *m_Reply = NULL;

    // Optional Progress dialog
    bool m_ShowProgressDialog = false;

    #ifndef KSTARS_LITE
    QProgressDialog* progressDialog = NULL;
    #endif
    bool isCancelled = false;
    QString label, title;
};

#endif // FILEDOWNLOADER_H
