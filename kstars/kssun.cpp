/***************************************************************************
                          kssun.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Jul 22 2001
    copyright            : (C) 2001 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <klocale.h>
#include <kstars.h>
#include <qfile.h>
#include <qtextstream.h>
#include <math.h>
#include "kssun.h"
#include "ksutils.h"

KSSun::KSSun( KStars *ks, QString fn ) : KSPlanet( ks, I18N_NOOP( "Sun" ), fn ) {
	/*
	JD0 = 2447892.5; //Jan 1, 1990
	eclong0 = 279.403303; //mean ecliptic longitude at JD0
	plong0 = 282.768422; //longitude of sun at perigee for JD0
	e0 = 0.016713; //eccentricity of Earth's orbit at JD0
	*/
}

bool KSSun::loadData() {
//	kdDebug() << k_funcinfo << endl;
	return (odm.loadData("earth") != 0);
}

bool KSSun::findPosition( const KSNumbers *num, const KSPlanetBase *Earth ) {


	if (Earth) {
		//
		// For the precision we need, the earth's orbit is circular.
		// So don't bother to iterate like KSPlanet does. Just subtract
		// The current delay and recompute (once).
		// 
		double delay = (.0057755183 * Earth->rsun()) / 365250.0;
		
		//
		// MHH 2002-02-04 I don't like this. But it avoids code duplication.
		// Maybe we can find a better way.
		//
		const KSPlanet *pEarth = dynamic_cast<const KSPlanet *>(Earth);

		EclipticPosition trialpos;
		pEarth->calcEcliptic(num->julianMillenia() - delay, trialpos);

		/*
		kdDebug() << name() << " : ELat = " << pEarth->ecLat().toDMSString() << " ELong = " << 
			pEarth->ecLong().toDMSString() << " rsun = " << pEarth->rsun() << endl;
		kdDebug() << name() << " : Lat = " << trialpos.latitude.toDMSString() << " Long = " << 
			trialpos.longitude.toDMSString() << " rsun = " << trialpos.radius << 
			" delay = " << delay << endl;
			*/


		setEcLong( trialpos.longitude.Degrees() + 180.0 );
		setEcLong( ecLong().reduce().Degrees() );
		setEcLat( -1.0*trialpos.latitude.Degrees() );

	} else {
		double sum[6];
		dms EarthLong, EarthLat; //heliocentric coords of Earth
		OrbitDataColl * odc;
		double T = num->julianMillenia(); //Julian millenia since J2000
		double Tpow[6];

		Tpow[0] = 1.0;
		for (int i=1; i<6; ++i) {
			Tpow[i] = Tpow[i-1] * T;
		}
			//First, find heliocentric coordinates
	
		if (!(odc =  odm.loadData("earth"))) return false;

		//Ecliptic Longitude
		for (int i=0; i<6; ++i) {
			sum[i] = 0.0;
			for (uint j = 0; j < odc->Lon[i].size(); ++j) {
				sum[i] += odc->Lon[i][j]->A * cos( odc->Lon[i][j]->B + odc->Lon[i][j]->C*T );
			}
			sum[i] *= Tpow[i];
			//kdDebug() << name() << " : sum[" << i << "] = " << sum[i] <<endl;
		}

		EarthLong.setRadians( sum[0] + sum[1] + sum[2] + 
				sum[3] + sum[4] + sum[5] );
		EarthLong.setD( EarthLong.reduce().Degrees() );
	  	
		//Compute Ecliptic Latitude
		for (int i=0; i<6; ++i) {
			sum[i] = 0.0;
			for (uint j = 0; j < odc->Lat[i].size(); ++j) {
				sum[i] += odc->Lat[i][j]->A * cos( odc->Lat[i][j]->B + odc->Lat[i][j]->C*T );
			}
			sum[i] *= Tpow[i];
		}


		EarthLat.setRadians( sum[0] + sum[1] + sum[2] + sum[3] + 
				sum[4] + sum[5] );
  	
		//Compute Heliocentric Distance
		for (int i=0; i<6; ++i) {
			sum[i] = 0.0;
			for (uint j = 0; j < odc->Dst[i].size(); ++j) {
				sum[i] += odc->Dst[i][j]->A * cos( odc->Dst[i][j]->B + odc->Dst[i][j]->C*T );
			}
			sum[i] *= Tpow[i];
		}

		ep.radius = sum[0] + sum[1] + sum[2] + sum[3] + sum[4] + sum[5];

		setEcLong( EarthLong.Degrees() + 180.0 );
		setEcLong( ecLong().reduce().Degrees() );
		setEcLat( -1.0*EarthLat.Degrees() );

		/*
		kdDebug() << name() << " : " << ec.long.Degrees() << ", " << 
			ep.lat.Degrees() << " (e) " <<
			EarthLong.Degrees() << ", " << EarthLat.Degrees() << endl;
			*/

	}
		

	//Finally, convert Ecliptic coords to Ra, Dec.  Ecliptic latitude is zero, by definition
	EclipticToEquatorial( num->obliquity() );

	nutate(num);

	aberrate(num);

	return true;	
}
