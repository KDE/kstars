/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later

    Handle INDI Standard properties.
*/

#pragma once

#include "indicommon.h"

#include <indiproperty.h>
#include <basedevice.h>

#include <QObject>
#include <QVariant>
#include <QJsonArray>

#ifndef KSTARS_LITE
#include <QDBusArgument>
#endif

#define MAXINDIFILENAME 512

class ClientManager;
class DriverInfo;
class DeviceInfo;
class QTimer;
class QFile;

using Properties = INDI::BaseDevice::Properties;

// INDI Standard Device Namespace
namespace ISD
{

typedef enum { PARK_UNKNOWN, PARK_PARKED, PARK_PARKING, PARK_UNPARKING, PARK_UNPARKED, PARK_ERROR } ParkStatus;

// Create instances as per driver interface.
class ConcreteDevice;
class Mount;
class Camera;
class Guider;
class Focuser;
class FilterWheel;
class Dome;
class GPS;
class Weather;
class AdaptiveOptics;
class DustCap;
class LightBox;
class Detector;
class Rotator;
class Spectrograph;
class Correlator;
class Auxiliary;

class GDSetCommand : public QObject
{
        Q_OBJECT

    public:
        GDSetCommand(INDI_PROPERTY_TYPE inPropertyType, const QString &inProperty, const QString &inElement,
                     QVariant qValue, QObject *parent);
        INDI_PROPERTY_TYPE propType;

        QString indiProperty;
        QString indiElement;
        QVariant elementValue;
};

/**
 * @class GDInterface
 * GDInterface is the Generic Device <i>Interface</i> for INDI devices. It is used as part of the Decorator Pattern when initially a new INDI device is created as a
 * Generic Device in INDIListener. If the device registers an INDI Standard Property belonging to one specific device type (e.g. Telescope), then the device functionality
 * is extended to the particular device type.
 *
 * DeviceDecorator subclasses GDInterface and calls concrete decorators methods.
 *
 * @author Jasem Mutlaq
 */
class GDInterface : public QObject
{
        Q_OBJECT

    public:
        explicit GDInterface(QObject *parent) : QObject(parent) {}

        // Property registration
        virtual void registerProperty(INDI::Property prop) = 0;
        virtual void removeProperty(const QString &name) = 0;

        // Property updates
        virtual void processSwitch(ISwitchVectorProperty *svp) = 0;
        virtual void processText(ITextVectorProperty *tvp) = 0;
        virtual void processNumber(INumberVectorProperty *nvp) = 0;
        virtual void processLight(ILightVectorProperty *lvp) = 0;
        virtual void processBLOB(IBLOB *bp) = 0;

        // Messages
        virtual void processMessage(int messageID) = 0;
};

/**
 * @class GenericDevice
 * GenericDevice is the Generic Device for INDI devices. When a new INDI device is created in INDIListener, it gets created as a GenericDevice initially. If the device
 * registers a standard property that is a key property to a device type family (e.g. Number property EQUATORIAL_EOD_COORD signifies a Telescope device), then the specialized version of
 * the device is extended via the Decorator Pattern.
 *
 * GenericDevice handles common functions shared across many devices such as time and location handling, configuration processing, retrieving information about properties, driver info..etc.
 *
 * @author Jasem Mutlaq
 */
class GenericDevice : public GDInterface
{
        Q_OBJECT

    public:
        explicit GenericDevice(DeviceInfo &idv, ClientManager *cm, QObject *parent = nullptr);
        virtual ~GenericDevice() override;

        virtual void registerProperty(INDI::Property prop) override;
        virtual void removeProperty(const QString &name) override;
        virtual void processSwitch(ISwitchVectorProperty *svp) override;
        virtual void processText(ITextVectorProperty *tvp) override;
        virtual void processNumber(INumberVectorProperty *nvp) override;
        virtual void processLight(ILightVectorProperty *lvp) override;
        virtual void processBLOB(IBLOB *bp) override;
        virtual void processMessage(int messageID) override;

        virtual const QString &getDeviceName() const;
        virtual DriverInfo *getDriverInfo() const
        {
            return m_DriverInfo;
        }
        virtual DeviceInfo *getDeviceInfo() const
        {
            return m_DeviceInfo;
        }
        virtual Properties getProperties()
        {
            return m_BaseDevice->getProperties();
        }
        virtual uint32_t getDriverInterface()
        {
            return m_DriverInterface;
        }
        virtual QString getDriverVersion()
        {
            return m_DriverVersion;
        }

