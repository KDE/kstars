/***************************************************************************
                          deviceorientation.h  -  K Desktop Planetarium
                             -------------------
    begin                : 13/02/2017
    copyright            : (C) 2017 by Artem Fedoskin
    email                : afedoskin3@gmail.com
 ***************************************************************************/
/** *************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#pragma once

#include <QObject>

class DeviceOrientation : public QObject
{
    Q_OBJECT

    //    Q_PROPERTY(int azimuth MEMBER m_Azimuth NOTIFY azimuthChanged)
    //    Q_PROPERTY(int pitch MEMBER m_Pitch NOTIFY pitchChanged)
    //    Q_PROPERTY(int roll MEMBER m_Roll NOTIFY rollChanged)
  public:
    explicit DeviceOrientation(QObject *parent = nullptr);
    void getOrientation();

    float getAzimuth() { return m_Azimuth; }
    float getAltitude() { return m_Altitude; }
    float getRoll() { return m_Roll; }
    void stopSensors();
    void startSensors();
  signals:
    //    void pitchChanged(int pitch);
    //    void azimuthChanged(int Azimuth);
    //    void rollChanged(int roll);

  private:
    float m_Azimuth { 0 };
    float m_Altitude { 0 };
    float m_Roll { 0 };
};
