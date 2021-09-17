/*
    SPDX-FileCopyrightText: 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    Adapted from https://wiki.qt.io/Download_Data_from_URL

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QByteArray>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QObject>
#include <QTemporaryFile>

#include <functional>

class QProgressDialog;

class FileDownloader : public QObject
{
    Q_OBJECT
  public:
    explicit FileDownloader(QObject *parent = nullptr);
    ~FileDownloader() override = default;

    void get(const QUrl &fileUrl);
    void post(const QUrl &fileUrl, QByteArray &data);
    void post(const QUrl &fileUrl, QHttpMultiPart *parts);

    QByteArray downloadedData() const;

    QUrl getDownloadedFileURL() const;
    bool setDownloadedFileURL(const QUrl &DownloadedFile);

    void setProgressDialogEnabled(bool ShowProgressDialog, const QString &textTitle = QString(),
                                  const QString &textLabel = QString());

    // Callbacks to verify data before being accepted
    void registerDataVerification(std::function<bool(const QByteArray &data)> verifyFunc) { m_verifyData = verifyFunc; }
    void registerFileVerification(std::function<bool(const QString &filename)> verifyFile){ m_verifyFile = verifyFile; }

  signals:
    void downloaded();
    void canceled();
    void error(const QString &errorString);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);

  private slots:
    void dataFinished(QNetworkReply *pReply);
    void dataReady();
    void slotError();
    void setDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);

  private:
    QNetworkAccessManager m_WebCtrl;
    QByteArray m_DownloadedData;

    // Downloaded file
    QUrl m_DownloadedFileURL;

    // Temporary file used until download is successful
    QTemporaryFile m_downloadTemporaryFile;

    // Network reply
    QNetworkReply *m_Reply { nullptr };

    // Optional Progress dialog
    bool m_ShowProgressDialog { false };

#ifndef KSTARS_LITE
    QProgressDialog *progressDialog { nullptr };
#endif
    bool isCancelled { false };
    QString label;
    QString title;

    std::function<bool(const QByteArray &data)> m_verifyData;
    std::function<bool(const QString &filename)> m_verifyFile;
};
