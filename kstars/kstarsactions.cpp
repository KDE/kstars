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
#include <kfiledialog.h>
#include <kprocess.h>
#include <qpaintdevicemetrics.h>
#include <qradiobutton.h>
#include <qcheckbox.h>

#include "kstars.h"
#include "timedialog.h"
#include "locationdialog.h"
#include "finddialog.h"
#include "focusdialog.h"
#include "viewopsdialog.h"
#include "astrocalc.h"
#include "lcgenerator.h"
#include "infoboxes.h"
#include "ksutils.h"
#include "elts.h"
#include "wutdialog.h"
#include "indimenu.h"
#include "devmanager.h"
#include "indidriver.h"
#include "scriptbuilder.h"
#include "indiconf.h"
#include "planetviewer.h"

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

	// update time for all objects because they might be not initialized
	// it's needed when using horizontal coordinates
	data()->setFullTimeUpdate();
	updateTime();

	map()->forceUpdate();
}

/** Major Dialog Window Actions **/

void KStars::slotCalculator() {
	AstroCalc astrocalc (this);
	astrocalc.exec();
}

void KStars::slotLCGenerator() {
	if (AAVSODialog == NULL)
		AAVSODialog = new LCGenerator(this);

	AAVSODialog->show();
}

void KStars::slotElTs() {
	elts * eltsDialog = new elts(this);
	eltsDialog->show();
}

void KStars::slotWUT() {
	WUTDialog dialog(this);
	dialog.exec();
}

void KStars::slotScriptBuilder() {
	ScriptBuilder sb(this);
	sb.exec();
}

void KStars::slotSolarSystem() {
	PlanetViewer pv(this);
	pv.exec();
}

void KStars::slotINDIPanel() {

   if (indimenu == NULL)
     indimenu = new INDIMenu(this);

   indimenu->updateStatus();
}

void KStars::slotINDIDriver() {
	if (indidriver == NULL)
		indidriver = new INDIDriver(this);
	indidriver->show();
}

void KStars::slotINDIConf() {

   INDIConf indiconf(this);

   indiconf.timeCheck->setChecked( options()->indiAutoTime );
   indiconf.longCheck->setChecked( options()->indiAutoLong );
   indiconf.latCheck->setChecked ( options()->indiAutoLat  );
   indiconf.crosshairCheck->setChecked( options()->indiCrosshairs);
   indiconf.messagesCheck->setChecked ( options()->indiMessages);

   if (indiconf.exec() == QDialog::Accepted)
   {
     options()->indiAutoTime = indiconf.timeCheck->isChecked();
     options()->indiAutoLong = indiconf.longCheck->isChecked();
     options()->indiAutoLat  = indiconf.latCheck->isChecked();
     options()->indiCrosshairs = indiconf.crosshairCheck->isChecked();
     options()->indiMessages = indiconf.messagesCheck->isChecked();

     map()->forceUpdateNow();
   }
}

void KStars::slotGeoLocator() {
	LocationDialog locationdialog (this);
	if ( locationdialog.exec() == QDialog::Accepted ) {
		int ii = locationdialog.getCityIndex();
		if ( ii >= 0 ) {
			// set new location in options
			GeoLocation *newLocation = data()->geoList.at(ii);
			options()->setLocation( *newLocation );

			// reset infoboxes
			infoBoxes()->geoChanged( newLocation );

			// call changeTime to reset DST change times
			// However, adjust local time to keep UT the same.
			// create new LT without DST offset
			QDateTime ltime = data()->UTime.addSecs( int( 3600 * newLocation->TZ0()) );

			// reset timezonerule to compute next dst change
			newLocation->tzrule()->reset_with_ltime( ltime, newLocation->TZ0(), data()->isTimeRunningForward() );

			// reset next dst change time
			data()->setNextDSTChange( KSUtils::UTtoJD( newLocation->tzrule()->nextDSTChange() ) );

			// reset local sideral time
			setLST( clock()->UTC() );
			
			// Make sure Numbers, Moon, planets, and sky objects are updated immediately
			data()->setFullTimeUpdate();

			// If the sky is in Horizontal mode and not tracking, reset focus such that
			// Alt/Az remain constant.
			if ( ! options()->isTracking && options()->useAltAz ) {
				map()->focus()->HorizontalToEquatorial( LST(), geo()->lat() );
			}

			// recalculate new times and objects
			options()->setSnapNextFocus();
			updateTime();
		}
	}
}

