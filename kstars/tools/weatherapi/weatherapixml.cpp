/***************************************************************************
                          weatherapixml.cpp  -  K Desktop Planetarium
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

#include "weatherapixml.h"

#include <QNetworkReply>
#include <QXmlStreamReader>
#include <QXmlStreamAttributes>
#include <QStringRef>
#include <QDebug>

void WeatherApiXml::replyFinished(QNetworkReply *reply)
{
    if(reply->request().originatingObject() != static_cast<QObject*>(this)) {
        return;
    }

    m_XmlReader = new QXmlStreamReader(reply->readAll());
    bool result = false;

    readResponse();

    emit dataCollectionFinished(result);
}

void WeatherApiXml::readResponse()
{

    while(!m_XmlReader->atEnd()){

        m_XmlReader->readNext();

        if( m_XmlReader->name() == "current" && m_XmlReader->isStartElement() ){
            readCurrentWeatherData();
        }
    }

}

void WeatherApiXml::readCurrentWeatherData(){
    while( !m_XmlReader->atEnd() ){

        m_XmlReader->readNext();

        if( m_XmlReader->isEndElement() || m_XmlReader->name() == "" ) {
            continue;
        }

        if( m_XmlReader->name() == "city" ) {
            data.m_City = m_XmlReader->attributes().value("name").toString();
        }else if ( m_XmlReader->name() == "temperature" ) {
            data.m_TempMin = m_XmlReader->attributes().value("min").toString().toDouble();
            data.m_TempMax = m_XmlReader->attributes().value("max").toString().toDouble();
            data.m_TempUnit = m_XmlReader->attributes().value("unit").toString();
        }else if ( m_XmlReader->name() == "humidity" ) {
            data.m_Humidity = m_XmlReader->attributes().value("value").toString().toDouble();
            data.m_HumidityUnit = m_XmlReader->attributes().value("unit").toString();
        }else if ( m_XmlReader->name() == "pressure" ) {
            data.m_Pressure = m_XmlReader->attributes().value("value").toString().toDouble();
            data.m_PressureUnit = m_XmlReader->attributes().value("unit").toString().toDouble();
        }else if ( m_XmlReader->name() == "speed" ) {
            data.m_WindSpeed = m_XmlReader->attributes().value("value").toString().toDouble();
            data.m_WindSpeedName = m_XmlReader->attributes().value("name").toString();
        }else if ( m_XmlReader->name() == "direction" ) {
            data.m_WindDirection = m_XmlReader->attributes().value("value").toString().toDouble();
            data.m_WindDirectionName = m_XmlReader->attributes().value("name").toString();
        }else if ( m_XmlReader->name() == "clouds" ) {
            data.m_Clouds = m_XmlReader->attributes().value("value").toString().toDouble();
            data.m_CloudsName = m_XmlReader->attributes().value("name").toString();
        }else if ( m_XmlReader->name() == "precipitation" ) {
            data.m_PrecMode = m_XmlReader->attributes().value("mode").toString();
        }else if ( m_XmlReader->name() == "lastupdate" ) {
            data.m_LastUpdate = m_XmlReader->attributes().value("value").toString();
        }

    }
}
