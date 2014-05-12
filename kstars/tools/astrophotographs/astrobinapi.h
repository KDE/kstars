/***************************************************************************
                          astrobinapi.h  -  K Desktop Planetarium
                             -------------------
    begin                : Wed Aug 8 2012
    copyright            : (C) 2012 by Lukasz Jaskiewicz and Rafal Kulaga
    email                : lucas.jaskiewicz@gmail.com, rl.kulaga@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef ASTROBINAPI_H
#define ASTROBINAPI_H

#include "astrobinsearchresult.h"

#include <QString>
#include <QObject>

QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
class QNetworkReply;
QT_END_NAMESPACE


class AstroBinApi : public QObject
{
    Q_OBJECT

public:
    AstroBinApi(QNetworkAccessManager *manager, QObject *parent = 0);

    virtual ~AstroBinApi();

    void searchImageOfTheDay();
    void searchObjectImages(const QString &name);
    void searchTitleContaining(const QString &string);
    void searchDescriptionContaining(const QString &string);
    void searchUserImages(const QString &user);

    void setNumberOfResultsLimit(int limit) { m_ResultsLimit = limit; }
    void setOffset(int offset) { m_Offset = offset; }

    AstroBinSearchResult getResult() { return m_LastSearchResult; }

signals:
    void searchFinished(bool resultOK);

protected slots:
    virtual void replyFinished(QNetworkReply *reply) = 0;

protected:
    QNetworkAccessManager *m_NetworkManager;

    // last search result
    AstroBinSearchResult m_LastSearchResult;

    QString m_UrlApiTypeEnding;

private:
    void astroBinRequestFormatAndSend(const QString &requestStub);

    QString m_UrlBase;
    QString m_ApiKey;
    QString m_ApiSecret;

    int m_ResultsLimit;
    int m_Offset;
};

#endif // ASTROBINAPI_H
