/***************************************************************************
                          kspluto.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Mon Sep 24 2001
    copyright            : (C) 2001 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <math.h>

#include <qfile.h>
#include <kdebug.h>

#include "kspluto.h"
#include "ksutils.h"
#include "ksnumbers.h"

int KSPluto::DATAARRAYSIZE = 106;
bool KSPluto::data_loaded = false;
double *KSPluto::freq = 0;
KSPluto::XYZData *KSPluto::xdata = 0;
KSPluto::XYZData *KSPluto::ydata = 0;
KSPluto::XYZData *KSPluto::zdata = 0;
int KSPluto::objects = 0;

KSPluto::KSPluto(KStarsData *kd, QString fn, double pSize ) : KSPlanetBase( kd, I18N_NOOP( "Pluto" ), fn, pSize ) {
	objects++;
}

KSPluto::~KSPluto() {
	objects--;
	// delete arrays if all other objects are closed
	if (!objects) {
		if (freq) {
			delete [] freq;
			freq = 0;
		}
		if (xdata) {
			delete [] xdata;
			xdata = 0;
		}
		if (ydata) {
			delete [] ydata;
			ydata = 0;
		}
		if (zdata) {
			delete [] zdata;
			zdata = 0;
		}
	}
}

bool KSPluto::loadData() {
	return loadData("pluto");
}

bool KSPluto::loadData(QString fn) {
	QString fname, snum, line;
	QFile f;

	if (data_loaded) return true;

//	kdDebug() << "inside pluto loadData" << endl;
	freq = new double[DATAARRAYSIZE];
	xdata = new XYZData[DATAARRAYSIZE];
	ydata = new XYZData[DATAARRAYSIZE];
	zdata = new XYZData[DATAARRAYSIZE];

	//read in the periodic frequencies
	int n = 0;
	if ( KSUtils::openDataFile( f, fn.lower() + ".freq" ) ) {
		QTextStream stream( &f );
		while ( !stream.eof() ) {
			line = stream.readLine();
			QTextIStream instream( &line );
			instream >> freq[n++];

			if (n > DATAARRAYSIZE) {
				kdDebug() << "We tried to read more than " << DATAARRAYSIZE <<
					"elements into KSPluto::freq" << endl;
				return false;
			}
		}
		f.close();
	}

	if (n==0) return false;

	n=0;
	if ( KSUtils::openDataFile( f, fn.lower() + ".x" ) ) {
		QTextStream stream( &f );
		while ( !stream.eof() ) {
			double ac,as;
			line = stream.readLine();
			QTextIStream instream( &line );
			instream >> ac >> as;
			xdata[n++] = XYZData(ac, as);

			if (n > DATAARRAYSIZE) {
				kdDebug() << "We tried to read more than " << DATAARRAYSIZE <<
					"elements into KSPluto::xdata" << endl;
				return false;
			}
		}
		f.close();
	}

	if (n==0) return false;

	n=0;
	if ( KSUtils::openDataFile( f, fn.lower() + ".y" ) ) {
		QTextStream stream( &f );
		while ( !stream.eof() ) {
			double ac,as;
			line = stream.readLine();
			QTextIStream instream( &line );
			instream >> ac >> as;
			ydata[n++] = XYZData(ac, as);

			if (n > DATAARRAYSIZE) {
				kdDebug() << "We tried to read more than " << DATAARRAYSIZE <<
					"elements into KSPluto::ydata" << endl;
				return false;
			}
		}
		f.close();
	}

	if (n==0) return false;

	n=0;
	if ( KSUtils::openDataFile( f, fn.lower() + ".z" ) ) {
		QTextStream stream( &f );
		while ( !stream.eof() ) {
			double ac,as;
			line = stream.readLine();
			QTextIStream instream( &line );
			instream >> ac >> as;
			zdata[n++] = XYZData(ac, as);

			if (n > DATAARRAYSIZE) {
				kdDebug() << "We tried to read more than " << DATAARRAYSIZE <<
					"elements into KSPluto::zdata" << endl;
				return false;
			}
		}
		f.close();
	}

	if (n==0) return false;

	data_loaded=true;
	return true;
}


KSPluto::XYZpos KSPluto::calcRectCoords(double jd)  {
	double X, Y, Z, U;

	loadData(name());

	double T = 2.0*(jd -  2341972.5)/146120.0 - 1.0;

	U =  T*73060.0;

	//compute secular terms
	X = 98083308510. - 1465718392.*T + 11528487809.*T*T + 55397965917.*T*T*T;
	Y = 101846243715. + 57789.*T - 5487929294.*T*T + 8520205290.*T*T*T;
	Z = 2183700004. + 433209785.*T - 4911803413.*T*T - 14029741184.*T*T*T;


// compute periodic X terms
	for(int n=0; n < DATAARRAYSIZE; ++n) {
		double tempdouble = xdata[n].ac*cos( freq[n]*U ) + xdata[n].as*sin( freq[n]*U );
		if (n > 81) tempdouble *= T;
		if (n > 100) tempdouble *= T;

		X += tempdouble;
	}

// compute periodic Y terms
	for(int n=0; n < DATAARRAYSIZE; ++n) {
		double tempdouble = ydata[n].ac*cos( freq[n]*U ) + ydata[n].as*sin( freq[n]*U );
		if (n > 81) tempdouble *= T;
		if (n > 100) tempdouble *= T;

		Y += tempdouble;
	}

// compute periodic Z terms
	for(int n=0; n < DATAARRAYSIZE; ++n) {
		double tempdouble = zdata[n].ac*cos( freq[n]*U ) + zdata[n].as*sin( freq[n]*U );
		if (n > 81) tempdouble *= T;
		if (n > 100) tempdouble *= T;

		Z += tempdouble;
	}

	return XYZpos(X,Y,Z);
}

bool KSPluto::findGeocentricPosition( const KSNumbers *num, const KSPlanetBase *Earth ) {
	double X0, Y0, Z0, RARad;
	dms L0, B0; //geocentric ecliptic coords of Sun
	dms EarthLong, EarthLat; //heliocentric ecliptic coords of Earth
	double sinL0, cosL0, sinB0, cosB0;

	XYZpos pos;

	double olddst = -1000;
	double dst = 0;

	double jd = num->julianDay();

	Earth->ecLong()->SinCos( sinL0, cosL0 );
	Earth->ecLat()->SinCos( sinB0, cosB0 );

	double eX = Earth->rsun()*cosB0*cosL0;
	double eY = Earth->rsun()*cosB0*sinL0;
	double eZ = Earth->rsun()*sinB0;

	while (fabs(dst-olddst) > .001) {
		pos = calcRectCoords(jd);
		//convert X, Y, Z to AU (given in 1.0E10 AU)
		pos.X /= 10000000000.0; pos.Y /= 10000000000.0; pos.Z /= 10000000000.0;

		/*
		kdDebug() << "pluto: X = " << pos.X << " Y = " << pos.Y << " Z = " << pos.Z << endl;
		*/

		double dX = pos.X - eX;
		double dY = pos.Y - eY;
		double dZ = pos.Z - eZ;

		olddst = dst;
		dst = sqrt( dX * dX + dY * dY + dZ * dZ );
		double delay = .0057755183 * dst;

		jd = num->julianDay() - delay;

	}

	//Compute the heliocentric ecliptic coords
	setRsun( sqrt( pos.X*pos.X + pos.Y*pos.Y + pos.Z*pos.Z ) );
	L0.setRadians( atan( pos.Y / pos.X ) );
	if ( pos.X < 0.0 ) L0.setD( L0.Degrees() + 180.0 );

