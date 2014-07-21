#ifndef EKOSMANAGER_H
#define EKOSMANAGER_H

/*  Ekos
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */


#include <baseclient.h>

#include "ui_ekosmanager.h"
#include "indi/indistd.h"
#include "capture.h"
#include "focus.h"
#include "guide.h"
#include "align.h"

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
    void refreshRemoteDrivers();

public slots:
    void processINDI();
    void connectDevices();
    void disconnectDevices();
    void cleanDevices();

    void processNewDevice(ISD::GDInterface*);
    void processNewProperty(INDI::Property*);
    void processNewNumber(INumberVectorProperty *nvp);
    void processNewText(ITextVectorProperty *tvp);

    void updateLog();
    void clearLog();

    void processTabChange();

    void removeDevice(ISD::GDInterface*);

    void deviceConnected();
    void deviceDisconnected();

    void processINDIModeChange();
    void checkINDITimeout();

    void setTelescope(ISD::GDInterface *);
    void setCCD(ISD::GDInterface *);
    void setFilter(ISD::GDInterface *);
    void setFocuser(ISD::GDInterface *);
    void setST4(ISD::ST4 *);

 private:

    void loadDefaultDrivers();
    void saveDefaultDrivers();
    void removeTabs();
    void reset();
    void initCapture();
    void initFocus();
    void initGuide();
    void initAlign();

    void initLocalDrivers();
    void initRemoteDrivers();

    void processLocalDevice(ISD::GDInterface*);
    void processRemoteDevice(ISD::GDInterface*);
    bool isRunning(const QString &process);

    //bool useGuiderFromCCD;
    //bool useFilterFromCCD;
    bool useGuideHead;
    bool useST4;
    bool guideStarted;
    bool ccdStarted;

    ISD::GDInterface *scope, *ccd, *guider, *focuser, *filter, *aux, *ao;
    DriverInfo *scope_di, *ccd_di, *guider_di, *filter_di, *focuser_di, *aux_di, *ao_di, *remote_indi;

    Ekos::Capture *captureProcess;
    Ekos::Focus *focusProcess;
    Ekos::Guide *guideProcess;
    Ekos::Align *alignProcess;

    QString guiderName;


    bool localMode;

    unsigned short nDevices;
    QList<DriverInfo *> managedDevices;

    QHash<QString, DriverInfo *> driversList;

    QStringList logText;



};


#endif // EKOS_H
