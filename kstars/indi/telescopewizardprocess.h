/*  Telescope wizard
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef TELESCOPEWIZARDPROCESS_H_
#define TELESCOPEWIZARDPROCESS_H_

#include <QStringList>
#include <QHash>

#include "indi/indistd.h"
#include "ui_telescopewizard.h"

class KStars;
class QTimer;
class INDI_D;
class DriverInfo;

class QProgressDialog;

class telescopeWizardProcess : public QDialog
{

    Q_OBJECT

public:
    explicit telescopeWizardProcess( QWidget* parent = 0, const char* name = 0);
    ~telescopeWizardProcess();

    unsigned int currentPage;
    enum { INTRO_P=0, MODEL_P=1, TELESCOPE_P=2, LOCAL_P=3, PORT_P=4 };

private:   
    Ui::telescopeWizard *ui;

    ISD::GDInterface *scopeDevice;

    QProgressDialog *progressScan;

    QStringList portList;
    QString currentDevice;

    int currentPort;
    int timeOutCount;
    bool INDIMessageBar;
    bool linkRejected;

    QHash<QString, DriverInfo *> driversList;

    QList<DriverInfo *> managedDevice;

    void establishLink();
    void Reset();

public slots:
    void cancelCheck();
    void processNext();
    void processBack();
    void newTime();
    void newLocation();
    void processTelescope(ISD::GDInterface *);
    void scanPorts();
    void linkSuccess();

};

#endif
