/*  INDI STD
    Copyright (C) 2012 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    Handle INDI Standard properties.
 */

#ifndef INDISTD_H
#define INDISTD_H

#include <indiproperty.h>
#include <QObject>

#include "skypoint.h"
#include "indicommon.h"
#include "fitsviewer/fitscommon.h"

#include "config-kstars.h"

#define MAXINDIFILENAME 512

class ClientManager;
class DriverInfo;
class DeviceInfo;
class QTimer;

// INDI Standard Device Namespace
namespace ISD
{


class GDSetCommand : public QObject
{
    Q_OBJECT

public:
    GDSetCommand(INDI_PROPERTY_TYPE inPropertyType, const QString & inProperty, const QString & inElement, QVariant qValue, QObject *parent);
    INDI_PROPERTY_TYPE   propType;

    QString indiProperty;
    QString indiElement;
    QVariant elementValue;

};

/**
 * @class GDInterface
 *  GDInterface is the Generic Device <i>Interface</i> for INDI devices. It is used as part of the Decorater Pattern when initially a new INDI device is created as a
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
    virtual void registerProperty(INDI::Property *prop) =0;
    virtual void removeProperty(INDI::Property *prop) =0;
    virtual void processSwitch(ISwitchVectorProperty *svp)=0;
    virtual void processText(ITextVectorProperty* tvp)=0;
    virtual void processNumber(INumberVectorProperty *nvp)=0;
    virtual void processLight(ILightVectorProperty *lvp)=0;
    virtual void processBLOB(IBLOB *bp)=0;
    virtual void processMessage(int messageID)=0;

    // Accessors
    virtual QList<INDI::Property *> getProperties() =0;
    virtual DeviceFamily getType()=0;
    virtual DriverInfo * getDriverInfo() = 0;
    virtual DeviceInfo * getDeviceInfo() = 0;
    virtual INDI::BaseDevice* getBaseDevice()=0;

    // Convenience functions
    virtual bool setConfig(INDIConfig tConfig)=0;
    virtual const char *getDeviceName()=0;
    virtual bool isConnected()=0;
    virtual bool getMinMaxStep(const QString & propName, const QString & elementName, double *min, double *max, double *step)=0;
    virtual IPState getState(const QString &propName)=0;
    virtual IPerm getPermission(const QString &propName)=0;

    virtual ~GDInterface() {}

protected:
    DeviceFamily dType;
    QList<INDI::Property *> properties;

public slots:
    virtual bool Connect()=0;
    virtual bool Disconnect()=0;
    virtual bool runCommand(int command, void *ptr=NULL)=0;
    virtual bool setProperty(QObject *)=0;

signals:
    void Connected();
    void Disconnected();
    void switchUpdated(ISwitchVectorProperty *svp);
    void textUpdated(ITextVectorProperty* tvp);
    void numberUpdated(INumberVectorProperty *nvp);
    void lightUpdated(ILightVectorProperty *lvp);
    void BLOBUpdated(IBLOB *bp);
    void messageUpdated(int messageID);

    void propertyDefined(INDI::Property *prop);
    void propertyDeleted(INDI::Property *prop);


};

/**
 * @class GenericDevice
 * GenericDevice is the Generic Device for INDI devices. When a new INDI device is created in INDIListener, it gets created as a GenericDevice initially. If the device
 * registers a standard property that is a key property to a device type family (e.g. Number property EQUATORIAL_EOD_COORD signifies a Telescope device), then the specialized version of
 * the device is exnteded via the Decorater Pattern.
 *
 * GenericDevice handles common functions shared across many devices such as time and location handling, configuration processing, retrieving information about properties, driver info..etc.
 *
 * @author Jasem Mutlaq
 */
class GenericDevice : public GDInterface
{
    Q_OBJECT

public:

    GenericDevice(DeviceInfo* idv);
    ~GenericDevice();

