#ifndef EKOSMANAGER_H
#define EKOSMANAGER_H

/*  Ekos
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */


#include <libindi/baseclient.h>

#include "ui_ekosmanager.h"
#include "indi/indistd.h"
#include "capture.h"
#include "focus.h"
#include "guide.h"

#include <QDialog>
#include <QHash>

class DriverInfo;
class ClientManager;
class QTimer;

class EkosManager : public QDialog, public Ui::EkosManager
{
    Q_OBJECT

public:
    EkosManager(  );
    ~EkosManager();

    void appendLogText(const QString &);

public slots:
    void processINDI();
    void connectDevices();
    void disconnectDevices();
    void cleanDevices();

    void processNewDevice(ISD::GDInterface*);
    void processNewProperty(INDI::Property*);

    void updateLog();
    void clearLog();

    void removeDevice(ISD::GDInterface*);

    void setTelescope(ISD::GDInterface *);
    void setCCD(ISD::GDInterface *);
    void setFilter(ISD::GDInterface *);
    void setFocuser(ISD::GDInterface *);

 private:

    void loadDefaultDrivers();
    void saveDefaultDrivers();
    void reset();
    void initCapture();
    void initFocus();
    void initGuide();
    bool useGuiderFromCCD;
    bool useFilterFromCCD;

    ISD::GDInterface *scope, *ccd, *guider, *focuser, *filter;
    DriverInfo *scope_di, *ccd_di, *guider_di, *filter_di, *focuser_di;

    Ekos::Capture *captureProcess;
    Ekos::Focus *focusProcess;
    Ekos::Guide *guideProcess;

    unsigned short nDevices;
    QList<DriverInfo *> managedDevices;

    QHash<QString, DriverInfo *> driversList;

    QStringList logText;



};


#endif // EKOS_H
