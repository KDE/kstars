/***************************************************************************
                          wutdialog.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Die Feb 25 2003
    copyright            : (C) 2003 by Thomas Kabelmann
    email                : tk78@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "wutdialog.h"
#include "wutdialogui.h"

#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "ksnumbers.h"
#include "skyobjectname.h"
#include "objectnamelist.h"
#include "simclock.h"
#include "detaildialog.h"
#include "locationdialog.h"
#include "timedialog.h"
#include "kssun.h"
#include "ksmoon.h"

#include <kcombobox.h>
#include <klocale.h>
#include <klistbox.h>
#include <kpushbutton.h>

#include <qgroupbox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qframe.h>
#include <qtabbar.h>
#include <qtimer.h>
#include <qcursor.h>

WUTDialog::WUTDialog(KStars *ks) :
	KDialogBase (KDialogBase::Plain, i18n("What's up Tonight"), Close, Close, (QWidget*)ks),
	kstars(ks), EveningFlag(0) {

	QFrame *page = plainPage();
	setMainWidget(page);
	QVBoxLayout *vlay = new QVBoxLayout( page, 0, spacingHint() );
	WUT = new WUTDialogUI( page );
	vlay->addWidget( WUT );

	objectList = &(ks->data()->ObjNames);
//	objectList->setLanguage( Options::useLatinConstellNames() );

	//initialize location and date to current KStars settings:
	geo = kstars->geo();
	
	T0 = kstars->data()->lt();
	//If the Time is earlier than 6:00 am, assume the user wants the night of the previous date
	if ( T0.time().hour() < 6 ) 
		T0 = T0.addDays( -1 );

	//Now, set time T0 to midnight (of the following day)
	T0.setTime( QTime( 0, 0, 0 ) );
	T0 = T0.addDays( 1 );
	UT0 = geo->LTtoUT( T0 );
	
	//Set the Tomorrow date/time to Noon the following day
	Tomorrow = T0.addSecs( 12*3600 );
	TomorrowUT = geo->LTtoUT( Tomorrow );
	
	//Set the Evening date/time to 6:00 pm 
	Evening = T0.addSecs( -6*3600 );
	EveningUT = geo->LTtoUT( Evening );
	
	QString sGeo = geo->translatedName();
	if ( ! geo->translatedProvince().isEmpty() ) sGeo += ", " + geo->translatedProvince();
	sGeo += ", " + geo->translatedCountry();
	WUT->LocationLabel->setText( i18n( "at %1" ).arg( sGeo ) );
	WUT->DateLabel->setText( i18n( "The night of %1" ).arg( Evening.date().toString( Qt::LocalDate ) ) );

	initCategories();

	makeConnections();

	QTimer::singleShot(0, this, SLOT(init()));
}

WUTDialog::~WUTDialog(){
}

void WUTDialog::makeConnections() {
	connect( WUT->DateButton, SIGNAL( clicked() ), SLOT( slotChangeDate() ) );
	connect( WUT->LocationButton, SIGNAL( clicked() ), SLOT( slotChangeLocation() ) );
	connect( WUT->CenterButton, SIGNAL( clicked() ), SLOT( slotCenter() ) );
	connect( WUT->DetailButton, SIGNAL( clicked() ), SLOT( slotDetails() ) );
	connect( WUT->CategoryListBox, SIGNAL( highlighted(int) ), SLOT( slotLoadList(int) ) );
	connect( WUT->ObjectListBox, SIGNAL( selectionChanged(QListBoxItem*) ),
			SLOT( slotDisplayObject(QListBoxItem*) ) );
	connect( WUT->EveningMorningBox, SIGNAL( activated(int) ), SLOT( slotEveningMorning(int) ) );
}

void WUTDialog::initCategories() {
	WUT->CategoryListBox->insertItem( i18n( "Planets" ) );
	WUT->CategoryListBox->insertItem( i18n( "Comets" ) );
	WUT->CategoryListBox->insertItem( i18n( "Asteroids" ) );
	WUT->CategoryListBox->insertItem( i18n( "Stars" ) );
	WUT->CategoryListBox->insertItem( i18n( "Constellations" ) );
	WUT->CategoryListBox->insertItem( i18n( "Star Clusters" ) );
	WUT->CategoryListBox->insertItem( i18n( "Nebulae" ) );
	WUT->CategoryListBox->insertItem( i18n( "Galaxies" ) );
	WUT->CategoryListBox->setSelected(0, true);
}

void WUTDialog::init() {
	QString sRise, sSet, sDuration;

	// reset all lists
	for (int i=0; i<NCATEGORY; i++) {
		lists.visibleList[i].clear();
		lists.initialized[i] = false;
	}

	// sun almanac information
	KSSun *oSun = (KSSun*) kstars->data()->PCat->planetSun();
	sunRiseTomorrow = oSun->riseSetTime( TomorrowUT, geo, true );
	sunSetToday = oSun->riseSetTime( EveningUT, geo, false );
	sunRiseToday = oSun->riseSetTime( EveningUT, geo, true );

	//check to see if Sun is circumpolar
	KSNumbers *num = new KSNumbers( UT0.djd() );
	KSNumbers *oldNum = new KSNumbers( kstars->data()->ut().djd() );
	dms LST = geo->GSTtoLST( T0.gst() );
	
	oSun->updateCoords( num, true, geo->lat(), &LST );
	if ( oSun->checkCircumpolar( geo->lat() ) ) {
		if ( oSun->alt()->Degrees() > 0.0 ) {
			sRise = i18n( "circumpolar" );
			sSet = i18n( "circumpolar" );
			sDuration = "00:00";
		} else {
			sRise = i18n( "does not rise" );
			sSet = i18n( "does not rise" );
			sDuration = "24:00";
		}
	} else {
		//Round times to the nearest minute by adding 30 seconds to the time
		sRise = sunRiseTomorrow.addSecs(30).toString("hh:mm");
		sSet = sunSetToday.addSecs(30).toString("hh:mm");

		float Dur = 24.0 + (float)sunRiseTomorrow.hour() 
				+ (float)sunRiseTomorrow.minute()/60.0 
				+ (float)sunRiseTomorrow.second()/3600.0 
				- (float)sunSetToday.hour() 
				- (float)sunSetToday.minute()/60.0
				- (float)sunSetToday.second()/3600.0;
		int hDur = int(Dur);
		int mDur = int(60.0*(Dur - (float)hDur));
		sDuration = QString().sprintf( "%02d:%02d", hDur, mDur );
	}

	WUT->SunSetLabel->setText( i18n( "Sunset: %1" ).arg(sSet) );
	WUT->SunRiseLabel->setText( i18n( "Sunrise: %1" ).arg(sRise) );
	WUT->NightDurationLabel->setText( i18n( "Night duration: %1 hours" ).arg( sDuration ) );

	// moon almanac information
	KSMoon *oMoon = (KSMoon*) kstars->data()->Moon;
	moonRise = oMoon->riseSetTime( UT0, geo, true );
	moonSet = oMoon->riseSetTime( UT0, geo, false );

	//check to see if Moon is circumpolar
	oMoon->updateCoords( num, true, geo->lat(), &LST );
	if ( oMoon->checkCircumpolar( geo->lat() ) ) {
		if ( oMoon->alt()->Degrees() > 0.0 ) {
			sRise = i18n( "circumpolar" );
			sSet = i18n( "circumpolar" );
		} else {
			sRise = i18n( "does not rise" );
			sSet = i18n( "does not rise" );
		}
	} else {
		//Round times to the nearest minute by adding 30 seconds to the time
		sRise = moonRise.addSecs(30).toString("hh:mm");
		sSet = moonSet.addSecs(30).toString("hh:mm");
	}

	WUT->MoonRiseLabel->setText( i18n( "Moon rises at: %1" ).arg( sRise ) );
	WUT->MoonSetLabel->setText( i18n( "Moon sets at: %1" ).arg( sSet ) );
	oMoon->findPhase( oSun ); 
	WUT->MoonIllumLabel->setText( oMoon->phaseName() + QString( " (%1%)" ).arg(
			int(100.0*oMoon->illum() ) ) );

	//Restore Sun's and Moon's coordinates, and recompute Moon's original Phase
	oMoon->updateCoords( oldNum, true, geo->lat(), kstars->LST() );
	oSun->updateCoords( oldNum, true, geo->lat(), kstars->LST() );
	oMoon->findPhase( oSun ); 
	
	splitObjectList();
	// load first list
	slotLoadList(0);

	delete num;
	delete oldNum;
}

void WUTDialog::splitObjectList() {
	// don't append objects which are never visible
	for (SkyObjectName *oname=objectList->first(); oname; oname=objectList->next()) {
		bool visible = true;
		SkyObject *o = oname->skyObject();
		// is object circumpolar or never visible
		if (o->checkCircumpolar(geo->lat()) == true) {
			// never visible
			if (o->alt()->Degrees() <= 0) visible = false;
		}
		if (visible == true) appendToList(oname);
	}
}

void WUTDialog::appendToList(SkyObjectName *o) {
	// split into several lists
	switch (o->skyObject()->type()) {
		//case SkyObject::CATALOG_STAR			: //Omitting CATALOG_STARs from list
		case SkyObject::PLANET            : lists.visibleList[0].append(o); break;
		case SkyObject::COMET             : lists.visibleList[1].append(o); break;
		case SkyObject::ASTEROID          : lists.visibleList[2].append(o); break;
		case SkyObject::STAR              : lists.visibleList[3].append(o); break;
		case SkyObject::CONSTELLATION     : lists.visibleList[4].append(o); break;
		case SkyObject::OPEN_CLUSTER      :
		case SkyObject::GLOBULAR_CLUSTER  : lists.visibleList[5].append(o); break;
		case SkyObject::GASEOUS_NEBULA    :
		case SkyObject::PLANETARY_NEBULA  :
		case SkyObject::SUPERNOVA_REMNANT : lists.visibleList[6].append(o); break;
		case SkyObject::GALAXY            : lists.visibleList[7].append(o); break;
	}
}

void WUTDialog::slotLoadList(int i) {
	WUT->ObjectListBox->clear();
	setCursor(QCursor(Qt::WaitCursor));
	QPtrList <SkyObjectName> invisibleList;
	for (SkyObjectName *oname=lists.visibleList[i].first(); oname; oname=lists.visibleList[i].next()) {
		bool visible = true;
		if (lists.initialized[i] == false) {
			if (i == 0) {  //planets, sun and moon
				if (oname->skyObject()->name() == "Sun" ) visible = false;  // don't ever display the sun
				else visible = checkVisibility(oname);
			}
			if (visible == false) {
				// collect all invisible objects
				invisibleList.append(oname);
			}
		}
		// append to listbox
		if (visible == true) new SkyObjectNameListItem(WUT->ObjectListBox, oname);
	}
	// remove all invisible objects from list
	if (invisibleList.isEmpty() == false) {
		for (SkyObjectName *o=invisibleList.first(); o; o=invisibleList.next()) {
			lists.visibleList[i].removeRef(o);
		}
	}
	setCursor(QCursor(Qt::ArrowCursor));
	lists.initialized[i] = true;

	// highlight first item
	if ( WUT->ObjectListBox->count() ) {
		WUT->ObjectListBox->setSelected(0, true);
		WUT->ObjectListBox->setFocus();
	}
}

bool WUTDialog::checkVisibility(SkyObjectName *oname) {
	bool visible( false );
	double minAlt = 6.0; //An object is considered 'visible' if it is above horizon during civil twilight.

	//Initial values for T1, T2 assume all night option of EveningMorningBox
	KStarsDateTime T1 = Evening;
	T1.setTime( sunSetToday );
	KStarsDateTime T2 = Tomorrow;
	T2.setTime( sunRiseTomorrow );

	//Check Evening/Morning only state:
	if ( EveningFlag==0 ) { //Evening only
		T2 = T0; //midnight
	} else if ( EveningFlag==1 ) { //Morning only
		T1 = T0; //midnight
	}

	for ( KStarsDateTime test = T1; test < T2; test = test.addSecs(3600) ) {
		//Need LST of the test time, expressed as a dms object.
		KStarsDateTime ut = geo->LTtoUT( test );
		dms LST = geo->GSTtoLST( ut.gst() );
		SkyPoint sp = oname->skyObject()->recomputeCoords( ut, geo );
		
		//check altitude of object at this time.
		sp.EquatorialToHorizontal( &LST, geo->lat() );

		if ( sp.alt()->Degrees() > minAlt ) {
			visible = true;
			break;
		}
	}

	return visible;
}

void WUTDialog::slotDisplayObject(QListBoxItem *item) {
	QTime tRise, tSet, tTransit;
	QString sRise, sTransit, sSet;

	if ( item==0 ) { //no object selected
		WUT->ObjectBox->setTitle( i18n( "No Object Selected" ) );

		sRise = "--:--";
		sTransit = "--:--";
		sSet = "--:--";
		WUT->DetailButton->setEnabled( false );
	} else {
		SkyObject *o = ((SkyObjectNameListItem*)item)->objName()->skyObject();
		WUT->ObjectBox->setTitle( o->name() );

		if ( o->checkCircumpolar( geo->lat() ) ) {
			if ( o->alt()->Degrees() > 0.0 ) {
				sRise = i18n( "circumpolar" );
				sSet = i18n( "circumpolar" );
			} else {
				sRise = i18n( "does not rise" );
				sSet = i18n( "does not rise" );
			}
		} else {
			tRise = o->riseSetTime( T0, geo, true );
			tSet = o->riseSetTime( T0, geo, false );
//			if ( tSet < tRise ) 
//				tSet = o->riseSetTime( JDTomorrow, geo, false );
			
			sRise = QString().sprintf( "%02d:%02d", tRise.hour(), tRise.minute() );
			sSet = QString().sprintf( "%02d:%02d", tSet.hour(), tSet.minute() );
		}

		tTransit = o->transitTime( T0, geo );
//		if ( tTransit < tRise ) 
//			tTransit = o->transitTime( JDTomorrow, geo );
		
		sTransit = QString().sprintf( "%02d:%02d", tTransit.hour(), tTransit.minute() );

		WUT->DetailButton->setEnabled( true );
	}

	WUT->ObjectRiseLabel->setText( i18n( "Rises at: %1" ).arg( sRise ) );
	WUT->ObjectTransitLabel->setText( i18n( "Transits at: %1" ).arg( sTransit ) );
	WUT->ObjectSetLabel->setText( i18n( "Sets at: %1" ).arg( sSet ) );
}

void WUTDialog::slotCenter() {
	SkyObject *o = 0;
	// get selected item
	if (WUT->ObjectListBox->selectedItem() != 0) {
		o = ((SkyObjectNameListItem*)WUT->ObjectListBox->selectedItem())->objName()->skyObject();
	}
	if (o != 0) {
		kstars->map()->setFocusPoint( o );
		kstars->map()->setFocusObject( o );
		kstars->map()->setDestination( kstars->map()->focusPoint() );
	}
}

void WUTDialog::slotDetails() {
	SkyObject *o = 0;
	// get selected item
	if (WUT->ObjectListBox->selectedItem() != 0) {
		o = ((SkyObjectNameListItem*)WUT->ObjectListBox->selectedItem())->objName()->skyObject();
	}
	if (o != 0) {
		DetailDialog detail(o, kstars->data()->LTime, geo, kstars);
		detail.exec();
	}
}

void WUTDialog::slotChangeDate() {
	TimeDialog td( T0, this );
	if ( td.exec() == QDialog::Accepted ) {
		T0 = td.selectedDateTime();
		//If the Time is earlier than 6:00 am, assume the user wants the night of the previous date
		if ( T0.time().hour() < 6 ) 
			T0 = T0.addDays( -1 );
		
		//Now, set time T0 to midnight (of the following day)
		T0.setTime( QTime( 0, 0, 0 ) );
		T0 = T0.addDays( 1 );
		UT0 = geo->LTtoUT( T0 );
		
		//Set the Tomorrow date/time to Noon the following day
		Tomorrow = T0.addSecs( 12*3600 );
		TomorrowUT = geo->LTtoUT( Tomorrow );
		
		//Set the Evening date/time to 6:00 pm 
		Evening = T0.addSecs( -6*3600 );
		EveningUT = geo->LTtoUT( Evening );
		
		WUT->DateLabel->setText( i18n( "The night of %1" ).arg( Evening.date().toString() ) );

		int i = WUT->CategoryListBox->currentItem();
		init();
		slotLoadList( i );
	}
}

void WUTDialog::slotChangeLocation() {
	LocationDialog ld( kstars );
	if ( ld.exec() == QDialog::Accepted ) {
		GeoLocation *newGeo = ld.selectedCity();
		if ( newGeo ) {
			geo = newGeo;
			UT0 = geo->LTtoUT( T0 );
			TomorrowUT = geo->LTtoUT( Tomorrow );
			EveningUT = geo->LTtoUT( Evening );
			
			WUT->LocationLabel->setText( i18n( "at %1" ).arg( geo->fullName() ) );
			
			int i = WUT->CategoryListBox->currentItem();
			init();
			slotLoadList( i );
		}
	}
}

void WUTDialog::slotEveningMorning( int index ) {
	if ( EveningFlag != index ) {
		EveningFlag = index;
//		splitObjectList();
//		slotLoadList( WUT->CategoryListBox->currentItem() );
		int i = WUT->CategoryListBox->currentItem();
		init();
		slotLoadList( i );
	}
}

#include "wutdialog.moc"
