/***************************************************************************
                          kstarsactions.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Mon Feb 25 2002
    copyright            : (C) 2002 by Jason Harris
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
#include <kaccel.h>
#include <kiconloader.h>
#include <kprinter.h>
#include <ktip.h>
#include <qpaintdevicemetrics.h>

#include "kstars.h"
#include "timedialog.h"
#include "locationdialog.h"
#include "finddialog.h"
#include "focusdialog.h"
#include "viewopsdialog.h"
#include "astrocalc.h"
#include "infopanel.h"
#include "ksutils.h"

//This file contains function definitions for Actions declared in kstars.h

/** ViewToolBar Action.  All of the viewToolBar buttons are connected to this slot. **/

void KStars::slotViewToolBar() {
	if ( sender()->name() == QString( "show_stars" ) ) {
		options()->drawSAO = !options()->drawSAO;
	} else if ( sender()->name() == QString( "show_deepsky" ) ) {
		options()->drawDeepSky = !options()->drawDeepSky;
	} else if ( sender()->name() == QString( "show_planets" ) ) {
		options()->drawPlanets = !options()->drawPlanets;
	} else if ( sender()->name() == QString( "show_clines" ) ) {
		options()->drawConstellLines = !options()->drawConstellLines;
	} else if ( sender()->name() == QString( "show_cnames" ) ) {
		options()->drawConstellNames = !options()->drawConstellNames;
	} else if ( sender()->name() == QString( "show_mw" ) ) {
		options()->drawMilkyWay = !options()->drawMilkyWay;
	} else if ( sender()->name() == QString( "show_grid" ) ) {
		options()->drawGrid = !options()->drawGrid;
	} else if ( sender()->name() == QString( "show_horizon" ) ) {
		options()->drawGround = !options()->drawGround;
	}

	skymap->Update();
}

/** Major Dialog Window Actions **/

void KStars::slotCalculator() {
	AstroCalc astrocalc (this);
	astrocalc.exec();
}

void KStars::slotGeoLocator() {
	LocationDialog locationdialog (this);
	if ( locationdialog.exec() == QDialog::Accepted ) {
		if ( !locationdialog.selectedCityName().isEmpty() ) { //user closed the location dialog without adding their new city;
			locationdialog.addCity();                   //call addCity() for them!
		}

		int ii = locationdialog.getCityIndex();
		if ( ii >= 0 ) {
			geo()->reset( data()->geoList.at(ii) );
			options()->CityName = geo()->name();
			options()->ProvinceName = geo()->province();
			options()->CountryName = geo()->country();

			infoPanel->geoChanged(geo());

			// Adjust Local time for new time zone
			data()->LTime.setDate( data()->UTime.date() );
			data()->LTime.setTime( data()->UTime.time() );
			data()->LTime = data()->LTime.addSecs( int(geo()->TZ()*3600) );

			data()->LST = KSUtils::UTtoLST( data()->UTime, geo()->lng() );
			data()->LSTh.setH( data()->LST.hour(), data()->LST.minute(), data()->LST.second() );
			data()->HourAngle.setH( data()->LSTh.Hours() - skymap->focus()->ra().Hours() );

      //need to recompute Alt/Az coordinates of all objects, so
			//adjust LastSkyUpdate to ensure computation.  Then
			//explicitly call updateTime()
			data()->LastSkyUpdate -= 1.0; // a full day, should be plenty
			updateTime();
		}
	}
}

void KStars::slotViewOps() {
	// save options for cancel
	data()->saveOptions();

	ViewOpsDialog viewopsdialog (this);
	// connect caching funktions
	QObject::connect( &viewopsdialog, SIGNAL( clearCache() ), this, SLOT( clearCachedFindDialog() ) );

	// ask for the new options
	if ( viewopsdialog.exec() != QDialog::Accepted ) {
		// cancelled
		data()->restoreOptions();
		skymap->Update();
	}
}

void KStars::slotSetTime() {
	TimeDialog timedialog ( data()->LTime, this );

	if ( timedialog.exec() == QDialog::Accepted ) {
		QTime newTime( timedialog.selectedTime() );
		QDate newDate( timedialog.selectedDate() );

		changeTime(newDate, newTime);
	}
}

void KStars::slotFind() {
	if ( !findDialog ) {	  // create new dialog if no dialog is existing
		findDialog = new FindDialog( this );
	}

	if ( !findDialog ) kdWarning() << i18n( "KStars::slotFind() - Not enough memory for dialog" ) << endl;
	
	if ( findDialog->exec() == QDialog::Accepted && findDialog->currentItem() ) {
		skymap->setClickedObject( findDialog->currentItem()->objName()->skyObject() );
		skymap->setClickedPoint( skymap->clickedObject() );
		skymap->slotCenter();
	}
	// check if data has changed while dialog was open
	if ( DialogIsObsolete ) clearCachedFindDialog();
}

/** Menu Slots **/

//File
void KStars::newWindow() {
	new KStars(true);
}

void KStars::closeWindow() {
	close();
}

