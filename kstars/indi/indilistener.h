/*  INDI Listener
    Copyright (C) 2012 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    Handle INDI Standard properties.
 */

#ifndef INDILISETNER_H
#define INDILISETNER_H

#include <libindi/indiproperty.h>
#include <QObject>

#include "indi/indistd.h"

class ClientManager;
class FITSViewer;
class DriverInfo;

class INDIListener : public QObject
{
    Q_OBJECT
public:

    static INDIListener *Instance();

    void addClient(ClientManager *cm);
    void removeClient(ClientManager *cm);

    void setISOMode(bool enable) { ISOMode = enable; }
    void setBatchMode(bool enable) { batchMode = enable; }

    QList<ISD::GDInterface *> getDevices() { return devices; }

    int size() { return devices.size(); }

    bool isStandardProperty(const QString &name);

    ISD::GDInterface * getDevice(const QString &name);

  private:
    INDIListener();
    static INDIListener * _INDIListener;
    QList<ClientManager *> clients;
    QList<ISD::GDInterface *> devices;

    bool batchMode;
    bool ISOMode;

    FITSViewer * fv;

public slots:

    void registerProperty(INDI::Property *prop);
    void removeProperty(INDI::Property *prop);
    void processDevice(DriverInfo *dv);
    void processSwitch(ISwitchVectorProperty *svp);
    void processText(ITextVectorProperty* tvp);
    void processNumber(INumberVectorProperty *nvp);
    void processLight(ILightVectorProperty *lvp);
    void processBLOB(IBLOB *bp);
    void removeDevice(DriverInfo *dv);

signals:
    void newDevice(ISD::GDInterface*);
    void newTelescope(ISD::GDInterface *);
    void newCCD(ISD::GDInterface *);
    void newFilter(ISD::GDInterface *);
    void newFocuser(ISD::GDInterface *);
    void deviceRemoved(ISD::GDInterface *);

};

#endif // INDILISETNER_H
