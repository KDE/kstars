/*  Telescope wizard
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef TELESCOPEWIZARDPROCESS_H
#define TELESCOPEWIZARDPROCESS_H

#include <qstringlist.h>

#include "telescopewizard.h"

class KStars;
class INDIMenu;
class INDIDriver;
class QTimer;
class INDI_D;

class KProgressDialog;

class telescopeWizardProcess : public telescopeWizard
{

Q_OBJECT

public:
	telescopeWizardProcess( QWidget* parent = 0, const char* name = 0);
	~telescopeWizardProcess();

	unsigned int currentPage;
	enum { INTRO_P=0, MODEL_P=1, TELESCOPE_P=2, LOCAL_P=3, PORT_P=4 };

private:
	KStars * ksw;
	INDIMenu   *indimenu;
	INDIDriver *indidriver;
	QTimer *newDeviceTimer;

	INDI_D *indiDev;

	KProgressDialog *progressScan;

	QStringList portList;
	QString currentDevice;

	int currentPort;
	int timeOutCount;
	bool INDIMessageBar;
        bool linkRejected;

	int establishLink();
	void Reset();

public slots:
	void processNext();
	void processBack();
	void newTime();
	void newLocation();
	void processPort();
	void scanPorts();
	void linkSuccess();

};




#endif
