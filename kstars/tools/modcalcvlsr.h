/***************************************************************************
                          modcalcLSRVel.h  -  description
                             -------------------
    begin                : dom mar 13 2005
    copyright            : (C) 2005 by Pablo de Vicente
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

#ifndef MODCALCVLSR_H
#define MODCALCVLSR_H

#include <kapplication.h>
#include <qwidget.h>
#include "modcalcvlsrdlg.h"

class QWidget;
class SkyPoint;
class KStarsDateTime;
class GeoLocation;
class dms;

/**
  *@author Pablo de Vicente
  *Module to compute the heliocentric radial velocity, geocentric radial velocity and 
  *topocentric radial velocity for a source, given its coordinates, its Vlsr and the date and
  *location on the Earth.
  */

class modCalcVlsr : public modCalcVlsrDlg  {

Q_OBJECT

public: 
	modCalcVlsr(QWidget *p, const char *n);
	~modCalcVlsr();
	
public slots:

	void slotClearCoords();
	void slotComputeVelocities();
	void slotUtChecked();
	void slotDateChecked();
	void slotRaChecked();
	void slotDecChecked();
	void slotEpochChecked();
	void slotLongChecked();
	void slotLatChecked();
	void slotHeightChecked();
	void slotVlsrChecked();
	void slotInputFile();
	void slotOutputFile();
	void slotRunBatch();

private:

	/**@returns a SkyPoint constructed from the coordinates in the 
	 * RA and Dec dmsBoxes. */
	SkyPoint getEquCoords(void);

	/* @return a double with the Vlsr from the vlsrBox in the UI
	 */
	double getVLSR (void);
	
	/* @return a double with the Vhel from the vhelBox in the UI
	 */
	double getVhel (void);
	
	/* @return a double with the Vgeo from the vgeoBox in the UI
	 */
	double getVgeo (void);
	
	/* @return a double with the Vtopo from the vtopoBox in the UI
	 */
	double getVtopo (void);

	/**@returns a KStarsDateTime constructed from the Time and Date fields. */
	KStarsDateTime getDateTime (void);

	/**Parse the string argument as a double
	 * @param eName    String from which the epoch is to be constructed
	 *                 once it is converted to a double
	 */
	double getEpoch (QString eName);

	/* Creates a dms object from the latitude box */
	dms getLatitude (void);

	/* Creates a dms object from the longitude box */
	dms getLongitude (void);

	/* creates a double from the height box */ 
	double getHeight(void);
	
	/* Constructs a GeoLocation object from the longitude, latitude and height fields */

	void getGeoLocation(void);

	/**Fill the Time and Date fields with the current values from the 
	 * CPU clock. */
	void showCurrentDateTime(void);

	/* Constructs the a GeoLocation object (geoPlace) from the calling classes.
	 * This is for using as Geolocation the location setup in KStars 
	 * */
	void initGeo(void);

	/* Fills the longitude, latitude and height fields with the values in the 
	 * geoPlace object, which come from the calling classes.
	 */
	void showLongLat(void);

	/**Fills the VLSR box with its value
	 **/
	void showVlsr (const double vlsr );

	/**Fills the Heliocentric velocity box with its value
	 **/
	void showHelVel (const double vhel );

	/**Fills the geocentric velocity box with its value
	 **/
	void showGeoVel (const double vgeo );

	/**Fills the topocentric velocity box with its value
	 **/
	void showTopoVel (const double vtopo );

	/**Fills the epoch box with the value corresponding to a julian day
	 * @param dt   date/time from which to construct the epoch string
	 */
	void showEpoch (const KStarsDateTime &dt );

	/* Method to process the lines from a file
	 */
	void processLines( QTextStream &istream );
  
	GeoLocation *geoPlace;

};



#endif
