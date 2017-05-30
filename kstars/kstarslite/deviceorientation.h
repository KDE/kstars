#ifndef DEVICEORIENTATION_H
#define DEVICEORIENTATION_H
#include <QObject>

class DeviceOrientation : public QObject
{
        Q_OBJECT

//    Q_PROPERTY(int azimuth MEMBER m_Azimuth NOTIFY azimuthChanged)
//    Q_PROPERTY(int pitch MEMBER m_Pitch NOTIFY pitchChanged)
//    Q_PROPERTY(int roll MEMBER m_Roll NOTIFY rollChanged)
    public:
        DeviceOrientation(QObject * parent = nullptr);
        void getOrientation();

        float getAzimuth()
        {
            return m_Azimuth;
        }
        float getAltitude()
        {
            return m_Altitude;
        }
        float getRoll()
        {
            return m_Roll;
        }
        void stopSensors();
        void startSensors();
    signals:
//    void pitchChanged(int pitch);
//    void azimuthChanged(int Azimuth);
//    void rollChanged(int roll);

    private:
        float m_Azimuth;
        float m_Altitude;
        float m_Roll;
};

#endif // DEVICEORIENTATION_H