void KStars::slotViewOps() {
	// save options for cancel
	data()->backupOptions();

	ViewOpsDialog viewopsdialog (this);
	// connect caching funktions
	QObject::connect( &viewopsdialog, SIGNAL( clearCache() ), this, SLOT( clearCachedFindDialog() ) );

	// ask for the new options
	if ( viewopsdialog.exec() != QDialog::Accepted ) {
		// cancelled
		data()->restoreOptions();
		map()->forceUpdate();
	}
	else
		data()->saveOptions(this);
}

void KStars::slotSetTime() {
	TimeDialog timedialog ( data()->LTime, this );

	if ( timedialog.exec() == QDialog::Accepted ) {
		QTime newTime( timedialog.selectedTime() );
		QDate newDate( timedialog.selectedDate() );

		changeTime(newDate, newTime);

		if ( options()->useAltAz ) {
			map()->focus()->HorizontalToEquatorial( LST(), geo()->lat() );
		}
	}
}

void KStars::slotFind() {
	clearCachedFindDialog();
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
	// since QT 3.1 close() just emits lastWindowClosed if the window is not hidden
	show();
	close();
}

void KStars::slotRunScript() {
	KURL fileURL = KFileDialog::getOpenURL( QDir::homeDirPath(), "*.kstars|KStars Scripts (*.kstars)" );

	if ( fileURL.isValid() && fileURL.isLocalFile() ) {
		//Remove the leading 'file:', which confuses QFile :(
		QString fname = fileURL.url().mid(5);
		QFile f( fname );

		if ( !f.open( IO_ReadOnly) ) {
			QString message = i18n( "Could not open file %1" ).arg( f.name() );
			KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
			return;
		}

		// Before we run the script, make sure that it's safe.  Each line must either begin with "#"
		// or begin with "dcop $KSTARS".  Otherwise, the line must be equal to one of the following:
		// "KSTARS=`dcopfind -a 'kstars*'`";  "MAIN=KStarsInterface";  or "CLOCK=clock#1"
		QTextStream istream(&f);
		QString line;
		bool fileOK( true );

		while (  ! istream.eof() ) {
			line = istream.readLine();
			if ( line.left(1) != "#" && line.left(12) != "dcop $KSTARS"
					&& line.stripWhiteSpace() != "KSTARS=`dcopfind -a 'kstars*'`"
					&& line.stripWhiteSpace() != "MAIN=KStarsInterface"
					&& line.stripWhiteSpace() != "CLOCK=clock#1" ) {
				fileOK = false;
				break;
			}
		}

		if ( ! fileOK ) {
			KMessageBox::sorry( 0, i18n( "The selected file appears to be an invalid KStars script." ),
					i18n( "Script Validation Failed!" ) );
		} else {
			//DEBUG
			kdDebug() << "running script: " << fname << endl;

			//file is OK, run it!
			KProcess p;
			p << fname;
			p.start( KProcess::DontCare );
		}
	}
}

