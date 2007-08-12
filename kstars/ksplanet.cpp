/***************************************************************************
                          ksplanet.cpp  -  K Desktop Planetarium
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

#include "ksplanet.h"

#include <math.h>

#include <QFile>
#include <QTextStream>

#include <kdebug.h>

#include "ksnumbers.h"
#include "ksutils.h"
#include "ksfilereader.h"

KSPlanet::OrbitDataManager KSPlanet::odm;

KSPlanet::OrbitDataColl::OrbitDataColl() {
}

KSPlanet::OrbitDataManager::OrbitDataManager() {
//EMPTY
}

bool KSPlanet::OrbitDataManager::readOrbitData(const QString &fname,
		QVector<OrbitData> *vector) {
	QString line;
	QFile f;
	double A, B, C;

	if ( KSUtils::openDataFile( f, fname ) ) {
		KSFileReader fileReader( f ); // close file is included
		while ( fileReader.hasMoreLines() ) {
			line = fileReader.readLine();
			QTextStream instream( &line );
			instream >> A >> B >> C;
			vector->append( OrbitData(A, B, C) );
		}
	} else {
		return false;
	}
	return true;
}

bool KSPlanet::OrbitDataManager::loadData( KSPlanet::OrbitDataColl &odc, const QString &n ) {
	QString fname, snum, line;
	QFile f;
	int nCount = 0;
	QString nl = n.toLower();

	if ( hash.contains( nl ) ) {
		odc = hash[nl];
		return true;  //orbit data already loaded
	}

	//Create a new OrbitDataColl
	OrbitDataColl ret;

	//Ecliptic Longitude
	for (int i=0; i<6; ++i) {
		snum.setNum( i );
		fname = nl + ".L" + snum + ".vsop";
		if ( readOrbitData( fname, &ret.Lon[i] ) )
			nCount++;
	}

	if ( nCount==0 ) return false;

	//Ecliptic Latitude
	for (int i=0; i<6; ++i) {
		snum.setNum( i );
		fname = nl + ".B" + snum + ".vsop";
		if ( readOrbitData( fname, &ret.Lat[i] ) )
			nCount++;
	}

	if ( nCount==0 ) return false;

	//Heliocentric Distance
	for (int i=0; i<6; ++i) {
		snum.setNum( i );
		fname = nl + ".R" + snum + ".vsop";
		if ( readOrbitData( fname, &ret.Dst[i] ) )
			nCount++;
	}

	if ( nCount==0 ) return false;

	hash[nl] = ret;
	odc = hash[nl];

	return true;
}

KSPlanet::KSPlanet( KStarsData *kd, const QString &s, 
		const QString &imfile, const QColor & c, double pSize )
 : KSPlanetBase(kd, s, imfile, c, pSize ), data_loaded(false) {
}

//we don't need the reference to the ODC, so just give it a junk variable
bool KSPlanet::loadData() {
	OrbitDataColl odc;
	return odm.loadData( odc, name() );
}

void KSPlanet::calcEcliptic(double Tau, EclipticPosition &epret) const {
	double sum[6];
	OrbitDataColl odc;
	double Tpow[6];

	Tpow[0] = 1.0;
	for (int i=1; i<6; ++i) {
		Tpow[i] = Tpow[i-1] * Tau;
	}

	if ( ! odm.loadData( odc, name() ) ) {
		epret.longitude = 0.0;
		epret.latitude = 0.0;
		epret.radius = 0.0;
		kError() << "Could not get data for '" << name() << "'" << endl;
		return;
	}

	//Ecliptic Longitude
	for (int i=0; i<6; ++i) {
		sum[i] = 0.0;
		for (int j = 0; j < odc.Lon[i].size(); ++j) {
			sum[i] += odc.Lon[i][j].A * cos( odc.Lon[i][j].B + odc.Lon[i][j].C*Tau );
			/*
			kDebug() << "sum[" << i <<"] =" << sum[i] <<
				" A = " << odc.Lon[i][j].A << " B = " << odc.Lon[i][j].B <<
				" C = " << odc.Lon[i][j].C << endl;
				*/
	  	}
		sum[i] *= Tpow[i];
		//kDebug() << name() << " : sum[" << i << "] = " << sum[i];
  	}

	epret.longitude.setRadians( sum[0] + sum[1] + sum[2] + sum[3] + sum[4] + sum[5] );
	epret.longitude.setD( epret.longitude.reduce().Degrees() );

	//Compute Ecliptic Latitude
	for (uint i=0; i<6; ++i) {
		sum[i] = 0.0;
		for (int j = 0; j < odc.Lat[i].size(); ++j) {
			sum[i] += odc.Lat[i][j].A * cos( odc.Lat[i][j].B + odc.Lat[i][j].C*Tau );
	  	}
		sum[i] *= Tpow[i];
  	}


	epret.latitude.setRadians( sum[0] + sum[1] + sum[2] + sum[3] + sum[4] + sum[5] );

	//Compute Heliocentric Distance
	for (uint i=0; i<6; ++i) {
		sum[i] = 0.0;
		for (int j = 0; j < odc.Dst[i].size(); ++j) {
			sum[i] += odc.Dst[i][j].A * cos( odc.Dst[i][j].B + odc.Dst[i][j].C*Tau );
	  	}
		sum[i] *= Tpow[i];
  	}

	epret.radius = sum[0] + sum[1] + sum[2] + sum[3] + sum[4] + sum[5];

	/*
	kDebug() << name() << " pre: Lat = " << epret.latitude.toDMSString() << " Long = " <<
		epret.longitude.toDMSString() << " Dist = " << epret.radius << endl;
	*/

}

