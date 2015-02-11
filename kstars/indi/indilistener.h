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

#include <indiproperty.h>
#include <QObject>

#include "indi/indistd.h"

class ClientManager;
class FITSViewer;
class DeviceInfo;

/**
 * @class INDIListener is responsible for creating ISD::GDInterface generic devices as new devices arrive from ClientManager. It can support multiple ClientManagers
 * and will first create a generic INDI device. Upon arrival of INDI properties, INDIListener can create specialized devices (e.g. Telescope) if it detects key Standard INDI
 * property that signifies a particular device family. The generic device functionality is extended via the Decorator design pattern.
 *
 * INDIListener also delegates INDI properties as they are received from ClientManager to the appropiate device to be processed.
 *
 * @author Jasem Mutlaq
 */
class INDIListener : public QObject
{
    Q_OBJECT
public:

    static INDIListener *Instance();

    void addClient(ClientManager *cm);
    void removeClient(ClientManager *cm);

    ISD::GDInterface * getDevice(const QString &name);
    QList<ISD::GDInterface *> getDevices() { return devices; }

    int size() { return devices.size(); }

    bool isStandardProperty(const QString &name);   

  private:
    INDIListener();
    static INDIListener * _INDIListener;
    QList<ClientManager *> clients;
    QList<ISD::GDInterface *> devices;
    QList<ISD::ST4*> st4Devices;

public slots:

    void registerProperty(INDI::Property *prop);
    void removeProperty(INDI::Property *prop);
    void processDevice(DeviceInfo *dv);
    void processSwitch(ISwitchVectorProperty *svp);
    void processText(ITextVectorProperty* tvp);
    void processNumber(INumberVectorProperty *nvp);
    void processLight(ILightVectorProperty *lvp);
    void processBLOB(IBLOB *bp);
    void removeDevice(DeviceInfo *dv);

signals:
    void newDevice(ISD::GDInterface*);
    void newTelescope(ISD::GDInterface *);
    void newCCD(ISD::GDInterface *);
    void newFilter(ISD::GDInterface *);
    void newFocuser(ISD::GDInterface *);
    void newST4(ISD::ST4*);
    void deviceRemoved(ISD::GDInterface *);


};

#endif // INDILISETNER_H