void KStars::slotPrint() {
	bool switchColors(false);
	// save current colorscheme using copy constructor
	ColorScheme cs( * options()->colorScheme() );

	KPrinter printer( true, QPrinter::HighResolution );

//Suggest Chart color scheme
	if ( options()->colorScheme()->colorNamed( "SkyColor" ) != "#FFFFFF" ) {
		QString message = i18n( "You can save printer ink by using the \"Star Chart\" color scheme, which uses a white background. Would you like to switch to the Star Chart color scheme for printing?" );

		int answer;
		answer = KMessageBox::questionYesNo( 0, message, i18n( "Switch to Star Chart Colors?" ),
			KStdGuiItem::yes(), KStdGuiItem::no(), "askAgainPrintColors" );

		if ( answer == KMessageBox::Yes ) {
			switchColors = true;
			map()->setColors( "chart.colors" );
		//	map()->forceUpdate( true );
		}
	}

	printer.setFullPage( false );
	if( printer.setup( this, i18n("Print sky") ) ) {
		kapp->setOverrideCursor( waitCursor );

		QPainter p;

		//shortcuts to inform wheter to draw different objects
		bool drawPlanets( options()->drawPlanets );
		bool drawMW( options()->drawMilkyWay );
		bool drawCNames( options()->drawConstellNames );
		bool drawCLines( options()->drawConstellLines );
		bool drawGrid( options()->drawGrid );

		p.begin( &printer );
		QPaintDeviceMetrics pdm( p.device() );

		//scale image such that it fills 90% of the x or y dimension on the paint device
		double xscale = pdm.width() / map()->width();
		double yscale = pdm.height() / map()->height();
		double scale = (xscale < yscale) ? xscale : yscale;

		int pdWidth = int( scale * map()->width() );
		int pdHeight = int( scale * map()->height() );
		int x1 = int( 0.5*(pdm.width()  - pdWidth) );
		int y1 = int( 0.5*(pdm.height()  - pdHeight) );

		p.setClipRect( QRect( x1, y1, pdWidth, pdHeight ) );
		p.setClipping( true );

		//Fil background with sky color
		p.fillRect( x1, y1, pdWidth, pdHeight, QBrush( options()->colorScheme()->colorNamed( "SkyColor" ) ) );

		p.translate( x1, y1 );

		QFont stdFont = p.font();
		QFont smallFont = p.font();
		smallFont.setPointSize( stdFont.pointSize() - 2 );

		if ( drawMW ) map()->drawMilkyWay( p, scale );
		if ( drawGrid ) map()->drawCoordinateGrid( p, scale );
		if ( options()->drawEquator ) map()->drawEquator( p, scale );
		if ( options()->drawEcliptic ) map()->drawEcliptic( p, scale );
		if ( drawCLines ) map()->drawConstellationLines( p, scale );
		if ( drawCNames ) map()->drawConstellationNames( p, stdFont, scale );

		// stars and planets use the same font size
		if ( options()->ZoomLevel < 6 ) {
			p.setFont( smallFont );
		} else {
			p.setFont( stdFont );
		}
		map()->drawStars( p, scale );

		map()->drawDeepSkyObjects( p, scale );
		map()->drawSolarSystem( p, drawPlanets, scale );
		map()->drawAttachedLabels( p, scale );
		map()->drawHorizon( p, stdFont, scale );

		p.end();

		kapp->restoreOverrideCursor();
	}

	// restore old color scheme if necessary
	// if printing will aborted the colorscheme will restored too
	if ( switchColors ) {
		options()->colorScheme()->copy( cs );
		// restore colormode in skymap
		map()->setStarColorMode( cs.starColorMode() );
		map()->forceUpdate();
	}
}

//Set Time to CPU clock
void KStars::slotSetTimeToNow() {
	QDateTime now = QDateTime::currentDateTime();
	changeTime( now.date(), now.time() );
}

void KStars::slotToggleTimer() {
	if ( clock()->isActive() ) {
		clock()->stop();
		updateTime();
	} else {
		if ( fabs( clock()->scale() ) > options()->slewTimeScale )
			clock()->setManualMode( true );
		clock()->start();
		if ( clock()->isManualMode() ) map()->forceUpdate();
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
		if ( data()->isSolarSystem( map()->focusObject() ) && data()->temporaryTrail ) {
			((KSPlanetBase*)map()->focusObject())->clearTrail();
			data()->temporaryTrail = false;
		}

		map()->setClickedObject( NULL );
		map()->setFocusObject( NULL );//no longer tracking focusObject
	} else {
		map()->setClickedPoint( map()->focus() );
		options()->isTracking = true;
		actionCollection()->action("track_object")->setIconSet( BarIcon( "encrypted" ) );
	}

	map()->forceUpdate();
}

