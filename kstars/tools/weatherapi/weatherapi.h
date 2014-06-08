/***************************************************************************
                          weatherapi.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun June 8 2014
    copyright            : (C) 2012 by Vijay Dhameliya
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

#ifndef WEATHERAPI_H
#define WEATHERAPI_H

#include <QString>
#include <QObject>

#include "dms.h"
#include "currentweatherdata.h"

QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
class QNetworkReply;
QT_END_NAMESPACE

class WeatherAPI : public QObject
{
    Q_OBJECT

public:
    WeatherAPI(QNetworkAccessManager *manager, QObject *parent = 0);

    void getWeatherByCity(const QString &name);

    void getWeatherByPos(const dms * lat, const dms * lon);

    CurrentWeatherData currentWeatherData() const { return data; }

protected slots:
    virtual void replyFinished(QNetworkReply *reply) = 0;

signals:
    void dataCollectionFinished(bool resultOK);

protected:
    CurrentWeatherData data;
    QNetworkAccessManager *m_NetworkManager;

private:
    void weatherApiRequestFormatAndSend(const QString &requestStub);

    QString m_UrlBase;
};

#endif // WEATHERAPI_H
