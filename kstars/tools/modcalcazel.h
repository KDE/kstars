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
class KStarsDateTime;
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
	void slotUtChecked();
	void slotDateChecked();
	void slotRaChecked();
	void slotDecChecked();
	void slotEpochChecked();
	void slotLongChecked();
	void slotLatChecked();
	void slotAzChecked();
	void slotElChecked();
	void slotInputFile();
	void slotOutputFile();
	void slotRunBatch();

private:

	/**@returns a SkyPoint constructed from the coordinates in the 
	 * RA and Dec dmsBoxes. */
	SkyPoint getEquCoords(void);

	/**@returns a SkyPoint constructed from the coordinates in the 
	 * Az and El dmsBoxes. */
	SkyPoint getHorCoords(void);

	/**Fill the Time and Date fields with the current values from the 
	 * CPU clock. */
	void showCurrentDateTime(void);

	/**@returns a KStarsDateTime constructed from the Time and Date fields. */
	KStarsDateTime getDateTime (void);

	/**Parse the string argument as a double
	 * @param eName    String from which the epoch is to be constructed
	 *                 once it is converted to a double
	 */
	double getEpoch (QString eName);

	/**Fill the Az and El dmsBoxes with values of the SkyPoint argument.
	 * @param sp   SkypPoint object which contains the coordinates to 
	 *             be displayed */
	void showHorCoords ( SkyPoint sp );

	/**Fill the Az and El dmsBoxes with values of the SkyPoint argument. 
	 * @param sp   SkypPoint object which contains the coordinates to 
	 *             be displayed 
	 * @param jd   Julian day for which the conversion has been performed
	 *             The epoch is constructed for that day
	 */
	void showEquCoords ( SkyPoint sp );
	
	/**Fills the epoch box with the value corresponding to a julian day
	 * @param dt   date/time from which to construct the epoch string
	 */
	void showEpoch ( const KStarsDateTime &dt );

	/* Creates a dms object from the latitude box */
	dms getLatitude (void);

	/* Creates a dms object from the longitude box */
	dms getLongitude (void);
	
	void initGeo(void);
	
	void showLongLat(void);

	void getGeoLocation (void);

	void horNoCheck();
	void equNoCheck();
	void processLines( QTextStream &istream );
  
	GeoLocation *geoPlace;
	bool horInputCoords;

};

#endif
