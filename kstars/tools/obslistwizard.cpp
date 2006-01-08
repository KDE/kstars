/***************************************************************************
                          obslistwizard.cpp  -  Display overhead view of the solar system
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

//#include <qcheckbox.h>
//#include <qlabel.h>
//#include <qlayout.h>
//#include <q3widgetstack.h>

#include <QVBoxLayout>
#include <QFrame>
#include <knuminput.h>
#include <klistbox.h>
#include <klistview.h>
#include <kpushbutton.h>

#include "kstars.h"
#include "kstarsdata.h"
#include "locationdialog.h"
#include "skyobject.h"
#include "deepskyobject.h"
#include "starobject.h"
#include "widgets/dmsbox.h"
#include "widgets/magnitudespinbox.h"
//#include "libkdeedu/extdate/extdatetimeedit.h"

#include "obslistwizard.h"

ObsListWizardUI::ObsListWizardUI( QWidget *p ) : QFrame ( p ) {
  setupUi( p );
}

ObsListWizard::ObsListWizard( QWidget *parent, const char *name ) 
  : KDialogBase( KDialogBase::Plain, i18n("Observing List Wizard"), Ok|Cancel, Ok, parent, name )
{
	ksw = (KStars*)parent;
	QFrame *page = plainPage();
	QVBoxLayout *vlay = new QVBoxLayout( page, 0, 0 );

	olw = new ObsListWizardUI( page );
	vlay->addWidget( olw );

	connect( olw->AllButton, SIGNAL( clicked() ), this, SLOT( slotAllButton() ) );
	connect( olw->NoneButton, SIGNAL( clicked() ), this, SLOT( slotNoneButton() ) );
	connect( olw->DeepSkyButton, SIGNAL( clicked() ), this, SLOT( slotDeepSkyButton() ) );
	connect( olw->SolarSystemButton, SIGNAL( clicked() ), this, SLOT( slotSolarSystemButton() ) );
	connect( olw->LocationButton, SIGNAL( clicked() ), this, SLOT( slotChangeLocation() ) );

	connect( olw->FilterList, SIGNAL( clicked(Q3ListViewItem*) ), this, SLOT( slotShowStackWidget(Q3ListViewItem*) ) );

	//Update the selected observing list when certain UI elements are modified
	connect( olw->TypeList, SIGNAL( selectionChanged() ), this, SLOT( slotUpdateObjectCount() ) );
	connect( olw->ConstellationList, SIGNAL( selectionChanged() ), this, SLOT( slotUpdateObjectCount() ) );
	connect( olw->RAMin, SIGNAL( lostFocus() ), this, SLOT( slotUpdateObjectCount() ) );
	connect( olw->RAMax, SIGNAL( lostFocus() ), this, SLOT( slotUpdateObjectCount() ) );
	connect( olw->DecMin, SIGNAL( lostFocus() ), this, SLOT( slotUpdateObjectCount() ) );
	connect( olw->DecMax, SIGNAL( lostFocus() ), this, SLOT( slotUpdateObjectCount() ) );
	connect( olw->RA, SIGNAL( lostFocus() ), this, SLOT( slotUpdateObjectCount() ) );
	connect( olw->Dec, SIGNAL( lostFocus() ), this, SLOT( slotUpdateObjectCount() ) );
	connect( olw->Radius, SIGNAL( valueChanged(double) ), this, SLOT( slotUpdateObjectCount() ) );
//	connect( olw->Date, SIGNAL( valueChanged( const ExtDate& ) ), this, SLOT( slotApplyFilters() ) );
	connect( olw->Mag, SIGNAL( valueChanged( double ) ), this, SLOT( slotUpdateObjectCount() ) );
	connect( olw->ExcludeNoMag, SIGNAL( clicked() ), this, SLOT( slotUpdateObjectCount() ) );

	connect( olw->SelectByConstellation, SIGNAL( toggled(bool) ), this, SLOT( slotEnableConstellationPage(bool) ) );
	connect( olw->SelectByRect, SIGNAL( toggled(bool) ), this, SLOT( slotEnableRectPage(bool) ) );
	connect( olw->SelectByCirc, SIGNAL( toggled(bool) ), this, SLOT( slotEnableCircPage(bool) ) );
//	connect( olw->SelectByDate, SIGNAL( toggled(bool) ), this, SLOT( slotEnableDatePage(bool) ) );
	connect( olw->SelectByMag,  SIGNAL( toggled(bool) ), this, SLOT( slotEnableMagPage(bool) ) );

	connect( this, SIGNAL( okClicked() ), this, SLOT( slotApplyFilters() ) );

	initialize();
}

ObsListWizard::~ObsListWizard()
{
}

void ObsListWizard::initialize()
{
	//Populate the list of constellations
	foreach ( SkyObject *p, ksw->data()->skyComposite()->constellationNames() ) {
		olw->ConstellationList->insertItem( p->name() );
	}

	//unSelect all object types
	olw->TypeList->selectAll( false );

	olw->Mag->setMinValue( -5.0 );
	olw->Mag->setMaxValue( 20.0 );
	olw->Mag->setValue( 6.0 );

	olw->FilterList->setSelected( olw->FilterList->firstChild(), true );

	olw->RA->setDegType( false );
	olw->RAMin->setDegType( false );
	olw->RAMax->setDegType( false );

	//Initialize object counts
	ObjectCount = 0; //number of objects in observing list
	StarCount = ksw->data()->skyComposite()->stars().size(); //total number of stars
	PlanetCount = 10; //Sun, Moon, 8 planets
	AsteroidCount = ksw->data()->skyComposite()->asteroids().size(); //total number of asteroids
	CometCount = ksw->data()->skyComposite()->comets().size(); //total number of comets
	//DeepSkyObjects
	OpenClusterCount = 0;
	GlobClusterCount = 0;
	GasNebCount = 0;
	PlanNebCount = 0;
	GalaxyCount = 0;
	foreach ( DeepSkyObject *o, ksw->data()->skyComposite()->deepSkyObjects() ) {
		if ( o->type() == SkyObject::GALAXY ) ++GalaxyCount; //most deepsky obj are galaxies, so check them first
		else if ( o->type() == SkyObject::STAR || o->type() == SkyObject::CATALOG_STAR ) ++StarCount;
		else if ( o->type() == SkyObject::OPEN_CLUSTER ) ++OpenClusterCount;
		else if ( o->type() == SkyObject::GLOBULAR_CLUSTER ) ++GlobClusterCount;
		else if ( o->type() == SkyObject::GASEOUS_NEBULA || o->type() == SkyObject::SUPERNOVA_REMNANT ) ++GasNebCount;
		else if ( o->type() == SkyObject::PLANETARY_NEBULA ) ++PlanNebCount;
	}

// 	//DEBUG
// 	kdDebug() << "StarCount: " << StarCount << endl;
// 	kdDebug() << "OpenClusterCount: " << OpenClusterCount << endl;
// 	kdDebug() << "GlobClusterCount: " << GlobClusterCount << endl;
// 	kdDebug() << "GasNebCount: " << GasNebCount << endl;
// 	kdDebug() << "PlanNebCount: " << PlanNebCount << endl;
// 	kdDebug() << "GalaxyCount: " << GalaxyCount << endl;

}

void ObsListWizard::slotAllButton() { olw->TypeList->selectAll( true ); }
void ObsListWizard::slotNoneButton() { olw->TypeList->selectAll( false ); }

void ObsListWizard::slotEnableConstellationPage( bool t ) {
	olw->ConstellationList->setEnabled(t);

	//disable the other two region options
	if ( t ) {
		olw->SelectByRect->setChecked( false );
		olw->SelectByCirc->setChecked( false );
	}
	
	slotUpdateObjectCount();
}

void ObsListWizard::slotEnableRectPage( bool t ) {
	olw->RAMin->setEnabled(t);
	olw->RAMax->setEnabled(t);
	olw->DecMin->setEnabled(t);
	olw->DecMax->setEnabled(t);

	//disable the other two region options
	if ( t ) {
		olw->SelectByConstellation->setChecked( false );
		olw->SelectByCirc->setChecked( false );
	}
	
	slotUpdateObjectCount();
}

void ObsListWizard::slotEnableCircPage( bool t ) {
	olw->RA->setEnabled(t);
	olw->Dec->setEnabled(t);
	olw->Radius->setEnabled(t);

	//disable the other two region options
	if ( t ) {
		olw->SelectByConstellation->setChecked( false );
		olw->SelectByRect->setChecked( false );
	}
	
	slotUpdateObjectCount();
}

// void ObsListWizard::slotEnableDatePage( bool t ) {
// 	olw->Date->setEnabled(t);
// 	olw->LocationButton->setEnabled(t);
// }

void ObsListWizard::slotEnableMagPage( bool t ) {
	olw->Mag->setEnabled(t);
	olw->ExcludeNoMag->setEnabled(t);
	slotUpdateObjectCount();
}

void ObsListWizard::slotShowStackWidget( Q3ListViewItem *i )
{
	if ( i ) {
		QString t = i->text(0);
	
		if ( t.contains( i18n( "Object type(s)" ) ) )      olw->FilterStack->setCurrentWidget( olw->ObjTypePage );
		if ( t.contains( i18n( "Region" ) ) )              olw->FilterStack->setCurrentWidget( olw->RegionPage );
		if ( t.contains( i18n( "In constellation(s)" ) ) ) olw->FilterStack->setCurrentWidget( olw->ConstellationPage );
		if ( t.contains( i18n( "Circular" ) ) )            olw->FilterStack->setCurrentWidget( olw->CircRegionPage );
		if ( t.contains( i18n( "Rectangular" ) ) )         olw->FilterStack->setCurrentWidget( olw->RectRegionPage );
//		if ( t.contains( i18n( "Observable on date" ) ) )  olw->FilterStack->setCurrentWidget( olw->ObsDatePage );
		if ( t.contains( i18n( "Magnitude limit" ) ) )     olw->FilterStack->setCurrentWidget( olw->MagLimitPage );
	}
}

void ObsListWizard::slotDeepSkyButton()
{
	olw->TypeList->selectAll( false );
	olw->TypeList->setSelected( olw->TypeList->findItem( i18n( "Open Clusters" ) ), true );
	olw->TypeList->setSelected( olw->TypeList->findItem( i18n( "Globular Clusters" ) ), true );
	olw->TypeList->setSelected( olw->TypeList->findItem( i18n( "Gaseous Nebulae" ) ), true );
	olw->TypeList->setSelected( olw->TypeList->findItem( i18n( "Planetary Nebulae" ) ), true );
	olw->TypeList->setSelected( olw->TypeList->findItem( i18n( "Galaxies" ) ), true );
}

void ObsListWizard::slotSolarSystemButton()
{
	olw->TypeList->selectAll( false );
	olw->TypeList->setSelected( olw->TypeList->findItem( i18n( "Sun, Moon, Planets" ) ), true );
	olw->TypeList->setSelected( olw->TypeList->findItem( i18n( "Comets" ) ), true );
	olw->TypeList->setSelected( olw->TypeList->findItem( i18n( "Asteroids" ) ), true );
}

void ObsListWizard::slotChangeLocation()
{
	LocationDialog ld( ksw );

	if ( ld.exec() == QDialog::Accepted ) {
		//set geographic location
	}
}

void ObsListWizard::slotUpdateObjectCount() 
{
	ObjectCount = 0;
	if ( olw->TypeList->findItem( i18n( "Stars" ) )->isSelected() )
		ObjectCount += StarCount;
	if ( olw->TypeList->findItem( i18n( "Sun, Moon, Planets" ) )->isSelected() ) 
		ObjectCount += PlanetCount;
	if ( olw->TypeList->findItem( i18n( "Comets" ) )->isSelected() )
		ObjectCount += CometCount;
	if ( olw->TypeList->findItem( i18n( "Asteroids" ) )->isSelected() )
		ObjectCount += AsteroidCount;
	if ( olw->TypeList->findItem( i18n( "Galaxies" ) )->isSelected() )
		ObjectCount += GalaxyCount;
	if ( olw->TypeList->findItem( i18n( "Open Clusters" ) )->isSelected() )
		ObjectCount += OpenClusterCount;
	if ( olw->TypeList->findItem( i18n( "Globular Clusters" ) )->isSelected() )
		ObjectCount += GlobClusterCount;
	if ( olw->TypeList->findItem( i18n( "Gaseous Nebulae" ) )->isSelected() )
		ObjectCount += GasNebCount;
	if ( olw->TypeList->findItem( i18n( "Planetary Nebulae" ) )->isSelected() )
		ObjectCount += PlanNebCount;

	applyFilters( false ); //false = only adjust counts, do not build list
}

void ObsListWizard::applyFilters( bool doBuildList )
{
	if ( doBuildList )
		obsList().clear();

	//make sure rect region data are valid
	rectOk = false;
	if ( olw->SelectByRect->isChecked() ) {
		ra1 = olw->RAMin->createDms( false, &rectOk ).Hours();
		if ( rectOk ) ra2 = olw->RAMax->createDms( false, &rectOk ).Hours();
		if ( rectOk ) dc1 = olw->DecMin->createDms( true, &rectOk ).Degrees();
		if ( rectOk ) dc2 = olw->DecMax->createDms( true, &rectOk ).Degrees();
		if ( ra2 == 0.0 ) ra2 = 24.0;

		//Make sure dc1 < dc2.  
		if ( dc1 > dc2 ) {
			double temp = dc2;
			dc2 = dc1;
			dc1 = temp;
		}

		//If ra1 > ra2, we may need to swap the two values, or subtract 24h from ra1.
		if ( ra1 > ra2 ) {
			if ( ra1 - ra2 > 12.0 ) { //the user probably wants a region that straddles 0h
				ra1 -= 24.0;
			} else { //the user probably wants ra2 to be the lower limit
				double temp = ra2;
				ra2 = ra1;
				ra1 = temp;
			}
		}
	}

	//make sure circ region data are valid
	circOk = false;
	if ( olw->SelectByCirc->isChecked() ) {
		double ra = olw->RA->createDms( false, &circOk ).Hours();
		double dc(0.0);
		if ( circOk ) dc = olw->Dec->createDms( true, &circOk ).Degrees();
		if ( circOk ) {
			pCirc.set( ra, dc );
			rCirc = olw->Radius->createDms().Degrees();
		}
	}

	double maglimit = 100.;
	if ( olw->SelectByMag->isChecked() ) maglimit = olw->Mag->value();

	//Stars
	QList<SkyObject*>& starList = ksw->data()->skyComposite()->stars();
	int starIndex( starList.size() );
	if ( olw->TypeList->findItem( i18n( "Stars" ) )->isSelected() ) {
		if ( maglimit < 100. ) {
			//Stars are sorted by mag, so use binary search algo to find index of faintest mag
			int low(0), high(starList.size()-1), middle(high);
			while ( low < high ) {
				middle = (low + high)/2;
				if ( maglimit == starList.at(middle)->mag() ) break;
				if ( maglimit <  starList.at(middle)->mag() ) high = middle - 1;
				if ( maglimit >  starList.at(middle)->mag() ) low = middle + 1;
			}
			//now, the star at "middle" has the right mag, but we want the *last* star that has this mag.
			for ( starIndex=middle+1; starIndex < starList.size(); ++starIndex ) {
				if ( starList.at(starIndex)->mag() > maglimit ) break;
			}
		}

		if ( doBuildList ) {
			for ( int i=0; i < starIndex; ++i ) {
				SkyObject *o = (SkyObject*)(starList[i]);
				applyRegionFilter( o, doBuildList, false ); //false = don't adjust ObjectCount 
			}
		} else {
			ObjectCount -= (starList.size() - starIndex); //reduce StarCount by appropriate amount
			for ( int i=0; i < starIndex; ++i ) {
				SkyObject *o = starList[i];
				applyRegionFilter( o, doBuildList );
			}
		}
	}

	//Sun, Moon, Planets
	if ( olw->TypeList->findItem( i18n( "Sun, Moon, Planets" ) )->isSelected() ) {
		applyRegionFilter( (SkyObject*)ksw->data()->skyComposite()->findByName("Sun"), doBuildList );
		applyRegionFilter( (SkyObject*)ksw->data()->skyComposite()->findByName("Moon"), doBuildList );
		applyRegionFilter( (SkyObject*)ksw->data()->skyComposite()->findByName("Mercury"), doBuildList );
		applyRegionFilter( (SkyObject*)ksw->data()->skyComposite()->findByName("Venus"), doBuildList );
		applyRegionFilter( (SkyObject*)ksw->data()->skyComposite()->findByName("Mars"), doBuildList );
		applyRegionFilter( (SkyObject*)ksw->data()->skyComposite()->findByName("Jupiter"), doBuildList );
		applyRegionFilter( (SkyObject*)ksw->data()->skyComposite()->findByName("Saturn"), doBuildList );
		applyRegionFilter( (SkyObject*)ksw->data()->skyComposite()->findByName("Uranus"), doBuildList );
		applyRegionFilter( (SkyObject*)ksw->data()->skyComposite()->findByName("Neptune"), doBuildList );
		applyRegionFilter( (SkyObject*)ksw->data()->skyComposite()->findByName("Pluto"), doBuildList );
	}

	//Deep sky objects
	foreach ( DeepSkyObject *o, ksw->data()->skyComposite()->deepSkyObjects() ) {
		//Skip unselected object types
		if ( (o->type() == SkyObject::STAR || o->type() == SkyObject::CATALOG_STAR) && ! olw->TypeList->findItem( i18n( "Stars" ) )->isSelected() ) continue;
		if ( o->type() == SkyObject::OPEN_CLUSTER && ! olw->TypeList->findItem( i18n( "Open Clusters" ) )->isSelected() ) continue;
		if ( o->type() == SkyObject::GLOBULAR_CLUSTER && ! olw->TypeList->findItem( i18n( "Globular Clusters" ) )->isSelected() ) continue;
		if ( (o->type() == SkyObject::GASEOUS_NEBULA || o->type() == SkyObject::SUPERNOVA_REMNANT) && ! olw->TypeList->findItem( i18n( "Gaseous Nebulae" ) )->isSelected() ) continue;
		if ( o->type() == SkyObject::PLANETARY_NEBULA && ! olw->TypeList->findItem( i18n( "Planetary Nebulae" ) )->isSelected() ) continue;
		if ( o->type() == SkyObject::GALAXY && ! olw->TypeList->findItem( i18n( "Galaxies" ) )->isSelected() ) continue;
		if ( o->type() == SkyObject::TYPE_UNKNOWN ) continue;

		if ( olw->SelectByMag->isChecked() ) {
			if ( o->mag() > 90. ) {
				if ( ! olw->ExcludeNoMag->isChecked() ) 
					applyRegionFilter( o, doBuildList );
				else if ( ! doBuildList )
					--ObjectCount;
			} else {
				if ( o->mag() <= maglimit ) 
					applyRegionFilter( o, doBuildList );
				else if ( ! doBuildList )
					--ObjectCount;
			}
		} else {
			applyRegionFilter( o, doBuildList );
		}
	}

	//Comets
	if ( olw->TypeList->findItem( i18n( "Comets" ) )->isSelected() ) {
		foreach ( SkyObject *o, ksw->data()->skyComposite()->comets() ) {
			//comets don't have magnitudes at this point, so skip mag check
			applyRegionFilter( o, doBuildList );
		}
	}

	//Asteroids
	if ( olw->TypeList->findItem( i18n( "Asteroids" ) )->isSelected() ) {
		foreach ( SkyObject *o, ksw->data()->skyComposite()->asteroids() ) {
			if ( olw->SelectByMag->isChecked() ) {
				if ( o->mag() > 90. ) {
					if ( ! olw->ExcludeNoMag->isChecked() ) 
						applyRegionFilter( o, doBuildList );
					else if ( ! doBuildList )
						--ObjectCount;
				} else {
					if ( o->mag() <= maglimit ) 
						applyRegionFilter( o, doBuildList );
					else if ( ! doBuildList )
						--ObjectCount;
				}
			} else {
				applyRegionFilter( o, doBuildList );
			}
		}
	}

	//Update the object count label
	if ( doBuildList ) ObjectCount = obsList().size();
	olw->CountLabel->setText( i18n("Current selection: %1 objects").arg( ObjectCount ) );
}
	
void ObsListWizard::applyRegionFilter( SkyObject *o, bool doBuildList, bool doAdjustCount ) {
	//select by constellation
	if ( olw->SelectByConstellation->isChecked() ) {
		QString c( ksw->data()->skyComposite()->constellation( o ) );
		Q3ListBoxItem *it = olw->ConstellationList->findItem( c );

		if ( it && it->isSelected() ) { 
			if ( doBuildList ) obsList().append ( o );
		} else if ( doAdjustCount ) --ObjectCount;
	}

	//select by rectangular region
	else if ( rectOk ) {
		double ra = o->ra()->Hours();
		double dec = o->dec()->Degrees();
		bool addObject = false;
		if ( dec >= dc1 && dec <= dc2 ) {
			if ( ra1 < 0.0 ) {
				if (ra >= ra1 + 24.0 || ra <= ra2 ) { addObject = true; }
			} else {
				if ( ra >= ra1 && ra <= ra2 ) { addObject = true; }
			}
		}

		if ( addObject && doBuildList ) obsList().append( o );
		if ( ! addObject && doAdjustCount ) --ObjectCount;
	}

	//select by circular region
	else if ( circOk ) {
		if ( o->angularDistanceTo( &pCirc ).Degrees() < rCirc ) { 
			if ( doBuildList ) obsList().append( o );
		} else if ( doAdjustCount ) --ObjectCount;
	}

	//No region filter, just add the object
	else if ( doBuildList ) {
		obsList().append( o );
	}
}

#include "obslistwizard.moc"
