/***************************************************************************
                          obslistwizard.h  -  Display overhead view of the solar system
                             -------------------
    begin                : Thu 23 Jun 2005
    copyright            : (C) 2005 by Jason Harris
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

#ifndef OBSLISTWIZARD_H
#define OBSLISTWIZARD_H

#include <kdialogbase.h>

#include "obslistwizardui.h"

class KStars;
class QListViewItem;

/**@class ObsListWizard
 *@short Wizard for constructing observing lists
 */

class ObsListWizard : public KDialogBase
{
	Q_OBJECT
	public:
	/**@short Constructor
		*/
		ObsListWizard( QWidget *parent = 0, const char *name = 0 );
	/**@short Destructor
		*/
		~ObsListWizard();

	/**@return reference to QPtrList of objects selected by the wizard
		*/
		QPtrList<SkyObject>& obsList() { return ObsList; }

	private slots:
		void slotAllButton();
		void slotNoneButton();
		void slotDeepSkyButton();
		void slotSolarSystemButton();
		void slotChangeLocation();
		void slotShowStackWidget(QListViewItem*);
		void slotEnableConstellationPage(bool);
		void slotEnableRectPage(bool);
		void slotEnableCircPage(bool);
//		void slotEnableDatePage(bool);
		void slotEnableMagPage(bool);

	/**@short Construct the observing list by applying the selected filters
		*/
		void slotUpdateObjectCount();
		void slotApplyFilters() { applyFilters( true ); }

	private:
		void initialize();
		void applyFilters( bool doBuildList );
		void applyRegionFilter( SkyObject *o, bool doBuildList, bool doAdjustCount=true );

		QPtrList<SkyObject> ObsList;
		KStars *ksw;
		ObsListWizardUI *olw;
		uint ObjectCount, StarCount, PlanetCount, CometCount, AsteroidCount;
		uint GalaxyCount, OpenClusterCount, GlobClusterCount, GasNebCount, PlanNebCount;
		bool rectOk, circOk;
		double ra1, ra2, dc1, dc2, rCirc;
		SkyPoint pCirc;
};

#endif
