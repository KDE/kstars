/***************************************************************************
                          deviceorientation.cpp  -  K Desktop Planetarium
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
#include "deviceorientation.h"

#if defined(Q_OS_ANDROID)
#include <QAndroidJniObject>
#include <QtAndroidExtras>
#include <QtAndroid>
#define ALPHA_LOW_PASS 0.1
#endif

DeviceOrientation::DeviceOrientation(QObject *parent) : QObject(parent)
{
}

void DeviceOrientation::stopSensors()
{
#if defined(Q_OS_ANDROID)
    QAndroidJniObject activity = QtAndroid::androidActivity();
    activity.callMethod<void>("stopSensors");
#endif
}

void DeviceOrientation::startSensors()
{
#if defined(Q_OS_ANDROID)
    QAndroidJniObject activity = QtAndroid::androidActivity();
    activity.callMethod<void>("startSensors");
#endif
}

void DeviceOrientation::getOrientation()
{
#if defined(Q_OS_ANDROID)
    QAndroidJniObject activity = QtAndroid::androidActivity();
    m_Azimuth                  = m_Azimuth + ALPHA_LOW_PASS * (activity.callMethod<float>("getAzimuth") - m_Azimuth);
    m_Altitude                 = m_Altitude + ALPHA_LOW_PASS * (activity.callMethod<float>("getPitch") - m_Altitude);

    float newRoll = activity.callMethod<float>("getRoll");
    m_Roll        = abs(newRoll - m_Roll) > 10 ? newRoll : m_Roll + ALPHA_LOW_PASS * (newRoll - m_Roll);
#endif
}
