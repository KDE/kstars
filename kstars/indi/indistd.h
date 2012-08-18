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

#include <libindi/indiproperty.h>
#include <QObject>

#include "skypoint.h"
#include "indicommon.h"


class ClientManager;
class FITSViewer;
class DriverInfo;
class StreamWG;

// INDI Standard Device Namespace
namespace ISD
{


class GDSetCommand : public QObject
{
    Q_OBJECT

public:
    GDSetCommand(INDI_TYPE inPropertyType, const QString & inProperty, const QString & inElement, QVariant qValue, QObject *parent);

    INDI_TYPE   propType;
    QString indiProperty;
    QString indiElement;
    QVariant elementValue;

};

class GDInterface : public QObject
{
    Q_OBJECT

public:
    virtual void registerProperty(INDI::Property *prop) =0;

    virtual void removeProperty(INDI::Property *prop) =0;
    virtual void processSwitch(ISwitchVectorProperty *svp)=0;
    virtual void processText(ITextVectorProperty* tvp)=0;
    virtual void processNumber(INumberVectorProperty *nvp)=0;
    virtual void processLight(ILightVectorProperty *lvp)=0;
    virtual void processBLOB(IBLOB *bp)=0;
    virtual DeviceFamily getType()=0;
    virtual const char *getDeviceName()=0;
    virtual DriverInfo * getDriverInfo() = 0;
    virtual QList<INDI::Property *> getProperties() =0;
    virtual bool isConnected()=0;

    virtual ~GDInterface() {}

protected:
    DeviceFamily dType;
    QList<INDI::Property *> properties;

public slots:
    virtual bool runCommand(int command, void *ptr=NULL)=0;
    virtual void setProperty(QObject *)=0;

signals:
    void Connected();
    void Disconnected();
    void switchUpdated(ISwitchVectorProperty *svp);
    void textUpdated(ITextVectorProperty* tvp);
    void numberUpdated(INumberVectorProperty *nvp);
    void lightUpdated(ILightVectorProperty *lvp);
    void BLOBUpdated(IBLOB *bp);

    void propertyDefined(INDI::Property *prop);
    void propertyDeleted(INDI::Property *prop);


};

class GenericDevice : public GDInterface
{

    Q_OBJECT

public:

    GenericDevice(DriverInfo* idv);
    ~GenericDevice();

    virtual void registerProperty(INDI::Property *prop);
    virtual void removeProperty(INDI::Property *prop);
    virtual void processSwitch(ISwitchVectorProperty *svp);
    virtual void processText(ITextVectorProperty* tvp);
    virtual void processNumber(INumberVectorProperty *nvp);
    virtual void processLight(ILightVectorProperty *lvp);
    virtual void processBLOB(IBLOB *bp);
    virtual DeviceFamily getType() { return dType; }
    virtual const char *getDeviceName();
    virtual DriverInfo * getDriverInfo() { return driverInfo;}
    virtual QList<INDI::Property *> getProperties() { return properties; }
    virtual bool isConnected() { return connected; }


public slots:
    virtual bool runCommand(int command, void *ptr=NULL);
    virtual void setProperty(QObject *);

protected:
    void createDeviceInit();
    void processDeviceInit();

    void updateTime();
    void updateLocation();

private:
    bool driverTimeUpdated;
    bool driverLocationUpdated;
    bool connected;

    DriverInfo *driverInfo;
    INDI::BaseDevice *baseDevice;
    ClientManager *clientManager;

};

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
    virtual DeviceFamily getType();
    virtual bool isConnected();
    const char *getDeviceName();
    DriverInfo *getDriverInfo();
    QList<INDI::Property *> getProperties();

public slots:
    virtual bool runCommand(int command, void *ptr=NULL);
    virtual void setProperty(QObject *);

protected:
    INDI::BaseDevice *baseDevice;
    ClientManager *clientManager;
    GDInterface *interfacePtr;

};

class Telescope : public DeviceDecorator
{
    Q_OBJECT

public:
    Telescope(GDInterface *iPtr) : DeviceDecorator(iPtr) { dType = KSTARS_TELESCOPE;}

    void registerProperty(INDI::Property *prop);
    void processSwitch(ISwitchVectorProperty *svp);
    void processText(ITextVectorProperty* tvp);
    void processNumber(INumberVectorProperty *nvp);

    DeviceFamily getType() { return dType;}

protected:
    void Slew(SkyPoint *ScopeTarget);

public slots:
    virtual bool runCommand(int command, void *ptr=NULL);

private:

    SkyPoint currentCoord;

};

class CCD : public DeviceDecorator
{
    Q_OBJECT

public:
    CCD(GDInterface *iPtr);
    ~CCD();

    void registerProperty(INDI::Property *prop);
    void processSwitch(ISwitchVectorProperty *svp);
    void processText(ITextVectorProperty* tvp);
    void processNumber(INumberVectorProperty *nvp);
    void processLight(ILightVectorProperty *lvp);
    void processBLOB(IBLOB *bp);

    DeviceFamily getType() { return dType;}

    void setFocusMode(bool enable) { focusMode = enable; }
    void setISOMode(bool enable) { ISOMode = enable; }
    void setBatchMode(bool enable) { batchMode = enable; }
    void setSeqPrefix(const QString &preFix) { seqPrefix = preFix; }
    void setSeqCount(int count) { seqCount = count; }

    FITSViewer *getViewer() { return fv;}
    int getFocusTabID() { return focusTabID; }

public slots:
    bool runCommand(int command, void *ptr=NULL);
    void FITSViewerDestroyed();
    void StreamWindowDestroyed();

private:
    bool focusMode;
    bool batchMode;
    bool ISOMode;
    QString		seqPrefix;
    int seqCount;
    int focusTabID;
    FITSViewer * fv;
    StreamWG *streamWindow;

};

class Filter : public DeviceDecorator
{
    Q_OBJECT

public:
    Filter(GDInterface *iPtr) : DeviceDecorator(iPtr) { dType = KSTARS_FILTER;}

    void registerProperty(INDI::Property *prop);
    void processSwitch(ISwitchVectorProperty *svp);
    void processText(ITextVectorProperty* tvp);
    void processNumber(INumberVectorProperty *nvp);
    void processLight(ILightVectorProperty *lvp);

    DeviceFamily getType() { return dType;}

};

class Focuser : public DeviceDecorator
{
    Q_OBJECT

public:
    Focuser(GDInterface *iPtr) : DeviceDecorator(iPtr) { dType = KSTARS_FOCUSER;}

    void registerProperty(INDI::Property *prop);
    void processSwitch(ISwitchVectorProperty *svp);
    void processText(ITextVectorProperty* tvp);
    void processNumber(INumberVectorProperty *nvp);
    void processLight(ILightVectorProperty *lvp);

    DeviceFamily getType() { return dType;}

public slots:
    bool runCommand(int command, void *ptr=NULL);

};

}

#endif // INDISTD_H
