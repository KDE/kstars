/***************************************************************************
                          modcalcsidtime.h  -  description
                             -------------------
    begin                : Wed Jan 23 2002
    copyright            : (C) 2002 by Pablo de Vicente
    email                : vicente@oan.es
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef MODCALCSIDTIME_H
#define MODCALCSIDTIME_H

#include "modcalcsidtimedlg.h"
#include <kapplication.h>

/**
  * Class which implements the KStars calculator module to compute Universal
  * time to/from Sidereal time.
  *
  * Inherits modCalcSidTimeDlg
  *@author Pablo de Vicente
	*@version 0.9
  */

class dms;
class QTime;
class ExtDate;

class modCalcSidTime : public modCalcSidTimeDlg  {

Q_OBJECT

public:

	modCalcSidTime(QWidget *p, const char *n);
	~modCalcSidTime();

	QTime computeUTtoST (QTime u, ExtDate d, dms l);
	QTime computeSTtoUT (QTime s, ExtDate d, dms l);

public slots:	
	

	/** No descriptions */
	void slotClearFields();

	/** No descriptions */
	void slotComputeTime();

	void slotUtChecked();
	void slotDateChecked();
	void slotStChecked();
	void slotLongChecked();
	void slotInputFile();
	void slotOutputFile();
	void slotRunBatch();
	void processLines( QTextStream &istream );

private:

	void showUT ( QTime ut );
	void showST ( QTime st );

	/* Fills the UT, Date boxes with the current time 
	 * and date and the longitude box with the current Geo location 
	 */
	void showCurrentTimeAndLong (void);

	void sidNoCheck();
	void utNoCheck();

	QTime getUT (void);
	QTime getST (void);
	ExtDate getDate (void);
	dms getLongitude (void);
	bool stInputTime;
	
};

#endif
