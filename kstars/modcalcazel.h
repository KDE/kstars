/***************************************************************************
                          modcalcazel.h  -  description
                             -------------------
    begin                : sáb oct 26 2002
    copyright            : (C) 2002 by Pablo de Vicente
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

#ifndef MODCALCAZEL_H
#define MODCALCAZEL_H

#include <kapplication.h>
#include <qwidget.h>
#include "modcalcazeldlg.h"

class QWidget;
class SkyPoint;
class QDateTime;
class GeoLocation;
class dms;

/**
  *@author Pablo de Vicente
  */

class modCalcAzel : public modCalcAzelDlg  {

Q_OBJECT

public: 
	modCalcAzel(QWidget *p, const char *n);
	~modCalcAzel();
	
public slots:

  /** No descriptions */
  void slotComputeCoords();
  /** No descriptions */
  void slotClearCoords();

private:
  
/**@returns a SkyPoint constructed from the coordinates in the RA and Dec dmsBoxes. */
	SkyPoint getEquCoords(void);

/**Fill the Time and Date fields with the current values from the CPU clock. */
	void showCurrentDateTime(void);

/**@returns a QDateTime constructed from the Time and Date fields. */
	QDateTime getQDateTime (void);

/**Convert the Time and Date to a Julian Day. */
	long double computeJdFromCalendar (void);

/**Parse the string argument as a double */
	double getEpoch (QString eName);

	long double epochToJd (double epoch);

/**Fill the Az and El dmsBoxes with values of the SkyPoint argument. */
	void showHorCoords ( SkyPoint sp );

	dms getLatitude (void);

	dms getLongitude (void);
	
	void initGeo(void);
	
	void showLongLat(void);

	dms DateTimetoLST (QDateTime utdt, dms longitude);

	dms QTimeToDMS(QTime qtime); 

	void getGeoLocation (void);

	GeoLocation *geoPlace;

};

#endif
