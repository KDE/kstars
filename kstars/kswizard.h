/***************************************************************************
                          kswizard.h  -  description
                             -------------------
    begin                : Wed 28 Jan 2004
    copyright            : (C) 2004 by Jason Harris
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

#ifndef KSWIZARD_H
#define KSWIZARD_H

#include <qmemarray.h>
#include "kswizardui.h"

class GeoLocation;

/**@class Setup Wizard for KStars
	*The Setup Wizard will be automatically opened when KStars runs 
	*for the first time.  It allows the user to set up some basic parameters:
	*@li Geographic Location
	*@li Download extra data files
	*@author Jason Harris
	*@version 1.0
	*/

class KStars;
class GeoLocation;

class KSWizard : public KSWizardUI
{
Q_OBJECT
public:
	KSWizard( QWidget *parent=0, const char *name=0 );
	~KSWizard();
	GeoLocation* geo() const { return Geo; }

private slots:
	void slotChangeCity();
	void slotFilterCities();
//Uncomment if we ever need the telescope page...
//	void slotTelescopeSetup();

private:
	void initGeoPage();
	
	KStars *ksw;
	QMemArray<int> GeoID;
	GeoLocation *Geo;
	QPtrList<GeoLocation> filteredCityList;
};

#endif
