/***************************************************************************
                          weatherapi.cpp  -  K Desktop Planetarium
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

#include "weatherapi.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QDebug>

WeatherAPI::WeatherAPI(QNetworkAccessManager *manager, QObject *parent)
    : QObject(parent), m_NetworkManager(manager)
{
    m_UrlBase = "http://api.openweathermap.org/data/2.5/";

    connect(m_NetworkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(replyFinished(QNetworkReply*)));
}

void WeatherAPI::getWeatherByCity(const QString &name){
    QString requestStub = "weather?q=" + name;
    weatherApiRequestFormatAndSend( requestStub );
}

void WeatherAPI::getWeatherByPos(const dms lat, const dms lon){
    QString requestStub = "weather?lat=" + QString::number( lat.degree() ) + "&lon=" + QString::number( lon.degree() );
    weatherApiRequestFormatAndSend( requestStub );
}


void WeatherAPI::weatherApiRequestFormatAndSend(const QString &requestStub){
    QUrl requestUrl(m_UrlBase + requestStub + "&mode=xml" );

    QNetworkRequest request(requestUrl);
    request.setOriginatingObject(this);
    m_NetworkManager->get(request);
}
