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
#include <kmessagebox.h>
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

	map()->Update();
}

/** Major Dialog Window Actions **/

void KStars::slotCalculator() {
	AstroCalc astrocalc (this);
	astrocalc.exec();
}

void KStars::slotGeoLocator() {
	LocationDialog locationdialog (this);
	if ( locationdialog.exec() == QDialog::Accepted ) {
		if ( locationdialog.addCityEnabled() ) { //user closed the location dialog without adding their new city;
			locationdialog.addCity();                   //call addCity() for them!
		}

		int ii = locationdialog.getCityIndex();
		if ( ii >= 0 ) {
			geo()->reset( data()->geoList.at(ii) );
			options()->CityName = geo()->name();
			options()->ProvinceName = geo()->province();
			options()->CountryName = geo()->country();

			infoPanel->geoChanged(geo());

			// Adjust Local time for new time zone (but we aren't sure about DST yet)
			data()->LTime.setDate( data()->UTime.date() );
			data()->LTime.setTime( data()->UTime.time() );
			data()->LTime = data()->LTime.addSecs( int(geo()->TZ()*3600) );

//Set DST, if necessary
			geo()->tzrule()->setDST( geo()->tzrule()->isDSTActive( data()->LTime ) );

			//compute JD for the next DST adjustment
			QDateTime changetime = geo()->tzrule()->nextDSTChange( data()->LTime );
			data()->NextDSTChange = KSUtils::UTtoJulian( changetime.addSecs( int(-3600 * geo()->TZ())) );

			//compute JD for the previous DST adjustment
			changetime = geo()->tzrule()->previousDSTChange( data()->LTime );
			data()->PrevDSTChange = KSUtils::UTtoJulian( changetime.addSecs( int(-3600 * geo()->TZ())) );

			//Set local time again, this time using correct DST setting
			data()->LTime.setDate( data()->UTime.date() );
			data()->LTime.setTime( data()->UTime.time() );
			data()->LTime = data()->LTime.addSecs( int(geo()->TZ()*3600) );

			data()->LST = KSUtils::UTtoLST( data()->UTime, geo()->lng() );
			data()->LSTh.setH( data()->LST.hour(), data()->LST.minute(), data()->LST.second() );
			data()->HourAngle.setH( data()->LSTh.Hours() - map()->focus()->ra().Hours() );

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
		map()->Update();
	}
	else
		saveOptions();
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
		map()->setClickedObject( findDialog->currentItem()->objName()->skyObject() );
		map()->setClickedPoint( map()->clickedObject() );
		map()->slotCenter();
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
	int currSColorMode;
	QString currSky, currMess, currNGC, currIC, currHST, currSName, currPName;
	QString currCName, currCLine, currMW, currEq, currEcl, currHorz, currGrid;
	bool switchColors(false);

#if (KDE_VERSION <= 222)
	KPrinter printer( true );
#else
	KPrinter printer( true, QPrinter::PrinterResolution );
#endif

//Suggest Chart color scheme
	if ( options()->colorSky != "#FFFFFF" ) {
		QString message = i18n( "You can save printer ink by using the \"Star Chart\" color scheme, which uses a white background. Would you like to switch to the Star Chart color scheme for printing? (Your current color settings will be restored when printing is finished.)" );

//KDE3 version of the messagebox allows us to include a "Don't ask again" checkbox...
		int answer;
#if (KDE_VERSION <= 222)
		answer = KMessageBox::questionYesNo( 0, message, i18n( "Switch to Star Chart colors?" ) );
#else
		answer = KMessageBox::questionYesNo( 0, message, i18n( "Switch to Star Chart colors?" ),
			KStdGuiItem::yes(), KStdGuiItem::no(), "askAgainPrintColors" );
#endif

		if ( answer == KMessageBox::Yes ) {
			//First, store current colors
			//I should implement a ColorScheme class...
			switchColors = true;
			currSColorMode = options()->starColorMode;
			currSky = options()->colorSky;
			currMess = options()->colorMess;
			currNGC = options()->colorNGC;
			currIC = options()->colorIC;
			currHST = options()->colorHST;
			currSName = options()->colorSName;
			currPName = options()->colorPName;
			currCName = options()->colorCName;
			currCLine = options()->colorCLine;
			currMW = options()->colorMW;
			currEq = options()->colorEq;
			currEcl = options()->colorEcl;
			currHorz = options()->colorHorz;
			currGrid = options()->colorGrid;

			map()->setColors( "chart.colors" );
			map()->UpdateNow();
		}
	}

	printer.setFullPage( false );
	if( printer.setup( this ) ) {
		kapp->setOverrideCursor( waitCursor );

		QPainter *p = new QPainter( &printer );
		QPaintDeviceMetrics pdm( p->device() );

		QImage img( map()->skyPixmap().convertToImage() );
	
	//	if ( img.width() > pdm.width() || img.height() > pdm.height() )
	//		img = img.smoothScale( pdm.width(), pdm.height(), QImage::ScaleMin );

		//Fit map image to page if it's larger than the page.
#if (KDE_VERSION <= 222)
		int wfactor = img.width()/pdm.width();
		int hfactor = img.height()/pdm.height();

		if ( wfactor > 1.0 || hfactor > 1.0 ) { //need to scale down the image
			if ( wfactor >= hfactor )
				img = img.smoothScale( int( img.width()/wfactor ), int( img.height()/wfactor ) );
			else
				img = img.smoothScale( int( img.width()/hfactor ), int( img.height()/hfactor ) );
		}

#else
		if ( img.width() > pdm.width() || img.height() > pdm.height() )
			img = img.smoothScale( pdm.width(), pdm.height(), QImage::ScaleMin );
#endif //KDE_VERSION

		//Make sure image is centered on the page
		QPoint pt( (pdm.width() - img.width())/2, (pdm.height() - img.height())/2 );

		//Draw Image
		p->drawImage( pt, img );

		delete p;

		if ( switchColors ) {
			options()->starColorMode = currSColorMode;
			options()->colorSky = currSky;
			options()->colorMess = currMess;
			options()->colorNGC = currNGC;
			options()->colorIC = currIC;
			options()->colorHST = currHST;
			options()->colorSName = currSName;
			options()->colorPName = currPName;
			options()->colorCName = currCName;
			options()->colorCLine = currCLine;
			options()->colorMW = currMW;
			options()->colorEq = currEq;
			options()->colorEcl = currEcl;
			options()->colorHorz = currHorz;
			options()->colorGrid = currGrid;

			map()->UpdateNow();
		}

		kapp->restoreOverrideCursor();
	}
}

//Set Time to CPU clock
void KStars::slotSetTimeToNow() {
  clock->setUTC( QDateTime::currentDateTime().addSecs( int( -3600 * geo()->TZ() ) ) );
}

void KStars::slotToggleTimer() {
	if ( clock->isActive() ) {
		clock->stop();
		updateTime();
	} else {
		if ( fabs( clock->scale() ) > options()->slewTimeScale )
			clock->setManualMode( true );
		clock->start();
		if ( clock->isManualMode() ) map()->Update();
	}
}

//Focus
void KStars::slotPointFocus() {
	QString sentFrom( sender()->name() );

	if ( sentFrom == "zenith" )
		map()->invokeKey( KAccel::stringToKey( "Z" ) );
	else if ( sentFrom == "north" )
		map()->invokeKey( KAccel::stringToKey( "N" ) );
	else if ( sentFrom == "east" )
		map()->invokeKey( KAccel::stringToKey( "E" ) );
	else if ( sentFrom == "south" )
		map()->invokeKey( KAccel::stringToKey( "S" ) );
	else if ( sentFrom == "west" )
		map()->invokeKey( KAccel::stringToKey( "W" ) );
}

void KStars::slotTrack() {
	if ( options()->isTracking ) {
		options()->isTracking = false;
		actionCollection()->action("track_object")->setIconSet( BarIcon( "decrypted" ) );
		map()->setClickedObject( NULL );
		map()->setFoundObject( NULL );//no longer tracking foundObject
	} else {
		map()->setClickedPoint( map()->focus() );
		options()->isTracking = true;
		actionCollection()->action("track_object")->setIconSet( BarIcon( "encrypted" ) );
	}
}

void KStars::slotManualFocus() {
	FocusDialog focusDialog( this ); // = new FocusDialog( this );
	
	if ( focusDialog.exec() == QDialog::Accepted ) {
		map()->setClickedPoint( focusDialog.point() );
		map()->setClickedObject( NULL );
		map()->setFoundObject( NULL ); //make sure no longer tracking
		options()->isTracking = false;
		actionCollection()->action("track_object")->setIconSet( BarIcon( "decrypted" ) );

		map()->slotCenter();
	}
}

//View
void KStars::slotZoomIn() {
	actionCollection()->action("zoom_out")->setEnabled (true);
	if ( data()->ZoomLevel < MAXZOOMLEVEL ) {
		++data()->ZoomLevel;
		map()->Update();
	}
	if ( data()->ZoomLevel == MAXZOOMLEVEL )
		actionCollection()->action("zoom_in")->setEnabled (false);
}

void KStars::slotZoomOut() {
	actionCollection()->action("zoom_in")->setEnabled (true);
	if ( data()->ZoomLevel > MINZOOMLEVEL ) {
		--data()->ZoomLevel;
		map()->Update();
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
	map()->Update();
}

void KStars::slotColorScheme() {
	QString filename = QString( sender()->name() ).mid(3) + ".colors";
	map()->setColors( filename );
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
