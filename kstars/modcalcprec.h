/***************************************************************************
                          modcalcprec.h  -  description
                             -------------------
    begin                : Sun Jan 27 2002
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

#ifndef MODCALCPREC_H
#define MODCALCPREC_H

#include <qvbox.h>

/**
  * Class which implements the KStars calculator precession module. Precesses
  * coordinates between different epochs. Conversions are performed assuming
  * that coordinates are in the FK5 system. For example the conversion between
  * B1950 and J2000 is not exact. 
  *
  * Inherits QVBox
  *@author Pablo de Vicente
	*@version 0.9
  */

class QWidget;
class QVBox;
class QLineEdit;
class QString;
class dms;
class dmsBox;
class SkyPoint;

class modCalcPrec : public QVBox  {

Q_OBJECT
public: 
	modCalcPrec(QWidget *p, const char *n); 
	~modCalcPrec();

	double JdtoEpoch (long double jd);
	long double epochToJd (double epoch);
	SkyPoint precess (dms ra0, dms dec0, double e0, double ef);

public slots:
	void slotClearCoords (void);
	void slotComputeCoords (void);

private:
	SkyPoint getEquCoords(void);
	QString  showCurrentEpoch(void);
	double setCurrentEpoch(void);
	double getEpoch (QString eName);
	void showEquCoords ( SkyPoint sp );

	QVBox *rightBox;
	QLineEdit *rafName, *decfName, *ra0Name, *dec0Name, *epoch0Name, 
		*epochfName;
	dmsBox *ra0Box, *dec0Box, *rafBox, *decfBox;
	
};

#endif