    virtual void registerProperty(INDI::Property *prop);
    virtual void removeProperty(INDI::Property *prop);
    virtual void processSwitch(ISwitchVectorProperty *svp);
    virtual void processText(ITextVectorProperty* tvp);
    virtual void processNumber(INumberVectorProperty *nvp);
    virtual void processLight(ILightVectorProperty *lvp);
    virtual void processBLOB(IBLOB *bp);
    virtual void processMessage(int messageID);

    virtual DeviceFamily getType() { return dType; }
    virtual const char *getDeviceName();
    virtual DriverInfo * getDriverInfo() { return driverInfo;}
    virtual DeviceInfo * getDeviceInfo() { return deviceInfo;}
    virtual QList<INDI::Property *> getProperties() { return properties; }


    virtual bool setConfig(INDIConfig tConfig);
    virtual bool isConnected() { return connected; }
    virtual INDI::BaseDevice* getBaseDevice() { return baseDevice;}
    virtual bool getMinMaxStep(const QString & propName, const QString & elementName, double *min, double *max, double *step);
    virtual IPState getState(const QString &propName);
    virtual IPerm getPermission(const QString &propName);

public slots:
    virtual bool Connect();
    virtual bool Disconnect();
    virtual bool runCommand(int command, void *ptr=NULL);
    virtual bool setProperty(QObject *);

protected slots:
    virtual void resetWatchdog();

protected:
    void createDeviceInit();
    void updateTime();
    void updateLocation();

private:
    bool connected;
    DriverInfo *driverInfo;
    DeviceInfo *deviceInfo;
    INDI::BaseDevice *baseDevice;
    ClientManager *clientManager;
    QTimer *watchDogTimer;
    char BLOBFilename[MAXINDIFILENAME];

};

/**
 * @class DeviceDecorator
 * DeviceDecorator is the base decorater for all specialized devices. It extends the functionality of GenericDevice.
 *
 * @author Jasem Mutlaq
 */
class DeviceDecorator : public GDInterface
{
    Q_OBJECT

public:


    DeviceDecorator(GDInterface *iPtr);
    ~DeviceDecorator();

    virtual void registerProperty(INDI::Property *prop);
    virtual void removeProperty(INDI::Property *prop);
    virtual void processSwitch(ISwitchVectorProperty *svp);
    virtual void processText(ITextVectorProperty* tvp);
    virtual void processNumber(INumberVectorProperty *nvp);
    virtual void processLight(ILightVectorProperty *lvp);
    virtual void processBLOB(IBLOB *bp);
    virtual void processMessage(int messageID);

    virtual DeviceFamily getType();

    virtual bool setConfig(INDIConfig tConfig);
    virtual bool isConnected();
    const char *getDeviceName();
    DriverInfo *getDriverInfo();
    DeviceInfo * getDeviceInfo();
    QList<INDI::Property *> getProperties();
    virtual INDI::BaseDevice* getBaseDevice();

    bool getMinMaxStep(const QString & propName, const QString & elementName, double *min, double *max, double *step);
    IPState getState(const QString &propName);
    IPerm getPermission(const QString &propName);


public slots:
    virtual bool Connect();
    virtual bool Disconnect();
    virtual bool runCommand(int command, void *ptr=NULL);
    virtual bool setProperty(QObject *);

protected:
    INDI::BaseDevice *baseDevice;
    ClientManager *clientManager;
    GDInterface *interfacePtr;

};

/**
 * @class ST4
 * ST4 is a special class that handles ST4 commands. Since ST4 functionalty can be part of a stand alone ST4 device,
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
    ~ST4();

    bool doPulse(GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs );
    bool doPulse(GuideDirection dir, int msecs );
    void setDECSwap(bool enable);
    const char *getDeviceName();

private:
    INDI::BaseDevice *baseDevice;
    ClientManager *clientManager;
    bool swapDEC;

};

}

#endif // INDISTD_H