        virtual bool setConfig(INDIConfig tConfig);
        virtual bool isConnected() const
        {
            return m_Connected;
        }
        virtual INDI::BaseDevice *getBaseDevice() const
        {
            return m_BaseDevice;
        }
        ClientManager *getClientManager() const
        {
            return m_ClientManager;
        }
        virtual bool getMinMaxStep(const QString &propName, const QString &elementName, double *min, double *max,
                                   double *step);
        virtual IPState getState(const QString &propName);
        virtual IPerm getPermission(const QString &propName);
        virtual INDI::Property getProperty(const QString &propName);
        virtual bool getJSONProperty(const QString &propName, QJsonObject &propObject, bool compact);
        virtual bool getJSONBLOB(const QString &propName, const QString &elementName, QJsonObject &blobObject);
        virtual bool setJSONProperty(const QString &propName, const QJsonArray &propElements);

        ConcreteDevice *getConcreteDevice(uint32_t interface);

    public slots:
        virtual bool Connect();
        virtual bool Disconnect();
        virtual bool setProperty(QObject *);

    protected slots:
        virtual void resetWatchdog();

    protected:
        void createDeviceInit();
        void updateTime();
        void updateLocation();
        void generateDevices();

    protected:
        uint32_t m_DriverInterface { 0 };
        QString m_DriverVersion;
        QMap<uint32_t, QSharedPointer<ConcreteDevice>> m_ConcreteDevices;

    signals:
        void Connected();
        void Disconnected();
        void switchUpdated(ISwitchVectorProperty *svp);
        void textUpdated(ITextVectorProperty *tvp);
        void numberUpdated(INumberVectorProperty *nvp);
        void lightUpdated(ILightVectorProperty *lvp);
        void BLOBUpdated(IBLOB *bp);
        void messageUpdated(int messageID);

        void interfaceDefined();
        void systemPortDetected();
        void propertyDefined(INDI::Property prop);
        void propertyDeleted(const QString &name);
        void ready();

        // These are emitted as soon as the driver interface defines them
        void newMount(Mount *device);
        void newCamera(Camera *device);
        void newGuider(Guider *device);
        void newFocuser(Focuser *device);
        void newFilterWheel(FilterWheel *device);
        void newDome(Dome *device);
        void newGPS(GPS *device);
        void newWeather(Weather *device);
        void newAdaptiveOptics(AdaptiveOptics *device);
        void newDustCap(DustCap *device);
        void newLightBox(LightBox *device);
        void newDetector(Detector *device);
        void newRotator(Rotator *device);
        void newSpectrograph(Spectrograph *device);
        void newCorrelator(Correlator *device);
        void newAuxiliary(Auxiliary *device);

    private:

        class StreamFileMetadata
        {
            public:
                QString device;
                QString property;
                QString element;
                QFile *file { nullptr};
        };

        static void registerDBusType();
        bool m_Connected { false };
        QString m_Name;
        DriverInfo *m_DriverInfo { nullptr };
        DeviceInfo *m_DeviceInfo { nullptr };
        INDI::BaseDevice *m_BaseDevice { nullptr };
        ClientManager *m_ClientManager { nullptr };
        QTimer *watchDogTimer { nullptr };
        QTimer *m_ReadyTimer {nullptr};
        QList<StreamFileMetadata> streamFileMetadata;
};

void propertyToJson(ISwitchVectorProperty *svp, QJsonObject &propObject, bool compact = true);
void propertyToJson(ITextVectorProperty *tvp, QJsonObject &propObject, bool compact = true);
void propertyToJson(INumberVectorProperty *nvp, QJsonObject &propObject, bool compact = true);
void propertyToJson(ILightVectorProperty *lvp, QJsonObject &propObject, bool compact = true);
void propertyToJson(INDI::Property prop, QJsonObject &propObject, bool compact = true);

}


#ifndef KSTARS_LITE
Q_DECLARE_METATYPE(ISD::ParkStatus)
QDBusArgument &operator<<(QDBusArgument &argument, const ISD::ParkStatus &source);
const QDBusArgument &operator>>(const QDBusArgument &argument, ISD::ParkStatus &dest);
#endif
