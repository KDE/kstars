/***************************************************************************
                          modcalcapcoord.h  -  description
                             -------------------
    begin                : Sun May 30 2004
    copyright            : (C) 2004 by Pablo de Vicente
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

#ifndef MODCALCANGDIST_H
#define MODCALCANGDIST_H

#include "modcalcangdistdlg.h"
#include <kapplication.h>

/** Module to compute the angular distance between two points in the sky 
  *@author Pablo de Vicente
  *@version 0.9
  */

class dms;
class dmsBox;
class SkyPoint;
class QTextStream;

class modCalcAngDist : public modCalcAngDistDlg  {

Q_OBJECT
public:
/**Constructor. */
	modCalcAngDist(QWidget *p, const char *n);
/**Destructor. */
	~modCalcAngDist();

public slots:
	void slotComputeDist();
	void slotClearCoords();
	void slotInputFile();
	void slotOutputFile();
	void slotRunBatch();

private:
	/** Process Lines **/
	void processLines( QTextStream &istream );

	/**@returns a SkyPoint constructed from the coordinates in the RA and Dec dmsBoxes. */
	SkyPoint getCoords(dmsBox * rBox, dmsBox* dBox);

	/**Fill the angular distance. */
	void showDist ( dms dist );

};

#endif
