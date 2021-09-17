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
        // Property handling
        virtual void registerProperty(INDI::Property prop)     = 0;
        virtual void removeProperty(const QString &name)       = 0;
        virtual void processSwitch(ISwitchVectorProperty *svp) = 0;
        virtual void processText(ITextVectorProperty *tvp)     = 0;
        virtual void processNumber(INumberVectorProperty *nvp) = 0;
        virtual void processLight(ILightVectorProperty *lvp)   = 0;
        virtual void processBLOB(IBLOB *bp)                    = 0;
        virtual void processMessage(int messageID)             = 0;

        // Accessors
        virtual Properties getProperties()                     = 0;
        virtual DeviceFamily getType()                         = 0;
        virtual DriverInfo *getDriverInfo()                    = 0;
        virtual DeviceInfo *getDeviceInfo()                    = 0;
        virtual INDI::BaseDevice *getBaseDevice()              = 0;
        virtual uint32_t getDriverInterface()                  = 0;
        virtual QString getDriverVersion()                     = 0;

        // Convenience functions
        virtual bool setConfig(INDIConfig tConfig)           = 0;
        virtual const QString &getDeviceName() const         = 0;
        virtual bool isConnected()                           = 0;
        virtual bool getMinMaxStep(const QString &propName, const QString &elementName, double *min, double *max,
                                   double *step)             = 0;
        virtual IPState getState(const QString &propName)    = 0;
        virtual IPerm getPermission(const QString &propName) = 0;
        virtual INDI::Property getProperty(const QString &propName) = 0;
        virtual bool getJSONProperty(const QString &propName, QJsonObject &propObject, bool compact) = 0;
        virtual bool getJSONBLOB(const QString &propName, const QString &elementName, QJsonObject &blobObject) = 0;
        virtual bool setJSONProperty(const QString &propName, const QJsonArray &propElements) = 0;

        virtual ~GDInterface() = default;

    public slots:
        virtual bool Connect()                                    = 0;
        virtual bool Disconnect()                                 = 0;
        virtual bool runCommand(int command, void *ptr = nullptr) = 0;
        virtual bool setProperty(QObject *)                       = 0;

    protected:
        DeviceFamily dType { KSTARS_UNKNOWN };
        uint32_t driverInterface { 0 };
        QString driverVersion;

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
        explicit GenericDevice(DeviceInfo &idv, ClientManager *cm);
        virtual ~GenericDevice();

        virtual void registerProperty(INDI::Property prop) override;
        virtual void removeProperty(const QString &name) override;
        virtual void processSwitch(ISwitchVectorProperty *svp) override;
        virtual void processText(ITextVectorProperty *tvp) override;
        virtual void processNumber(INumberVectorProperty *nvp) override;
        virtual void processLight(ILightVectorProperty *lvp) override;
        virtual void processBLOB(IBLOB *bp) override;
        virtual void processMessage(int messageID) override;

        virtual DeviceFamily getType() override
        {
            return dType;
        }
        virtual const QString &getDeviceName() const override;
        virtual DriverInfo *getDriverInfo() override
        {
            return driverInfo;
        }
        virtual DeviceInfo *getDeviceInfo() override
        {
            return deviceInfo;
        }
        virtual Properties getProperties() override
        {
            return baseDevice->getProperties();
        }
        virtual uint32_t getDriverInterface() override
        {
            return driverInterface;
        }
        virtual QString getDriverVersion() override
        {
            return driverVersion;
        }

        virtual bool setConfig(INDIConfig tConfig) override;
        virtual bool isConnected() override
        {
            return connected;
        }
        virtual INDI::BaseDevice *getBaseDevice() override
        {
            return baseDevice;
        }
        virtual bool getMinMaxStep(const QString &propName, const QString &elementName, double *min, double *max,
                                   double *step) override;
        virtual IPState getState(const QString &propName) override;
        virtual IPerm getPermission(const QString &propName) override;
        virtual INDI::Property getProperty(const QString &propName) override;
        virtual bool getJSONProperty(const QString &propName, QJsonObject &propObject, bool compact) override;
        virtual bool getJSONBLOB(const QString &propName, const QString &elementName, QJsonObject &blobObject) override;
        virtual bool setJSONProperty(const QString &propName, const QJsonArray &propElements) override;

    public slots:
        virtual bool Connect() override;
        virtual bool Disconnect() override;
        virtual bool runCommand(int command, void *ptr = nullptr) override;
        virtual bool setProperty(QObject *) override;

    protected slots:
        virtual void resetWatchdog();

    protected:
        void createDeviceInit();
        void updateTime();
        void updateLocation();

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
        bool connected { false };
        QString m_Name;
        DriverInfo *driverInfo { nullptr };
        DeviceInfo *deviceInfo { nullptr };
        INDI::BaseDevice *baseDevice { nullptr };
        ClientManager *clientManager { nullptr };
        QTimer *watchDogTimer { nullptr };
        QList<StreamFileMetadata> streamFileMetadata;
        //char BLOBFilename[MAXINDIFILENAME + 1];
};