bool KSPlanet::findGeocentricPosition( const KSNumbers *num, const KSPlanetBase *Earth ) {

	if ( Earth != NULL ) {
		double sinL, sinL0, sinB, sinB0;
		double cosL, cosL0, cosB, cosB0;
		double x = 0.0, y = 0.0, z = 0.0;

		double olddst = -1000;
		double dst = 0;

		EclipticPosition trialpos;

		double jm = num->julianMillenia();

		Earth->ecLong()->SinCos( sinL0, cosL0 );
		Earth->ecLat()->SinCos( sinB0, cosB0 );

		double eX = Earth->rsun()*cosB0*cosL0;
		double eY = Earth->rsun()*cosB0*sinL0;
		double eZ = Earth->rsun()*sinB0;

		bool once=true;
		while (fabs(dst - olddst) > .001) {
			calcEcliptic(jm, trialpos);
			
			// We store the heliocentric ecliptic coordinates the first time they are computed.
			if(once){
				helEcPos = trialpos;
				once=false;
			}
			
			olddst = dst;

			trialpos.longitude.SinCos( sinL, cosL );
			trialpos.latitude.SinCos( sinB, cosB );

			x = trialpos.radius*cosB*cosL - eX;
			y = trialpos.radius*cosB*sinL - eY;
			z = trialpos.radius*sinB - eZ;

			//distance from Earth
			dst = sqrt(x*x + y*y + z*z);
			
			//The light-travel time delay, in millenia
			//0.0057755183 is the inverse speed of light, 
			//in days/AU
			double delay = (.0057755183 * dst) / 365250.0;

			jm = num->julianMillenia() - delay;

		}

		ep.longitude.setRadians( atan2( y, x ) );
		ep.longitude.reduce();
		ep.latitude.setRadians( atan2( z, sqrt( x*x + y*y ) ) );
		setRsun( trialpos.radius );
		setRearth( dst );

		EclipticToEquatorial( num->obliquity() );

		nutate(num);
		aberrate(num);

	} else {

		calcEcliptic(num->julianMillenia(), ep);
		helEcPos = ep;
	}

	//determine the position angle
	findPA( num );

	return true;
}
