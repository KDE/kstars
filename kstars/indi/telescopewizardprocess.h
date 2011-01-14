/*  Telescope wizard
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef TELESCOPEWIZARDPROCESS_H_
#define TELESCOPEWIZARDPROCESS_H_

#include <qstringlist.h>

#include "ui_telescopewizard.h"

class KStars;
class INDIMenu;
class INDIDriver;
class QTimer;
class INDI_D;

class KProgressDialog;

class telescopeWizardProcess : public QDialog
{

    Q_OBJECT

public:
    explicit telescopeWizardProcess( QWidget* parent = 0, const char* name = 0);
    ~telescopeWizardProcess();

    unsigned int currentPage;
    enum { INTRO_P=0, MODEL_P=1, TELESCOPE_P=2, LOCAL_P=3, PORT_P=4 };

private:
    KStars * ksw;
    INDIMenu   *indimenu;
    INDIDriver *indidriver;
    Ui::telescopeWizard *ui;

    INDI_D *indiDev;

    KProgressDialog *progressScan;

    QStringList portList;
    QString currentDevice;

    int currentPort;
    int timeOutCount;
    bool INDIMessageBar;
    bool linkRejected;

    void establishLink();
    void Reset();

public slots:
    void cancelCheck();
    void processNext();
    void processBack();
    void newTime();
    void newLocation();
    void processPort();
    void scanPorts();
    void linkSuccess();

};

#endif
