/*
    SPDX-FileCopyrightText: 2013 Vijay Dhameliya <vijay.atwork13@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

/**
 * @class SkyObjDescription
 *
 * Fetches short description for various sky object from wikipedia.
 *
 * @author Vijay Dhameliya
 */
class SkyObjDescription : public QObject
{
  Q_OBJECT

  public:
    /**
     * @brief Constructor sends request to network for data from wikipedia API and starts
     * downloading data from QUrl
     * @param soName SkyObject name
     * @param soType SkyObject type
     */
    explicit SkyObjDescription(const QString soName, const QString soType);

    virtual ~SkyObjDescription() override = default;

    /** @return returns description if it was available on wikipedia else returns empty string */
    QString downloadedData() const { return m_description; }

    /** @return returns wikipedia link for skyobject */
    QString url() const { return m_url; }

  signals:
    void downloaded();

  private slots:
    /**
     * @brief parse downloaded data to extract description of SkyObject when downloading is finished
     *
     * @param reply
     */
    void fileDownloaded(QNetworkReply *reply);

  private:
    QString soName, soType, m_description, m_url;
    QNetworkAccessManager *manager { nullptr };
    QByteArray m_DownloadedData;
};
