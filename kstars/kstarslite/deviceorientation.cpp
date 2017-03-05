#include "deviceorientation.h"

#if defined(Q_OS_ANDROID)
    #include <QAndroidJniObject>
    #include <QtAndroidExtras>
    #include <QtAndroid>
#endif

DeviceOrientation::DeviceOrientation(QObject *parent)
    : QObject(parent)
{

}

void DeviceOrientation::stopSensors()
{
#if defined (Q_OS_ANDROID)
    QAndroidJniObject activity = QtAndroid::androidActivity();
    activity.callMethod<void>("stopSensors");
#endif
}

void DeviceOrientation::startSensors()
{
#if defined (Q_OS_ANDROID)
    QAndroidJniObject activity = QtAndroid::androidActivity();
    activity.callMethod<void>("startSensors");
#endif
}

void DeviceOrientation::getOrientation()
{
#if defined (Q_OS_ANDROID)
    QAndroidJniObject activity = QtAndroid::androidActivity();
    m_Azimuth = activity.callMethod<float>("getAzimuth");
    m_Altitude = activity.callMethod<float>("getPitch");
    m_Roll = activity.callMethod<float>("getRoll");
#endif
}
