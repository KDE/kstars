/***************************************************************************
                          weatherapixml.h  -  K Desktop Planetarium
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

#ifndef WEATHERAPIXML_H
#define WEATHERAPIXML_H

#include "weatherapi.h"
#include "currentweatherdata.h"

QT_BEGIN_NAMESPACE
class QXmlStreamReader;
QT_END_NAMESPACE

class WeatherApiXml : public WeatherAPI
{
    Q_OBJECT

public:
    WeatherApiXml( QNetworkAccessManager *manager, QObject *parent = 0 )
        : WeatherAPI(manager, parent)
    {
    }


protected slots:
    void replyFinished(QNetworkReply *reply);

private:
    void readResponse();
    void readCurrentWeatherData();

    QXmlStreamReader *m_XmlReader;
};

#endif // WEATHERAPIXML_H
