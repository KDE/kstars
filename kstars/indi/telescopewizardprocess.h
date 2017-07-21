/*  Telescope wizard
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "ui_telescopewizard.h"
#include "indi/indistd.h"

#include <QHash>
#include <QStringList>

#include <memory>

class DriverInfo;

class QProgressDialog;

class telescopeWizardProcess : public QDialog
{
    Q_OBJECT

  public:
    explicit telescopeWizardProcess(QWidget *parent = nullptr);
    ~telescopeWizardProcess();

    unsigned int currentPage;
    enum
    {
        INTRO_P     = 0,
        MODEL_P     = 1,
        TELESCOPE_P = 2,
        LOCAL_P     = 3,
        PORT_P      = 4
    };

private:
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

private:
    std::unique_ptr<Ui::telescopeWizard> ui;
    ISD::GDInterface *scopeDevice { nullptr };
    QProgressDialog *progressScan { nullptr };
    QStringList portList;
    int currentPort { -1 };
    bool INDIMessageBar { false };
    bool linkRejected { false };
    QHash<QString, DriverInfo *> driversList;
    QList<DriverInfo *> managedDevice;
};
