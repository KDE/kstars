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

#include "kstars.h"
#include "ksutils.h"
#include "objectnamelist.h"
#include "detaildialog.h"
#include "locationdialog.h"
#include "timedialog.h"
#include "wutdialogui.h"

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
	KDialogBase (KDialogBase::Plain, i18n("What's up tonight"), Ok, Ok, (QWidget*)ks),
	kstars(ks), EveningFlag(0) {
	
	QFrame *page = plainPage();
	QVBoxLayout *vlay = new QVBoxLayout( page, 0, 0 );
	
	objectList = &(ks->data()->ObjNames);
	objectList->setLanguage( ks->options()->useLatinConstellNames );
	
	//initialize location and date to current KStars settings:
	geo = kstars->geo();
	JDToday = kstars->data()->CurrentDate;
	//Today is the Local Date/Time
	Today = KSUtils::JDtoUT( JDToday ).addSecs( int( 3600.*geo->TZ() ) );
	
	//If the Time is earlier than 6:00 am, assume the user wants the night of the previous date`
	if ( Today.time().hour() < 6 ) {
		JDToday -= 1.0;
		Today = Today.addDays( -1 );
	}
	
	JDTomorrow = JDToday + 1.0;
	
	WUT = new WUTDialogUI( page );
	vlay->addWidget( WUT );
	
	QString sGeo = geo->translatedName();
	if ( ! geo->translatedProvince().isEmpty() ) sGeo += ", " + geo->translatedProvince();
	sGeo += ", " + geo->translatedCountry();
	WUT->LocationLabel->setText( i18n( "at %1" ).arg( sGeo ) );
	WUT->DateLabel->setText( i18n( "The night of %1" ).arg( KGlobal::locale()->formatDate( Today.date(), true ) ) );
	
	initCategories();
	
	makeConnections();

	QTimer::singleShot(0, this, SLOT(init()));
}

WUTDialog::~WUTDialog(){
}