//	if ( pos.X > 0.0 && pos.Y < 0.0 ) L += 360.0;
	setHelEcLong( L0 );
	
	B0.setRadians( asin( pos.Z / rsun() ) );
	setHelEcLat( B0 );

	//L0, B0 are Sun's Ecliptic coords (L0 = Learth + 180; B0 = -Bearth)
	L0.setD( Earth->ecLong()->Degrees() + 180.0 );
	L0.setD( L0.reduce().Degrees() );
	B0.setD( -1.0*Earth->ecLat()->Degrees() );

	L0.SinCos( sinL0, cosL0 );
	B0.SinCos( sinB0, cosB0 );

	double cosOb, sinOb;
	num->obliquity()->SinCos( sinOb, cosOb );

	X0 = Earth->rsun()*cosB0*cosL0;
	Y0 = Earth->rsun()*( cosB0*sinL0*cosOb - sinB0*sinOb );
	Z0 = Earth->rsun()*( cosB0*sinL0*sinOb + sinB0*cosOb );

	//transform to geocentric rectangular coordinates by adding Sun's values
	pos.X += X0; pos.Y += Y0; pos.Z += Z0;

	//Use Meeus's Eq. 32.10 to find Rsun, RA and Dec:
	RARad = atan( pos.Y / pos.X );
	if ( pos.X<0 ) RARad += dms::PI;
	if ( pos.X>0 && pos.Y<0 ) RARad += 2.0*dms::PI;
	dms newRA; newRA.setRadians( RARad );
	dms newDec; newDec.setRadians( asin( pos.Z/rsun() ) );
	setRA( newRA );
	setDec( newDec );

	//compute Ecliptic coordinates
	EquatorialToEcliptic( num->obliquity() );

	//determine the position angle
	findPA( num );

	setRearth( Earth );

	return true;
}
