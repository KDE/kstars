/***************************************************************************
                          modcalcgal.h  -  description
                             -------------------
    begin                : Thu Jan 17 2002
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

#ifndef MODCALCGALCOORD_H
#define MODCALCGALCOORD_H

#include "dms.h"
#include "modcalcgalcoorddlg.h"

/**
  * Class which implements the KStars calculator module to compute
  * Galactic coordinates to/from Equatorial coordinates.
  *
  * Inherits QWidget
  *@author Pablo de Vicente
	*@version 0.9
  */

class modCalcGalCoord : public modCalcGalCoordDlg  {

Q_OBJECT

public:
	
	modCalcGalCoord(QWidget *p, const char *n);
	~modCalcGalCoord();
	/**
	* Obtains the galactic coords. from the Box.
	*/
	void getGalCoords (void);
	void getEquCoords (void);
	void getEpoch (void);
	double getEpoch( QString t );
	void showEquCoords(void);
	void showGalCoords(void);
	void GalToEqu(void);
	void EquToGal(void);
	
public slots:

	void slotClearCoords (void);
	void slotComputeCoords (void);
	void slotGalLatCheckedBatch(void);
	void slotGalLongCheckedBatch(void);
	void slotRaCheckedBatch(void);
	void slotDecCheckedBatch(void);
	void slotEpochCheckedBatch(void);
	void slotInputFile(void);
	void slotOutputFile(void);
	void slotRunBatch();

private:
	void equCheck(void);
	void galCheck(void);
	void processLines( QTextStream &is );

	dms galLong, galLat, raCoord, decCoord;
	double epoch;
	bool galInputCoords;
};
#endif