void WUTDialog::makeConnections() {
	connect( WUT->DateButton, SIGNAL( clicked() ), SLOT( slotChangeDate() ) );
	connect( WUT->LocationButton, SIGNAL( clicked() ), SLOT( slotChangeLocation() ) );
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
	SkyObject *o = (SkyObject*) kstars->data()->PC->planetSun();
	sunRiseTomorrow = o->riseSetTime( JDTomorrow, geo, true );
	sunSetToday = o->riseSetTime( JDToday, geo, false );
	sunRiseToday = o->riseSetTime( JDToday, geo, true );
	
	//check to see if Sun is circumpolar
	if ( o->checkCircumpolar( geo->lat() ) ) {
		if ( o->alt()->Degrees() > 0.0 ) {
			sRise = i18n( "circumpolar" );
			sSet = i18n( "circumpolar" );
			sDuration = "00:00";
		} else {
			sRise = i18n( "does not rise" );
			sSet = i18n( "does not rise" );
			sDuration = "24:00";
		}
	} else {
		sRise = sunRiseTomorrow.toString("hh:mm");
		sSet = sunSetToday.toString("hh:mm");
		
		float Dur = 24.0 + (float)sunRiseTomorrow.hour() + (float)sunRiseTomorrow.minute()/60.0 -
				(float)sunSetToday.hour() - (float)sunSetToday.minute()/60.0;
		int hDur = int(Dur);
		int mDur = int(60.0*(Dur - (float)hDur));
		sDuration = QString().sprintf( "%02d:%02d", hDur, mDur );
	}
	
	WUT->SunSetLabel->setText( i18n( "Sunset: %1" ).arg(sSet) );
	WUT->SunRiseLabel->setText( i18n( "Sunrise: %1" ).arg(sRise) );
	WUT->NightDurationLabel->setText( i18n( "Night duration: %1 hours" ).arg( sDuration ) );
	
	// moon almanac information
	o = (SkyObject*) kstars->data()->Moon;
	moonRise = o->riseSetTime( JDToday, geo, true );
	moonSet = o->riseSetTime( JDToday, geo, false );
	if ( moonSet < moonRise ) 
		moonSet = o->riseSetTime( JDTomorrow, geo, false );
	
	//check to see if Moon is circumpolar
	if ( o->checkCircumpolar( geo->lat() ) ) {
		if ( o->alt()->Degrees() > 0.0 ) {
			sRise = i18n( "circumpolar" );
			sSet = i18n( "circumpolar" );
		} else {
			sRise = i18n( "does not rise" );
			sSet = i18n( "does not rise" );
		}
	} else {
		sRise = moonRise.toString("hh:mm");
		sSet = moonSet.toString("hh:mm");
	}
	
	WUT->MoonRiseLabel->setText( i18n( "Moon rises at: %1" ).arg( sRise ) );
	WUT->MoonSetLabel->setText( i18n( "Moon sets at: %1" ).arg( sSet ) );
	WUT->MoonIllumLabel->setText( i18n( "Moon's Illumination fraction", "Moon illum.: %1%" ).arg( 
			int(100.0*kstars->data()->Moon->illum() ) ) );
	
	splitObjectList();
	// load first list
	slotLoadList(0);
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
			visible = checkVisibility(oname);
			// don't show the sun or moon
			if (i == 0) {
				if (oname->text() == "Sun" || oname->text() == "Moon" ) visible = false;
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
	double minAlt = 20.0; //minimum altitude for object to be considered 'visible'
	SkyPoint sp = (SkyPoint)*(oname->skyObject()); //local copy of skyObject's position
	
	//Initial values for T1, T2 assume all night option of EveningMorningBox
	QDateTime T1 = Today;
	T1.setTime( sunSetToday );
	QDateTime T2 = Today;
	T2 = T2.addDays( 1 );
	T2.setTime( sunRiseTomorrow );

	//Check Evening/Morning only state:
	if ( EveningFlag==0 ) { //Evening only
		T1.setTime( sunSetToday );
		T2.setTime( QTime( 0, 0, 0 ) ); //midnight
	} else if ( EveningFlag==1 ) { //Morning only
		T1 = T1.addDays( 1 );
		T1.setTime( QTime( 0, 0, 0 ) ); //midnight
		T2.setTime( sunRiseTomorrow );
	}
	
	for ( QDateTime test = T1; test < T2; test = test.addSecs(3600) ) {
		//Need LST of the test time, expressed as a dms object.
		QDateTime ut = test.addSecs( int( -3600*geo->TZ() ) );
		dms LST = KSUtils::UTtoLST( ut, geo->lng() );
		
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
		WUT->ObjectBox->setTitle( i18n( "No object selected" ) );
		
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
			tRise = o->riseSetTime( JDToday, geo, true );
			tSet = o->riseSetTime( JDToday, geo, false );
			if ( tSet < tRise ) 
				tSet = o->riseSetTime( JDTomorrow, geo, false );

			sRise = QString().sprintf( "%02d:%02d", tRise.hour(), tRise.minute() );
			sSet = QString().sprintf( "%02d:%02d", tSet.hour(), tSet.minute() );
		}
		
		tTransit = o->transitTime( JDToday, geo );
		if ( tTransit < tRise )
			tTransit = o->transitTime( JDTomorrow, geo );
		sTransit = QString().sprintf( "%02d:%02d", tTransit.hour(), tTransit.minute() );
	
		WUT->DetailButton->setEnabled( true );
	}
	
	WUT->ObjectRiseLabel->setText( i18n( "Rises at: %1" ).arg( sRise ) );
	WUT->ObjectTransitLabel->setText( i18n( "Transits at: %1" ).arg( sTransit ) );
	WUT->ObjectSetLabel->setText( i18n( "Sets at: %1" ).arg( sSet ) );
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
	TimeDialog td( Today, this );
	if ( td.exec() == QDialog::Accepted ) {
		Today = td.selectedDateTime();
		if ( Today.time().hour() < 6 ) Today = Today.addDays( -1 ); //assume user wants previous night.
		JDToday = KSUtils::UTtoJD( Today.addSecs( int( -3600.*geo->TZ() ) ) );
		JDTomorrow = JDToday + 1.0;
		WUT->DateLabel->setText( i18n( "The night of %1" ).arg( KGlobal::locale()->formatDate( Today.date() ) ) );
		
		int i = WUT->CategoryListBox->currentItem();
		init();
		slotLoadList( i );
	}
}

void WUTDialog::slotChangeLocation() {
	LocationDialog ld( kstars );
	if ( ld.exec() == QDialog::Accepted ) {
		if ( ld.getCityIndex() >= 0 ) {
			geo = kstars->data()->geoList.at( ld.getCityIndex() );
			QString sGeo = geo->translatedName();
			if ( ! geo->translatedProvince().isEmpty() ) sGeo += ", " + geo->translatedProvince();
			sGeo += ", " + geo->translatedCountry();
			WUT->LocationLabel->setText( i18n( "at %1" ).arg( sGeo ) );
			
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
