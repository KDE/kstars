/***************************************************************************
                          planetcatalog.cpp  -  description
                             -------------------
    begin                : Mon Feb 18 2002
    copyright          : (C) 2002 by Mark Hollomon
    email                : mhh@mindspring.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <qstring.h>

#include "planetcatalog.h"
#include "dms.h"
#include "kstarsdata.h"
#include "ksnumbers.h"
#include "ksplanetbase.h"
#include "ksplanet.h"
#include "kssun.h"
#include "kspluto.h"
#include "skypoint.h"
#include "skyobject.h"
#include "objectnamelist.h"

PlanetCatalog::PlanetCatalog(KStarsData *dat) : Earth(0), Sun(0), kd(dat) {
}

PlanetCatalog::~PlanetCatalog() {
	while ( ! planets.isEmpty() ) delete planets.takeFirst();
	delete Earth;
}

bool PlanetCatalog::initialize() {
	KSPlanetBase *ksp;

	Earth = new KSPlanet( kd, I18N_NOOP( "Earth" ), "", 12756.28 /*diameter in km*/ );
	if (!Earth->loadData())
		return false;

	Sun = new KSSun( kd, "sun.png", 1392000. /*diameter in km*/ );
	if (Sun->loadData()) {
		planets.append(Sun);
	}

	ksp = new KSPluto( kd, "pluto.png", 2320. /*diameter in km*/ );
	planets.append(ksp);

	ksp = new KSPlanet( kd, I18N_NOOP( "Mercury" ), "mercury.png", 4879.4 /*diameter in km*/ );
	if (ksp->loadData()) {
		planets.append(ksp);
	}

	ksp = new KSPlanet( kd, I18N_NOOP( "Venus" ), "venus.png", 12103.6 /*diameter in km*/ );
	if (ksp->loadData()) {
		planets.append(ksp);
	}

	ksp = new KSPlanet( kd, I18N_NOOP( "Mars" ), "mars.png", 6792.4 /*diameter in km*/ );
	if (ksp->loadData()) {
		planets.append(ksp);
	}

	ksp = new KSPlanet( kd, I18N_NOOP( "Jupiter" ), "jupiter.png", 142984. /*diameter in km*/ );
	if (ksp->loadData()) {
		planets.append(ksp);
	}

	ksp = new KSPlanet( kd, I18N_NOOP( "Saturn" ), "saturn.png", 120536. /*diameter in km*/ );
	if (ksp->loadData()) {
		planets.append(ksp);
	}

	ksp = new KSPlanet( kd, I18N_NOOP( "Uranus" ), "uranus.png", 51118. /*diameter in km*/ );
	if (ksp->loadData()) {
		planets.append(ksp);
	}

	ksp = new KSPlanet( kd, I18N_NOOP( "Neptune" ), "neptune.png", 49572. /*diameter in km*/ );
	if (ksp->loadData()) {
		planets.append(ksp);
	}

	return true;
}

void PlanetCatalog::addObject( ObjectNameList &ObjNames ) const {
	for ( int i=0; i < planets.size(); ++i ) {
		ObjNames.append( planets[i] );
	}
}

void PlanetCatalog::findPosition( const KSNumbers *num, const dms *lat, const dms *LST ) {
	Earth->findPosition(num);
	for ( int i=0; i < planets.size(); ++i )
		planets[i]->findPosition(num, lat, LST, Earth);
}

void PlanetCatalog::EquatorialToHorizontal( dms *LST, const dms *lat ) {
	for ( int i=0; i < planets.size(); ++i ) {
		planets[i]->EquatorialToHorizontal( LST, lat);
		if ( planets[i]->hasTrail() ) planets[i]->updateTrail( LST, lat );
	}
}

bool PlanetCatalog::isPlanet(SkyObject *so) const {
	if (so == Earth)
		return true;

	for ( int i=0; i < planets.size(); ++i ) {
		if (so == planets[i])
			return true;
	}

	return false;
}

KSPlanetBase *PlanetCatalog::findByName( const QString n) const {
	if (n == "Earth")
		return Earth;

	QPtrListIterator<KSPlanetBase> it(planets);

	for ( int i=0; i < planets.size(); ++i ) {
		if (planets[i]->name() == n)
			return planets[i];
	}

	kdDebug() << k_funcinfo << "could not find planet named " << n << endl;

	return 0;
}

static double dist_squared(const SkyPoint *a, const SkyPoint *b) {
	double dRA = a->ra()->Hours() - b->ra()->Hours();
	double dDec = a->dec()->Degrees() - b->dec()->Degrees();
	double f = 15.0*cos( a->dec()->radians() );

	return f*f*dRA*dRA + dDec*dDec;
}

SkyObject *PlanetCatalog::findClosest(const SkyPoint *p, double &r) const {
	SkyObject *found = 0;
	double trialr = 0.0;
	double rmin = 100000.0;

	for ( int i=0; i < planets.size(); ++I ) {
		trialr = dist_squared( planets[i], p );
		if ( trialr < rmin ) {
			rmin = trialr;
			found = planets[i];
		}
	}

	r = rmin;
	return found;

}

#include "planetcatalog.moc"
