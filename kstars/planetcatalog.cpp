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


#include "planetcatalog.h"
#include "kssun.h"
#include "kspluto.h"


PlanetCatalog::PlanetCatalog() : Earth(0), Sun(0) {
	planets.setAutoDelete(true);
}

PlanetCatalog::~PlanetCatalog() {
	//
	// do NOT delete Sun. It is also in the QList
	// and will be deleted automatically.
	//
	delete Earth;
}



bool PlanetCatalog::initialize() {
	KSPlanetBase *ksp;

	Earth = new KSPlanet( I18N_NOOP( "Earth" ) );
	if (!Earth->loadData())
		return false;

	Sun = new KSSun("sun.png");
	if (Sun->loadData())
		planets.append(Sun);

	ksp = new KSPluto("pluto.png");
	if (ksp->loadData())
		planets.append(ksp);

	ksp = new KSPlanet(I18N_NOOP( "Mercury" ), "mercury.png");
	if (ksp->loadData())
		planets.append(ksp);

	ksp = new KSPlanet(I18N_NOOP( "Venus" ), "venus.png");
	if (ksp->loadData())
		planets.append(ksp);

	ksp = new KSPlanet(I18N_NOOP( "Mars" ), "mars.png");
	if (ksp->loadData())
		planets.append(ksp);

	ksp = new KSPlanet(I18N_NOOP( "Jupiter" ), "jupiter.png");
	if (ksp->loadData())
		planets.append(ksp);

	ksp = new KSPlanet(I18N_NOOP( "Saturn" ), "saturn.png");
	if (ksp->loadData())
		planets.append(ksp);

	ksp = new KSPlanet(I18N_NOOP( "Uranus" ), "uranus.png");
	if (ksp->loadData())
		planets.append(ksp);

	ksp = new KSPlanet(I18N_NOOP( "Neptune" ), "neptune.png");
	if (ksp->loadData())
		planets.append(ksp);

	return true;
}

void PlanetCatalog::addObject( ObjectNameList &ObjNames ) const {
	QListIterator<KSPlanetBase> it(planets);

	for (KSPlanetBase *ksp = it.toFirst(); ksp != 0; ksp = ++it) {
		ObjNames.append( ksp );
	}
}

void PlanetCatalog::findPosition( const KSNumbers *num) {
	Earth->findPosition(num);
	for (KSPlanetBase * ksp = planets.first(); ksp != 0; ksp = planets.next()) {
		ksp->findPosition(num, Earth);
	}

}

void PlanetCatalog::EquatorialToHorizontal( const dms LST, const dms lat ) {
	for (KSPlanetBase * ksp = planets.first(); ksp != 0; ksp = planets.next()) {
		ksp->EquatorialToHorizontal(LST, lat);
	}
}

bool PlanetCatalog::isPlanet(SkyObject *so) const {
	if (so == Earth)
		return true;

	QListIterator<KSPlanetBase> it(planets);

	for (KSPlanetBase *ksp = it.toFirst(); ksp != 0; ksp = ++it) {
		if (so == ksp)
			return true;
	}

	return false;
}

KSPlanetBase *PlanetCatalog::findByName( const QString n) const {
	if (n == "Earth")
		return Earth;

	QListIterator<KSPlanetBase> it(planets);

	for (KSPlanetBase *ksp = it.toFirst(); ksp != 0; ksp = ++it) {
		if (ksp->name() == n)
			return ksp;
	}

	kdDebug() << k_funcinfo << "could not find planet named " << n << endl;

	return 0;
}

static double dist_squared(const SkyPoint *a, const SkyPoint *b) {

	double dRA = a->ra().Hours() - b->ra().Hours();
	double dDec = a->dec().Degrees() - b->dec().Degrees();
	double f = 15.0*cos( a->dec().radians() );

	return f*f*dRA*dRA + dDec*dDec;
}

SkyObject *PlanetCatalog::findClosest(const SkyPoint *p, double &r) const {
	QListIterator<KSPlanetBase> it(planets);
	SkyObject *found = 0;
	double trialr = 0.0;
	double rmin = 100000.0;

	for (KSPlanetBase *ksp = it.toFirst(); ksp != 0; ksp = ++it) {
		trialr = dist_squared(ksp, p);
		if (trialr < rmin) {
			rmin = trialr;
			found = ksp;
		}
	}

	trialr = dist_squared(Earth, p);
	if (trialr < rmin) {
		rmin = trialr;
		found = Earth;
	}

	r = rmin;
	return found;

}

#include "planetcatalog.moc"
