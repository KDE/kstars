/***************************************************************************
                          skyobjdescription.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2013/10/13
    copyright            : (C) 2013 by Vijay Dhameliya
    email                : vijay.atwork13@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SKYOBJDESCRIPTION_H
#define SKYOBJDESCRIPTION_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

/**
 * \class SkyObjDescription
 * Fatches short description for various sky object from wikipedia.
 * \author Vijay Dhameliya
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

    /**
     * \brief Destructor
     */
    virtual ~SkyObjDescription();

    /**
     * @return returns description if it was available on wikipedia else returns empty string
     */
    QString downloadedData() const;

signals:
        void downloaded();

private slots:
    /**
     * @brief parse downloaded data to extract description of SkyObject when downloading is finished
     * @param reply
     */
    void fileDownloaded(QNetworkReply* reply);

private:
    QString soName, soType, m_description;
    QNetworkAccessManager* manager;
    QByteArray m_DownloadedData;

};

#endif // SKYOBJDESCRIPTION_H