void KStars::slotManualFocus() {
	FocusDialog focusDialog( this ); // = new FocusDialog( this );

	if ( focusDialog.exec() == QDialog::Accepted ) {
		//If we are correcting for atmospheric refraction, correct the coordinates for that effect
		if ( options()->useAltAz && options()->useRefraction ) {
			focusDialog.point()->EquatorialToHorizontal( LST(), geo()->lat() );
			focusDialog.point()->setAlt( map()->refract( focusDialog.point()->alt(), true ) );
			focusDialog.point()->HorizontalToEquatorial( LST(), geo()->lat() );
		}
		map()->setClickedPoint( focusDialog.point() );
		if ( options()->isTracking ) slotTrack();

		map()->slotCenter();
	}
}

//View Menu
void KStars::slotZoomIn() {
	actionCollection()->action("zoom_out")->setEnabled (true);
	if ( options()->ZoomLevel < MAXZOOMLEVEL ) {
		++options()->ZoomLevel;
		map()->forceUpdate();
	}
	if ( options()->ZoomLevel == MAXZOOMLEVEL )
		actionCollection()->action("zoom_in")->setEnabled (false);
}

void KStars::slotZoomOut() {
	actionCollection()->action("zoom_in")->setEnabled (true);
	if ( options()->ZoomLevel > MINZOOMLEVEL ) {
		--options()->ZoomLevel;
		map()->forceUpdate();
	}
	if ( options()->ZoomLevel == MINZOOMLEVEL )
		actionCollection()->action("zoom_out")->setEnabled (false);
}

void KStars::slotDefaultZoom() {
	options()->ZoomLevel = DEFAULTZOOMLEVEL;
	map()->forceUpdate();

	if ( options()->ZoomLevel > MINZOOMLEVEL )
		actionCollection()->action("zoom_out")->setEnabled (true);
	if ( options()->ZoomLevel < MAXZOOMLEVEL )
		actionCollection()->action("zoom_in")->setEnabled (true);
}

void KStars::slotCoordSys() {
	if ( options()->useAltAz ) {
		options()->useAltAz = false;
		actCoordSys->turnOn();
	} else {
		options()->useAltAz = true;
		actCoordSys->turnOff();
	}
	map()->forceUpdate();
}

//Settings Menu:
void KStars::slotColorScheme() {
	//use mid(3) to exclude the leading "cs_" prefix from the action name
	QString filename = QString( sender()->name() ).mid(3) + ".colors";
	map()->setColors( filename );
}

void KStars::slotTargetSymbol() {
	QString symbolName( sender()->name() );
	options()->setTargetSymbol( symbolName );
	map()->forceUpdate();
}

//Help Menu
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

//InfoBoxes: we only change options here; these are also connected to slots in
//InfoBoxes that actually toggle the display.
	if ( sender()->name() == QString( "show_boxes" ) )
		options()->showInfoBoxes = show;
	if ( sender()->name() == QString( "show_time_box" ) )
		options()->showTimeBox = show;
	if ( sender()->name() == QString( "show_location_box" ) )
		options()->showGeoBox = show;
	if ( sender()->name() == QString( "show_focus_box" ) )
		options()->showFocusBox = show;
}

void KStars::addColorMenuItem( QString name, QString actionName ) {
	colorActionMenu->insert( new KAction( name, 0,
			this, SLOT( slotColorScheme() ), actionCollection(), actionName.local8Bit() ) );
}

void KStars::removeColorMenuItem( QString actionName ) {
	kdDebug() << "removing " << actionName << endl;
	colorActionMenu->remove( actionCollection()->action( actionName.local8Bit() ) );
}

