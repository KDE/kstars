/***************************************************************************
                          modcalcazel.h  -  description
                             -------------------
    begin                : mier abr 20 2004
    copyright            : (C) 2004 by Pablo de Vicente
    email                : pvicentea@wanadoo.es
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef MODCALCEQUINOX_H
#define MODCALCEQUINOX_H

#include <kapplication.h>
#include <qwidget.h>
#include "modcalcequinoxdlg.h"

class QWidget;
class QTextStream;

/**
  *@author Pablo de Vicente
  */

class modCalcEquinox : public modCalcEquinoxDlg  {

Q_OBJECT

public: 
	modCalcEquinox(QWidget *p, const char *n);
	~modCalcEquinox();
	
public slots:

	void slotComputeEquinoxesAndSolstices(void);
	void slotClear(void);
	void slotYearCheckedBatch();
	void slotInputFile();
	void slotOutputFile();
	void slotRunBatch();

private:

	int getYear (QString eName);
	void showCurrentYear (void);
	void showStartDateTime(long double jd);
	void showSeasonDuration(float deltaJd);
	void processLines( QTextStream &istream );
  
};

#endif
