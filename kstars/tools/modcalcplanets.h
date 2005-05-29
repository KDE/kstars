/***************************************************************************
                          modcalcazel.h  -  description
                             -------------------
    begin                : mier abr 20 2004
    copyright            : (C) 2004-2005 by Pablo de Vicente
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

#ifndef MODCALCPLANETS_H
#define MODCALCPLANETS_H

#include <kapplication.h>
#include <qwidget.h>
#include "modcalcplanetsdlg.h"
#include "geolocation.h"
#include "kstarsdatetime.h"

class QWidget;
class QTextStream;
class KSPlanet;
class KSMoon;
class KSSun;
class KSPluto;

/**
  *@author Pablo de Vicente
  */

class modCalcPlanets : public modCalcPlanetsDlg  {

Q_OBJECT

public: 
	modCalcPlanets(QWidget *p, const char *n);
	~modCalcPlanets();
	
public slots:

	void slotComputePosition (void);
	void slotClear(void);
	void slotUtCheckedBatch();
	void slotDateCheckedBatch();
	void slotLongCheckedBatch();
	void slotLatCheckedBatch();
	void slotPlanetsCheckedBatch();
	void slotInputFile();
	void slotOutputFile();
	void slotRunBatch();
	void processLines( QTextStream &istream );
	//void slotInputFile();
	//void slotOutputFile();
	//void slotRunBatch();

private:

	void showCurrentDateTime (void);
	KStarsDateTime getDateTime (void);
	void showLongLat(void);
	GeoLocation getObserverPosition (void);
	void showCoordinates( const KSPlanet &ksp );
	void showCoordinates( const KSMoon &ksp );
	void showCoordinates( const KSSun &ksp );
	void showCoordinates( const KSPluto &ksp );
	void showHeliocentricEclipticCoords( const dms *hLong, const dms *hLat, double dist);
	void showGeocentricEclipticCoords( const dms *eLong, const dms *eLat, double r);
	void showEquatorialCoords( const dms *ra, const dms *dec);
	void showTopocentricCoords( const dms *az, const dms *el);
	unsigned int requiredBatchFields(void);

	// void processLines( QTextStream &istream );
  
};

#endif