/**
 * @class DeviceDecorator
 * DeviceDecorator is the base decorator for all specialized devices. It extends the functionality of GenericDevice.
 *
 * @author Jasem Mutlaq
 */
class DeviceDecorator : public GDInterface
{
        Q_OBJECT

    public:
        explicit DeviceDecorator(GDInterface *iPtr);
        virtual ~DeviceDecorator() override;

        virtual void registerProperty(INDI::Property prop) override;
        virtual void removeProperty(const QString &name) override;
        virtual void processSwitch(ISwitchVectorProperty *svp) override;
        virtual void processText(ITextVectorProperty *tvp) override;
        virtual void processNumber(INumberVectorProperty *nvp) override;
        virtual void processLight(ILightVectorProperty *lvp) override;
        virtual void processBLOB(IBLOB *bp) override;
        virtual void processMessage(int messageID) override;

        virtual DeviceFamily getType() override;

        virtual bool setConfig(INDIConfig tConfig) override;
        virtual bool isConnected() override;
        virtual const QString &getDeviceName() const override;
        DriverInfo *getDriverInfo() override;
        DeviceInfo *getDeviceInfo() override;
        virtual Properties getProperties() override;
        uint32_t getDriverInterface() override;
        QString getDriverVersion() override;
        virtual INDI::BaseDevice *getBaseDevice() override;


        bool getMinMaxStep(const QString &propName, const QString &elementName, double *min, double *max, double *step) override;
        IPState getState(const QString &propName) override;
        IPerm getPermission(const QString &propName) override;
        INDI::Property getProperty(const QString &propName) override;
        bool getJSONProperty(const QString &propName, QJsonObject &propObject, bool compact) override;
        bool getJSONBLOB(const QString &propName, const QString &elementName, QJsonObject &blobObject) override;
        bool setJSONProperty(const QString &propName, const QJsonArray &propElements) override;

    public slots:
        virtual bool Connect() override;
        virtual bool Disconnect() override;
        virtual bool runCommand(int command, void *ptr = nullptr) override;
        virtual bool setProperty(QObject *) override;

    protected:
        INDI::BaseDevice *baseDevice { nullptr };
        ClientManager *clientManager { nullptr };
        GDInterface *interfacePtr { nullptr };

    private:
        bool m_isProxy { false };
};

/**
 * @class ST4
 * ST4 is a special class that handles ST4 commands. Since ST4 functionality can be part of a stand alone ST4 device,
 * or as part of a larger device as CCD or Telescope, it is handled separately to enable one ST4 device regardless of the parent device type.
 *
 *  ST4 is a hardware port dedicated to sending guiding correction pulses to the mount.
 *
 * @author Jasem Mutlaq
 */
class ST4
{
    public:
        ST4(INDI::BaseDevice *bdv, ClientManager *cm);
        ~ST4() = default;

        bool doPulse(GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs);
        bool doPulse(GuideDirection dir, int msecs);
        void setDECSwap(bool enable);
        const QString &getDeviceName();

    private:
        INDI::BaseDevice *baseDevice { nullptr };
        ClientManager *clientManager { nullptr };
        bool swapDEC { false };
        QString m_Name;
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
