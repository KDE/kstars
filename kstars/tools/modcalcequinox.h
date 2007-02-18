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

#ifndef MODCALCEQUINOX_H_
#define MODCALCEQUINOX_H_

#include "ui_modcalcequinox.h"

class QTextStream;

/**
  *@author Pablo de Vicente
  */
class modCalcEquinox : public QFrame, public Ui::modCalcEquinoxDlg  {

Q_OBJECT

public: 
	modCalcEquinox(QWidget *p);
	~modCalcEquinox();
	
public slots:

	void slotCompute();
	void slotYearCheckedBatch();
	void slotInputFile();
	void slotOutputFile();
	void slotRunBatch();

private:

	void showCurrentYear (void);
	void processLines( QTextStream &istream );
  
};

#endif
