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

#include <qvbox.h>
#include <kapplication.h>  // KDE_VERSION is defined here (KDE < 3.0)

/**
  * Class which implements the KStars calculator module to compute Universal
  * time to/from Sidereal time.
  *
  * Inherits QWidget
  *@author Pablo de Vicente
	*@version 0.9
  */

class QWidget;
class dms;
class dmsBox;
class timeBox;
class QTime;
class QDate;
class QRadioButton;
class QTimeEdit;
class QDateEdit;

class modCalcSidTime : public QWidget  {

Q_OBJECT

public:

	modCalcSidTime(QWidget *p, const char *n);
	~modCalcSidTime();

	QTime computeUTtoST (QTime u, QDate d, dms l);
	QTime computeSTtoUT (QTime s, QDate d, dms l);

public slots:	
	

	/** No descriptions */
	void slotClearFields();

	/** No descriptions */
	void slotComputeTime();

private:

	void showUT ( QTime d );
	void showST ( QTime d );

	/* Fills the UT, Date boxes with the current time 
	 * and date and the longitude box with the current Geo location 
	 */
	void showCurrentTimeAndLong (void);

	QTime getUT (void);
	QTime getST (void);
	QDate getDate (void);
	dms getLongitude (void);
	
	QRadioButton *UtRadio, *StRadio;
	QWidget *rightBox;
#if (KDE_VERSION <= 299)
	timeBox *UtBox, *StBox, *datBox;
#else
	QTimeEdit *UtBox, *StBox;
	QDateEdit *datBox;
#endif
	dmsBox *longBox;

};

#endif
