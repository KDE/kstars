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
class TQTime;
class ExtDate;

class modCalcSidTime : public modCalcSidTimeDlg  {

Q_OBJECT

public:

	modCalcSidTime(TQWidget *p, const char *n);
	~modCalcSidTime();

	TQTime computeUTtoST (TQTime u, ExtDate d, dms l);
	TQTime computeSTtoUT (TQTime s, ExtDate d, dms l);

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
	void processLines( TQTextStream &istream );

private:

	void showUT ( TQTime ut );
	void showST ( TQTime st );

	/* Fills the UT, Date boxes with the current time 
	 * and date and the longitude box with the current Geo location 
	 */
	void showCurrentTimeAndLong (void);

	void sidNoCheck();
	void utNoCheck();

	TQTime getUT (void);
	TQTime getST (void);
	ExtDate getDate (void);
	dms getLongitude (void);
	bool stInputTime;
	
};

#endif
