/***************************************************************************
                          modcalcapcoord.h  -  description
                             -------------------
    begin                : Wed Apr 10 2002
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

#ifndef MODCALCAPCOORD_H
#define MODCALCAPCOORD_H

#include <qvbox.h>

/** Module to compute the equatorial coordinates for a given date and time 
 * from a given epoch or equinox
  *@author Pablo de Vicente
  */

class QWidget;
class QVBox;
class QLineEdit;
class QString;
class dms;
class dmsBox;
class SkyPoint;
class QDateTime;
class timeBox;

class modCalcApCoord : public QVBox  {

Q_OBJECT
public: 
	modCalcApCoord(QWidget *p, const char *n); 
	~modCalcApCoord();

	long double epochToJd (double epoch);
	SkyPoint precess (dms ra0, dms dec0, long double j0, long double jf);
	SkyPoint apparentCoordinates (dms r0, dms d0, long double j0, long double jf);

public slots:


  /** No descriptions */
  void slotComputeCoords();
  /** No descriptions */
  void slotClearCoords();

private:
	SkyPoint getEquCoords(void);
	void showCurrentTime(void);
	QDateTime getQDateTime (void);
	long double computeJdFromCalendar (void);
	double getEpoch (QString eName);
	void showEquCoords ( SkyPoint sp );

	QVBox *rightBox;
	QLineEdit *rafName, *decfName, *ra0Name, *dec0Name, *epoch0Name;
	dmsBox *ra0Box, *dec0Box, *rafBox, *decfBox;
	timeBox *datBox, *timBox;
	
};

#endif
