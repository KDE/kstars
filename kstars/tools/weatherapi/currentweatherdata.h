/***************************************************************************
                          currentweatherdata.h  -  K Desktop Planetarium
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

#ifndef CURRENTWEATHERDATA_H
#define CURRENTWEATHERDATA_H

#include <QString>

class CurrentWeatherData
{
    friend class WeatherApiXml;

public:
    inline QString city() const { return m_City; }
    inline double minimumTemp() const { return m_TempMin; }
    inline double maximumTemp() const { return m_TempMax; }
    inline QString unitOfTemp() const { return m_TempUnit; }
    inline double humidity() const { return m_Humidity; }
    inline QString unitOfHumidity() const { return m_HumidityUnit; }
    inline double pressure() const { return m_Pressure; }
    inline QString unitOfPressure() const { return m_PressureUnit; }
    inline double windSpeed() const { return m_WindSpeed; }
    inline QString windSpeedName() const { return m_WindSpeedName; }
    inline double windDirection() const { return m_WindDirection; }
    inline QString windDirectionName() const { return m_WindDirectionName; }
    inline double clouds() const { return m_Clouds; }
    inline QString cloudsName() const { return m_CloudsName; }
    inline QString precipitation() const { return m_PrecMode; }
    inline QString lastUpdate() const { return m_LastUpdate; }


private:
    QString m_City;
    double m_TempMin;
    double m_TempMax;
    QString m_TempUnit;
    double m_Humidity;
    QString m_HumidityUnit;
    double m_Pressure;
    QString m_PressureUnit;
    double m_WindSpeed;
    QString m_WindSpeedName;
    double m_WindDirection;
    QString m_WindDirectionName;
    double m_Clouds;
    QString m_CloudsName;
    QString m_PrecMode;
    QString m_LastUpdate;

};

#endif // CURRENTWEATHERDATA_H

