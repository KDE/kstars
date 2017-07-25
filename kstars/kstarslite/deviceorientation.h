
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
