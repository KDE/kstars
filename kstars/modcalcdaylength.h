/***************************************************************************
                          modcalcdaylength.h  -  description
                             -------------------
    begin                : wed jun 12 2002
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

#ifndef MODCALCDAYLENGTH_H
#define MODCALCDAYLENGTH_H

#include <qvbox.h>

/** Module to compute the equatorial coordinates for a given date and time 
 * from a given epoch or equinox
  *@author Pablo de Vicente
  */

class QWidget;
class QVBox;
class dms;
class dmsBox;
class GeoLocation;
class QDateTime;
class timeBox;


class modCalcDayLength : public QVBox  {
Q_OBJECT
public: 

/**Constructor. */
	modCalcDayLength(QWidget *p, const char *n);
/**Destructor. */
	~modCalcDayLength();

public slots:

  /** No descriptions */
  void slotComputePosTime();
  /** No descriptions */
  void slotClearCoords();

private:
/**@returns a SkyPoint constructed from the coordinates in the RA and Dec dmsBoxes. */
	QTime lengthOfDay(QTime setQTime, QTime riseQTime);

/**Fills the Date fields with the current values from the current date. */
	void showCurrentDate(void);

/**@returns a QDateTime constructed from the Time and Date fields. */
	QDateTime getQDateTime (void);

/**@returns a GeoLocation constructed from the Longitude and Latitude fields.
 * Height is arbitrarily set to 0.0 */

	void getGeoLocation (void);
	
	void initGeo(void);


/**Convert the Time and Date to a Julian Day. */
	long double computeJdFromCalendar (void);

/**Fills the Longitude and Latitude with the current location ones */
	void showLongLat(void);

	QVBox *rightBox;
	dmsBox *azSetBox, *azRiseBox, *elTransitBox, *longBox, *latBox;
	timeBox *riseTimeBox, *setTimeBox, *transitTimeBox, *dayLBox, *datBox;

	GeoLocation *geoPlace;
};

#endif
