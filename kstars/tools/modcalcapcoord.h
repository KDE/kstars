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

#include "modcalcapcoorddlg.h"
#include <kapplication.h>

/** Module to compute the equatorial coordinates for a given date and time
 * from a given epoch or equinox
  *@author Pablo de Vicente
	*@version 0.9
  */

class dms;
class dmsBox;
class SkyPoint;
class KStarsDateTime;
class QTextStream;

class modCalcApCoord : public modCalcApCoordDlg  {

Q_OBJECT
public:
/**Constructor. */
	modCalcApCoord(QWidget *p, const char *n);
/**Destructor. */
	~modCalcApCoord();

/**Precess the coordinates from epoch 1 to epoch 2 */
	SkyPoint precess (dms ra0, dms dec0, long double j0, long double jf);

/**Apply precession, nutation and aberration corrections to coordinates. */
	SkyPoint apparentCoordinates (dms r0, dms d0, long double j0, long double jf);

	/** Process Lines **/
//	void processLines( const QFile * f );
	void processLines( QTextStream &istream );
public slots:


	/** No descriptions */
	void slotComputeCoords();
	/** No descriptions */
	void slotClearCoords();
	void slotUtCheckedBatch();
	void slotDateCheckedBatch();
	void slotRaCheckedBatch();
	void slotDecCheckedBatch();
	void slotEpochCheckedBatch();
	void slotInputFile();
	void slotOutputFile();
	void slotRunBatch();

private:
/**@returns a SkyPoint constructed from the coordinates in the RA and Dec dmsBoxes. */
	SkyPoint getEquCoords(void);

/**Fill the Time and Date fields with the current values from the CPU clock. */
	void showCurrentTime(void);

/**@returns a KStarsDateTime constructed from the Time and Date fields. */
	KStarsDateTime getDateTime (void);

/**Parse the string argument as a double */
	double getEpoch (QString eName);

/**Fill the RA and Dec dmsBoxes with values of the SkyPoint argument. */
	void showEquCoords ( SkyPoint sp );

};

#endif