void KStars::slotPrint() {
//Doesn't work under KDE2:
//		KPrinter printer( true, QPrinter::PrinterResolution );
		KPrinter printer( true );

	printer.setFullPage( true );
	if( printer.setup( this ) ) {
		kapp->setOverrideCursor( waitCursor );

		QPaintDeviceMetrics pdm( &printer );
		QPainter *p = new QPainter;

		p->setWindow( 0, 0, skymap->width(), skymap->height() );
		p->setViewport( 0, 0, pdm.width(), pdm.height() );
		p->begin( &printer );
		p->drawPixmap( 0, 0, skymap->skyPixmap() );
		p->setPen( QColor("red") );
		p->drawRect( p->window() );
		p->end();
		delete p;
		kapp->restoreOverrideCursor();
	}
}

//Time
void KStars::slotSetTimeToNow() {
  clock->setUTC( QDateTime::currentDateTime().addSecs( int( -3600 * geo()->TZ() ) ) );
}

void KStars::slotToggleTimer() {
	if ( clock->isActive() ) {
		clock->stop();
	} else {
		clock->start();
	}
}

//Focus
void KStars::slotPointFocus() {
	QString sentFrom( sender()->name() );

	if ( sentFrom == "zenith" )
		skymap->invokeKey( KAccel::stringToKey( "Z" ) );
	else if ( sentFrom == "north" )
		skymap->invokeKey( KAccel::stringToKey( "N" ) );
	else if ( sentFrom == "east" )
		skymap->invokeKey( KAccel::stringToKey( "E" ) );
	else if ( sentFrom == "south" )
		skymap->invokeKey( KAccel::stringToKey( "S" ) );
	else if ( sentFrom == "west" )
		skymap->invokeKey( KAccel::stringToKey( "W" ) );
}

void KStars::slotTrack() {
	if ( options()->isTracking ) {
		options()->isTracking = false;
		actionCollection()->action("track_object")->setIconSet( BarIcon( "unlock" ) );
		skymap->setClickedObject( NULL );
		skymap->setFoundObject( NULL );//no longer tracking foundObject
	} else {
		skymap->setClickedPoint( skymap->focus() );
		options()->isTracking = true;
		actionCollection()->action("track_object")->setIconSet( BarIcon( "lock" ) );
	}
}

void KStars::slotManualFocus() {
	if ( !focusDialog ) {	  // create new dialog if no dialog is existing
		focusDialog = new FocusDialog( this );
	}

	if ( !focusDialog ) kdWarning() << i18n( "KStars::slotFocus() - error creating dialog" ) << endl;
	
	if ( focusDialog->exec() == QDialog::Accepted ) {
		skymap->setClickedPoint( focusDialog->point() );
		skymap->slotCenter();
	}
}

//View
void KStars::slotZoomIn() {
	actionCollection()->action("zoom_out")->setEnabled (true);
	if ( data()->ZoomLevel < MAXZOOMLEVEL ) {
		++data()->ZoomLevel;
		skymap->Update();
	}
	if ( data()->ZoomLevel == MAXZOOMLEVEL )
		actionCollection()->action("zoom_in")->setEnabled (false);
}

void KStars::slotZoomOut() {
	actionCollection()->action("zoom_in")->setEnabled (true);
	if ( data()->ZoomLevel > MINZOOMLEVEL ) {
		--data()->ZoomLevel;
		skymap->Update();
	}
	if ( data()->ZoomLevel == MINZOOMLEVEL )
		actionCollection()->action("zoom_out")->setEnabled (false);
}

void KStars::slotCoordSys() {
	if ( options()->useAltAz ) {
		options()->useAltAz = false;
		actCoordSys->turnOn();
	} else {
		options()->useAltAz = true;
		actCoordSys->turnOff();
	}
	skymap->Update();
}

void KStars::slotColorScheme() {
	QString filename = QString( sender()->name() ).mid(3) + ".colors";
	skymap->setColors( filename );
}

//Help
void KStars::slotTipOfDay() {
	KTipDialog::showTip("kstars/tips", true);
}

//toggle display of GUI Items on/off
void KStars::slotShowGUIItem( bool show ) {
//Toolbars
	if ( sender()->name() == QString( "show_mainToolBar" ) ) {
		options()->showMainToolBar = show;
		if ( show ) toolBar( "mainToolBar" )->show();
		else toolBar( "mainToolBar" )->hide();
	}
	if ( sender()->name() == QString( "show_viewToolBar" ) ) {
		options()->showViewToolBar = show;
		if ( show ) toolBar( "viewToolBar" )->show();
		else toolBar( "viewToolBar" )->hide();
	}

//InfoPanel: we only change options here; these are also connected to slots in
//InfoPanel that actually toggle the display.
	if ( sender()->name() == QString( "show_panel" ) )
		options()->showInfoPanel = show;
	if ( sender()->name() == QString( "show_time_panel" ) )
		options()->showIPTime = show;
	if ( sender()->name() == QString( "show_location_panel" ) )
		options()->showIPGeo = show;
	if ( sender()->name() == QString( "show_focus_panel" ) )
		options()->showIPFocus = show;
}
