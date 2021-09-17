/*
    SPDX-FileCopyrightText: 2017 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/
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
