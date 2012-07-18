#ifndef EKOS_H
#define EKOS_H

/*  Ekos
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */


#include "ui_ekos.h"

#include <QDialog>

class DeviceManager;
class KStars;
class INDI_D;
class INDI_P;
class IDevice;

namespace EkoDevice
{
   class GenericDevice;
   class Telescope;
   class CCD;
   class Focuser;
   class Filter;
}

class Ekos : public QDialog, public Ui::Ekos
{
    Q_OBJECT

public:
    Ekos( KStars *_ks );
    ~Ekos();

public slots:
    void processINDI();
    void connectDevices();
    void disconnectDevices();
    void cleanDevices();
    void processNewDevice(INDI_D *dp);


 private:
    DeviceManager *deviceManager;
    bool useGuiderFromCCD;
    bool useFilterFromCCD;

    EkoDevice::Telescope *scope;
    EkoDevice::CCD *ccd;
    EkoDevice::CCD *guider;
    EkoDevice::Focuser *focuser;
    EkoDevice::Filter *filter;

    unsigned short nDevices;
    QList<IDevice *> processed_devices;

};


#endif // EKOS_H
