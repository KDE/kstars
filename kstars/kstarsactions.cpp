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

//needed in slotRunScript() for chmod() syscall (remote script downloaded to temp file)
#include <sys/stat.h>

#include <kdebug.h>
#include <kshortcut.h>
#include <kiconloader.h>
#include <kio/netaccess.h>
#include <kmessagebox.h>
#include <kprinter.h>
#include <ktempfile.h>
#include <ktip.h>
#include <kconfigdialog.h>
#include <kfiledialog.h>
#include <kpopupmenu.h>
#include <kstatusbar.h>
#include <kprocess.h>
#include <qpaintdevicemetrics.h>
#include <qradiobutton.h>
#include <qcheckbox.h>

#include "opscatalog.h"
#include "opsguides.h"
#include "opssolarsystem.h"
#include "opscolors.h"
#include "opsadvanced.h"

//keep KDE 3.0.x and 3.1.x compatibility
//KDE_VERSION changed from decimal to hex during KDE 3.1.x, so have to use
//the MAJOR/MINOR/RELEASE triplet.  We can assume KDE_VERSION_MAJOR >= 3.
//for KDE_VERSION_RELEASE, I believe the post 3_1_BRANCH stuff started at 90,
//but I'll use 20 just in case (the highest 3.1.x will likely be x=4).
#include <kdeversion.h>
#if ( KDE_VERSION_MINOR >= 1 && KDE_VERSION_RELEASE > 20 )
#include <kinputdialog.h>
#else
#include <klineeditdlg.h>
#include <qvalidator.h>
#endif

#include "kstars.h"
#include "timedialog.h"
#include "locationdialog.h"
#include "finddialog.h"
#include "focusdialog.h"
#include "fovdialog.h"
#include "kswizard.h"
#include "astrocalc.h"
#include "lcgenerator.h"
#include "infoboxes.h"
#include "ksutils.h"
#include "altvstime.h"
#include "wutdialog.h"
#include "indimenu.h"
#include "devmanager.h"
#include "indidriver.h"
#include "scriptbuilder.h"
#include "indiconf.h"
#include "planetviewer.h"
#include "jmoontool.h"
#include "telescopewizardprocess.h"
#include "fitsviewer.h"

//This file contains function definitions for Actions declared in kstars.h

/** ViewToolBar Action.  All of the viewToolBar buttons are connected to this slot. **/

void KStars::slotViewToolBar() {

	if ( sender()->name() == QString( "show_stars" ) ) {
		Options::setShowStars( !Options::showStars() );
	} else if ( sender()->name() == QString( "show_deepsky" ) ) {
		Options::setShowDeepSky( ! Options::showDeepSky() );
	} else if ( sender()->name() == QString( "show_planets" ) ) {
		Options::setShowPlanets( ! Options::showPlanets() );
	} else if ( sender()->name() == QString( "show_clines" ) ) {
		Options::setShowCLines( !Options::showCLines() );
	} else if ( sender()->name() == QString( "show_cnames" ) ) {
		Options::setShowCNames( !Options::showCNames() );
	} else if ( sender()->name() == QString( "show_mw" ) ) {
		Options::setShowMilkyWay( !Options::showMilkyWay() );
	} else if ( sender()->name() == QString( "show_grid" ) ) {
		Options::setShowGrid( !Options::showGrid() );
	} else if ( sender()->name() == QString( "show_horizon" ) ) {
		Options::setShowGround( !Options::showGround() );
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

void KStars::slotWizard() {
	KSWizard wizard(this);
	wizard.exec();
	
	//TODO: Add code here to deal with settings 
}

void KStars::slotLCGenerator() {
	if (AAVSODialog == NULL)
		AAVSODialog = new LCGenerator(this);

	AAVSODialog->show();
}

void KStars::slotAVT() {
	AltVsTime * avt = new AltVsTime(this);
	avt->show();
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

void KStars::slotJMoonTool() {
	JMoonTool jmt(this);
	jmt.exec();
}

void KStars::slotTelescopeWizard()
{
  telescopeWizardProcess twiz(this);
  twiz.exec();
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

   indiconf.timeCheck->setChecked( Options::indiAutoTime() );
   indiconf.GeoCheck->setChecked( Options::indiAutoGeo() );
   indiconf.crosshairCheck->setChecked( Options::indiCrosshairs() );
   indiconf.messagesCheck->setChecked ( Options::indiMessages() );

   if (indiconf.exec() == QDialog::Accepted)
   {
     Options::setIndiAutoTime( indiconf.timeCheck->isChecked() );
     Options::setIndiAutoGeo( indiconf.GeoCheck->isChecked() );
     Options::setIndiCrosshairs( indiconf.crosshairCheck->isChecked() );
     Options::setIndiMessages( indiconf.messagesCheck->isChecked() );

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
			data()->setLocation( *newLocation );

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
			data()->setLST();

			// Make sure Numbers, Moon, planets, and sky objects are updated immediately
			data()->setFullTimeUpdate();

			// If the sky is in Horizontal mode and not tracking, reset focus such that
			// Alt/Az remain constant.
			if ( ! Options::isTracking() && Options::useAltAz() ) {
				map()->focus()->HorizontalToEquatorial( LST(), geo()->lat() );
			}

			// recalculate new times and objects
			data()->setSnapNextFocus();
			updateTime();
		}
	}
}

void KStars::slotViewOps() {
	KStandardDirs stdDirs;
 
	//An instance of your dialog could be already created and could be cached, 
	//in which case you want to display the cached dialog instead of creating 
	//another one 
	if ( KConfigDialog::showDialog( "settings" ) ) return; 
 
	//KConfigDialog didn't find an instance of this dialog, so lets create it : 
	KConfigDialog* dialog = new KConfigDialog( this, "settings", 
					     Options::self() ); 

	connect( dialog, SIGNAL( applyClicked() ), this, SLOT( slotApplySettings() ) );
	connect( dialog, SIGNAL( okClicked() ), this, SLOT( slotApplySettings() ) );

	OpsCatalog *opcatalog    = new OpsCatalog( this, "catalogs" ); 
	OpsGuides  *opguides     = new OpsGuides( this, "guides" ); 
	OpsSolarSystem *opsolsys = new OpsSolarSystem( this, "solarsystem" ); 
	OpsColors  *opcolors     = new OpsColors( this, "colors" ); 
	OpsAdvanced *opadvanced  = new OpsAdvanced( this, "advanced" ); 
 
	dialog->addPage( opcatalog,  i18n("Catalogs"), stdDirs.findResource( "data", "kstars/opscatalog.png" ) ); 
	dialog->addPage( opsolsys,   i18n("Solar System"), stdDirs.findResource( "data", "kstars/opssolarsystem.png" ) ); 
	dialog->addPage( opguides,   i18n("Guides"), stdDirs.findResource( "data", "kstars/opsguides.png" ) ); 
	dialog->addPage( opcolors,   i18n("Colors"), stdDirs.findResource( "data", "kstars/opscolors.png" ) ); 
	dialog->addPage( opadvanced, i18n("Advanced"), stdDirs.findResource( "data", "kstars/opsadvanced.png" ) ); 
 
}

void KStars::slotApplySettings() {
	Options::writeConfig();
	map()->forceUpdate();
}

void KStars::slotSetTime() {
	TimeDialog timedialog ( data()->LTime, this );

	if ( timedialog.exec() == QDialog::Accepted ) {
		data()->changeTime( timedialog.selectedDate(), timedialog.selectedTime() );

		if ( Options::useAltAz() ) {
			map()->focus()->HorizontalToEquatorial( LST(), geo()->lat() );
		}

		//If focusObject has a Planet Trail, clear it and start anew.
		if ( map()->focusObject() && map()->focusObject()->isSolarSystem() &&
		     ((KSPlanetBase*)map()->focusObject())->hasTrail() ) {
		  ((KSPlanetBase*)map()->focusObject())->clearTrail();
		  ((KSPlanetBase*)map()->focusObject())->addToTrail();
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

void KStars::slotOpenFITS()
{

  KURL fileURL = KFileDialog::getOpenURL( QDir::homeDirPath(), "*.fits *.fit *.fts|Flexible Image Transport System");
  
  if (fileURL.isEmpty())
    return;
    
  FITSViewer * fv = new FITSViewer(&fileURL, this);
  fv->show();
  
}

void KStars::slotExportImage() {
	KURL fileURL = KFileDialog::getSaveURL( QDir::homeDirPath(), "image/png image/jpeg image/gif image/x-portable-pixmap image/x-bmp" );
	KTempFile tmpfile;
	QString fname;
	tmpfile.setAutoDelete(true);
	QPixmap skyimage( map()->width(), map()->height() );

	if ( fileURL.isValid() ) {
		if ( fileURL.isLocalFile() ) {
			fname = fileURL.path();
		} else {
			fname = tmpfile.name();
		}

		//Determine desired image format from filename extension
		QString ext = fname.mid( fname.findRev(".")+1 );
		const char* format = "PNG";
		if ( ext.lower() == "png" ) { format = "PNG"; }
		else if ( ext.lower() == "jpg" || ext.lower() == "jpeg" ) { format = "JPG"; }
		else if ( ext.lower() == "gif" ) { format = "GIF"; }
		else if ( ext.lower() == "pnm" ) { format = "PNM"; }
		else if ( ext.lower() == "bmp" ) { format = "BMP"; }
		else { kdWarning() << i18n( "Could not parse image format of %1; assuming PNG!" ).arg( fname ) << endl; }

		map()->exportSkyImage( &skyimage );
		kapp->processEvents(10000);

		if ( ! skyimage.save( fname, format ) ) kdWarning() << i18n( "Unable to save image: %1 " ).arg( fname ) << endl;
		else kdDebug() << i18n( "Saved to file: %1" ).arg( fname ) << endl;

		if ( tmpfile.name() == fname ) { //attempt to upload image to remote location
			if ( ! KIO::NetAccess::upload( tmpfile.name(), fileURL ) ) {
				QString message = i18n( "Could not upload image to remote location: %1" ).arg( fileURL.prettyURL() );
				KMessageBox::sorry( 0, message, i18n( "Could not upload file" ) );
			}
		}
	}
}

void KStars::slotRunScript() {
	KURL fileURL = KFileDialog::getOpenURL( QDir::homeDirPath(), "*.kstars|KStars Scripts (*.kstars)" );
	QFile f;
	QString fname;

	if ( fileURL.isValid() ) {
		if ( ! fileURL.isLocalFile() ) {
			//Warn the user about executing remote code.
			QString message = i18n( "Warning!  You are about to execute a remote shell script on your machine. " );
			message += i18n( "If you absolutely trust the source of this script, press Continue to execute the script. " );
			message += i18n( "To save the file without executing it, press Save. " );
			message += i18n( "To cancel the download, press Cancel. " );

			int result = KMessageBox::warningYesNoCancel( 0, message, i18n( "Really Execute Remote Script?" ),
					i18n( "&Continue" ), i18n( "&Save" ) );

			if ( result == KMessageBox::Cancel ) return;
			if ( result == KMessageBox::No ) { //save file
				KURL saveURL = KFileDialog::getSaveURL( QDir::homeDirPath(), "*.kstars|KStars Scripts (*.kstars)" );
				KTempFile tmpfile;
				tmpfile.setAutoDelete(true);

				while ( ! saveURL.isValid() ) {
					message = i18n( "Save location is invalid.  Try another location?" );
					if ( KMessageBox::warningYesNo( 0, message, i18n( "Invalid save location" ) ) == KMessageBox::No ) return;
					saveURL = KFileDialog::getSaveURL( QDir::homeDirPath(), "*.kstars|KStars Scripts (*.kstars)" );
				}

				if ( saveURL.isLocalFile() ) {
					fname = saveURL.path();
				} else {
					fname = tmpfile.name();
				}

				if( KIO::NetAccess::download( fileURL, fname ) ) {
					chmod( fname.ascii(), S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH ); //make it executable

					if ( tmpfile.name() == fname ) { //upload to remote location
						if ( ! KIO::NetAccess::upload( tmpfile.name(), fileURL ) ) {
							QString message = i18n( "Could not upload image to remote location: %1" ).arg( fileURL.prettyURL() );
							KMessageBox::sorry( 0, message, i18n( "Could not upload file" ) );
						}
					}
				} else {
					KMessageBox::sorry( 0, i18n( "Could not download the file." ), i18n( "Download Error" ) );
				}

				return;
			}
		}

		//Damn the torpedos and full speed ahead, we're executing the script!
		KTempFile tmpfile;
		tmpfile.setAutoDelete(true);

		if ( ! fileURL.isLocalFile() ) {
			fname = tmpfile.name();
			if( KIO::NetAccess::download( fileURL, fname ) ) {
				chmod( fname.ascii(), S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH );
				f.setName( fname );
			}
		} else {
			f.setName( fileURL.path() );
		}

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
			//file is OK, run it!
			KProcess p;
			p << f.name();
			p.start( KProcess::DontCare );

			while ( p.isRunning() ) kapp->processEvents( 50 ); //otherwise tempfile may get deleted before script completes.
		}
	}
}

void KStars::slotPrint() {
	bool switchColors(false);
	// save current colorscheme using copy constructor
	ColorScheme cs( * data()->colorScheme() );

	KPrinter printer( true, QPrinter::HighResolution );

//Suggest Chart color scheme
	if ( cs.colorNamed( "SkyColor" ) != "#FFFFFF" ) {
		QString message = i18n( "You can save printer ink by using the \"Star Chart\" color scheme, which uses a white background. Would you like to switch to the Star Chart color scheme for printing?" );

		int answer;
		answer = KMessageBox::questionYesNoCancel( 0, message, i18n( "Switch to Star Chart Colors?" ),
			KStdGuiItem::yes(), KStdGuiItem::no(), "askAgainPrintColors" );

		if ( answer == KMessageBox::Cancel ) return;

		if ( answer == KMessageBox::Yes ) {
			switchColors = true;
			map()->setColors( "chart.colors" );
		//	map()->forceUpdate( true );
		}
	}

	printer.setFullPage( false );
	if( printer.setup( this, i18n("Print Sky") ) ) {
		kapp->setOverrideCursor( waitCursor );

		map()->setMapGeometry();
		map()->exportSkyImage( &printer );
		kapp->restoreOverrideCursor();
	}

	// restore old color scheme if necessary
	// if printing will aborted the colorscheme will restored too
	if ( switchColors ) {
		data()->colorScheme()->copy( cs );
		// restore colormode in skymap
		map()->setStarColorMode( cs.starColorMode() );
		map()->forceUpdate();
	}
}

//Set Time to CPU clock
void KStars::slotSetTimeToNow() {
	QDateTime now = QDateTime::currentDateTime();
	data()->changeTime( now.date(), now.time() );

	if ( Options::useAltAz() ) {
		map()->focus()->HorizontalToEquatorial( LST(), geo()->lat() );
	}

	//If focusObject has a Planet Trail, clear it and start anew.
	if ( map()->focusObject() && map()->focusObject()->isSolarSystem() &&
	     ((KSPlanetBase*)map()->focusObject())->hasTrail() ) {
	  ((KSPlanetBase*)map()->focusObject())->clearTrail();
	  ((KSPlanetBase*)map()->focusObject())->addToTrail();
	}
}

void KStars::slotToggleTimer() {
	if ( data()->clock()->isActive() ) {
		data()->clock()->stop();
		updateTime();
	} else {
		if ( fabs( data()->clock()->scale() ) > Options::slewTimeScale() )
			data()->clock()->setManualMode( true );
		data()->clock()->start();
		if ( data()->clock()->isManualMode() ) map()->forceUpdate();
	}
}

//Focus
void KStars::slotPointFocus() {
	QString sentFrom( sender()->name() );

	if ( sentFrom == "zenith" )
		map()->invokeKey( KKey( "Z" ).keyCodeQt() );
	else if ( sentFrom == "north" )
		map()->invokeKey( KKey( "N" ).keyCodeQt() );
	else if ( sentFrom == "east" )
		map()->invokeKey( KKey( "E" ).keyCodeQt() );
	else if ( sentFrom == "south" )
		map()->invokeKey( KKey( "S" ).keyCodeQt() );
	else if ( sentFrom == "west" )
		map()->invokeKey( KKey( "W" ).keyCodeQt() );
}

void KStars::slotTrack() {
	if ( Options::isTracking() ) {
		Options::setIsTracking( false );
		actionCollection()->action("track_object")->setText( i18n( "Engage &Tracking" ) );
		actionCollection()->action("track_object")->setIconSet( BarIcon( "decrypted" ) );
		if ( map()->focusObject() && map()->focusObject()->isSolarSystem() && data()->temporaryTrail ) {
			((KSPlanetBase*)map()->focusObject())->clearTrail();
			data()->temporaryTrail = false;
		}

		map()->setClickedObject( NULL );
		map()->setFocusObject( NULL );//no longer tracking focusObject
		map()->setFocusPoint( NULL );
	} else {
		map()->setClickedPoint( map()->focus() );
		Options::setIsTracking( true );
		actionCollection()->action("track_object")->setText( i18n( "Stop &Tracking" ) );
		actionCollection()->action("track_object")->setIconSet( BarIcon( "encrypted" ) );
	}

	map()->forceUpdate();
}

void KStars::slotManualFocus() {
	FocusDialog focusDialog( this ); // = new FocusDialog( this );
	if ( Options::useAltAz() ) focusDialog.activateAzAltPage();

	if ( focusDialog.exec() == QDialog::Accepted ) {
		//Do we need to convert Az/Alt to RA/Dec?
		if ( focusDialog.usedAltAz() )
			focusDialog.point()->HorizontalToEquatorial( LST(), geo()->lat() );

		//If we are correcting for atmospheric refraction, correct the coordinates for that effect
		if ( Options::useAltAz() && Options::useRefraction() ) {
			focusDialog.point()->EquatorialToHorizontal( LST(), geo()->lat() );
			focusDialog.point()->setAlt( map()->refract( focusDialog.point()->alt(), true ) );
			focusDialog.point()->HorizontalToEquatorial( LST(), geo()->lat() );
		}

		map()->setClickedPoint( focusDialog.point() );
		if ( Options::isTracking() ) slotTrack();

		map()->slotCenter();

		//Don't track if we set Alt/Az coordinates.  This way, Alt/Az remain constant.
		if ( focusDialog.usedAltAz() ) map()->stopTracking();
	}
}

//View Menu
void KStars::slotZoomIn() {
	actionCollection()->action("zoom_out")->setEnabled (true);
	if ( Options::zoomFactor() < MAXZOOM )
		Options::setZoomFactor( Options::zoomFactor()*DZOOM );

	if ( Options::zoomFactor() >= MAXZOOM ) {
		Options::setZoomFactor( MAXZOOM );
		actionCollection()->action("zoom_in")->setEnabled (false);
	}

	map()->forceUpdate();
}

void KStars::slotZoomOut() {
	actionCollection()->action("zoom_in")->setEnabled (true);
	if ( Options::zoomFactor() > MINZOOM )
		Options::setZoomFactor( Options::zoomFactor()/DZOOM );

	if ( Options::zoomFactor() <= MINZOOM ) {
		Options::setZoomFactor( MINZOOM );
		actionCollection()->action("zoom_out")->setEnabled (false);
	}

	map()->forceUpdate();
}

void KStars::slotDefaultZoom() {
	Options::setZoomFactor( DEFAULTZOOM );
	map()->forceUpdate();

	if ( Options::zoomFactor() > MINZOOM )
		actionCollection()->action("zoom_out")->setEnabled (true);
	if ( Options::zoomFactor() < MAXZOOM )
		actionCollection()->action("zoom_in")->setEnabled (true);
}

void KStars::slotSetZoom() {
	bool ok( false );
	double currentAngle = map()->width() / ( Options::zoomFactor() * dms::DegToRad );
	double angSize = currentAngle;
	double minAngle = map()->width() / ( MAXZOOM * dms::DegToRad );
	double maxAngle = map()->width() / ( MINZOOM * dms::DegToRad );

	//keep KDE 3.0.x and 3.1.x compatibility
	#if ( KDE_VERSION_MINOR >= 1 && KDE_VERSION_RELEASE > 20 )
	angSize = KInputDialog::getDouble( i18n( "The user should enter an angle for the field-of-view of the display",
			"Enter Desired Field-of-View Angle" ), i18n( "Enter a field-of-view angle in degrees: " ),
			currentAngle, minAngle, maxAngle, 0.1, 1, &ok );
	#else
	QString sCurrent = QString("%1").arg( currentAngle, 0, 'f', 1 );
	QString entry = KLineEditDlg::getText( i18n( "The user should enter an angle for the field-of-view of the display",
			"Enter Desired Field-of-View Angle" ), i18n( "Enter a field-of-view angle in degrees: " ),
			sCurrent, &ok, 0, &QDoubleValidator( minAngle, maxAngle, 1, 0 ) );

	if ( ok ) angSize = entry.toDouble( &ok );
	#endif

	if ( ok ) {
		Options::setZoomFactor( map()->width() / ( angSize * dms::DegToRad ) );

		if ( Options::zoomFactor() <= MINZOOM ) {
			Options::setZoomFactor( MINZOOM );
			actionCollection()->action("zoom_out")->setEnabled( false );
		} else {
			actionCollection()->action("zoom_out")->setEnabled( true );
		}

		if ( Options::zoomFactor() >= MAXZOOM ) {
			Options::setZoomFactor( MAXZOOM );
			actionCollection()->action("zoom_in")->setEnabled( false );
		} else {
			actionCollection()->action("zoom_in")->setEnabled( true );
		}

		map()->forceUpdate();
	}
}

void KStars::slotCoordSys() {
	if ( Options::useAltAz() ) {
		Options::setUseAltAz( false );
		actCoordSys->turnOn();
	} else {
		Options::setUseAltAz( true );
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
	Options::setFOVName( symbolName );
	data()->fovSymbol.setName( symbolName );
	data()->fovSymbol.setSize( Options::fOVSize() );
	data()->fovSymbol.setShape( Options::fOVShape() );
	data()->fovSymbol.setColor( Options::fOVColor().name() );

//Careful!!  If the user selects a small FOV (like HST), this basically crashes kstars :(
//	//set ZoomLevel to match the FOV symbol
//	zoom( (double)(map()->width()) * 60.0 / ( 2.0 * dms::DegToRad * data()->fovSymbol.size() ) );

	map()->forceUpdate();
}

void KStars::slotFOVEdit() {
	FOVDialog fovdlg( this );
	if ( fovdlg.exec() == QDialog::Accepted ) {
		//replace existing fov.dat with data from the FOVDialog
		QFile f;
		f.setName( locateLocal( "appdata", "fov.dat" ) );

		//rebuild fov.dat if FOVList is empty
		if ( fovdlg.FOVList.isEmpty() ) {
			f.remove();
			initFOV();
		} else {
			if ( ! f.open( IO_WriteOnly ) ) {
				kdDebug() << i18n( "Could not open fov.dat for writing!" ) << endl;
			} else {
				QTextStream ostream(&f);

				for ( FOV *fov = fovdlg.FOVList.first(); fov; fov = fovdlg.FOVList.next() )
					ostream << fov->name() << ":" << QString("%1").arg( fov->size(), 0, 'f', 2 )
							<< ":" << QString("%1").arg( fov->shape() ) << ":" << fov->color() << endl;

				f.close();
			}
		}

		//repopulate FOV menu  with items from new fov.dat
		fovActionMenu->popupMenu()->clear();

		if ( f.open( IO_ReadOnly ) ) {
			QTextStream stream( &f );
			while ( !stream.eof() ) {
				QString line = stream.readLine();
				QStringList fields = QStringList::split( ":", line );

				if ( fields.count() == 4 ) {
					QString nm = fields[0].stripWhiteSpace();
					fovActionMenu->insert( new KAction( nm, 0, this, SLOT( slotTargetSymbol() ), actionCollection(), nm.utf8() ) );
				}
			}
		} else {
			kdDebug() << i18n( "Could not open file: %1" ).arg( f.name() ) << endl;
		}

		fovActionMenu->popupMenu()->insertSeparator();
		fovActionMenu->insert( new KAction( i18n( "Edit FOV Symbols..." ), 0, this, SLOT( slotFOVEdit() ), actionCollection(), "edit_fov" ) );

		//set FOV to whatever was highlighted in FOV dialog
		if ( fovdlg.FOVList.count() > 0 ) {
			Options::setFOVName( fovdlg.FOVList.at( fovdlg.currentItem() )->name() );
			data()->fovSymbol.setName( Options::fOVName() );
			data()->fovSymbol.setSize( Options::fOVSize() );
			data()->fovSymbol.setShape( Options::fOVShape() );
			data()->fovSymbol.setColor( Options::fOVColor().name() );
		}

//Careful!!  If the user selects a small FOV (like HST), this basically crashes kstars :(
//		//set ZoomLevel to match the FOV symbol
//		zoom( (double)(map()->width()) * 60.0 / ( 2.0 * dms::DegToRad * data()->fovSymbol.size() ) );
	}
}

//Help Menu
void KStars::slotTipOfDay() {
	KTipDialog::showTip("kstars/tips", true);
}

// Toggle to and from full screen mode
void KStars::slotFullScreen()
{
  if ( topLevelWidget()->isFullScreen() ) {
    topLevelWidget()->showNormal();
    }
  else {
    topLevelWidget()->showFullScreen();
    }
}

//toggle display of GUI Items on/off
void KStars::slotShowGUIItem( bool show ) {
//Toolbars
	if ( sender()->name() == QString( "show_mainToolBar" ) ) {
		Options::setShowMainToolBar( show );
		if ( show ) toolBar( "mainToolBar" )->show();
		else toolBar( "mainToolBar" )->hide();
	}
	if ( sender()->name() == QString( "show_viewToolBar" ) ) {
		Options::setShowViewToolBar( show );
		if ( show ) toolBar( "viewToolBar" )->show();
		else toolBar( "viewToolBar" )->hide();
	}

	if ( sender()->name() == QString( "show_statusBar" ) ) {
		Options::setShowStatusBar( show );
		if ( show ) statusBar()->show();
		else  statusBar()->hide();
	}

	if ( sender()->name() == QString( "show_sbAzAlt" ) ) {
		Options::setShowAltAzField( show );
		if ( show ) {
			//To preserve the order (AzAlt before RADec), we have to remove 
			//the RADec field and then add both back.
			if ( Options::showRADecField() ) statusBar()->removeItem( 2 );
			
			QString s = "000d 00m 00s,   +00d 00\' 00\""; //only need this to set the width
			statusBar()->insertFixedItem( s, 1, true );
			statusBar()->setItemAlignment( 1, AlignRight | AlignVCenter );
			statusBar()->changeItem( "", 1 );

			if ( Options::showRADecField() ) {
				statusBar()->insertFixedItem( s, 2, true );
				statusBar()->setItemAlignment( 2, AlignRight | AlignVCenter );
				statusBar()->changeItem( "", 2 );
			}
		} else {
			statusBar()->removeItem( 1 );
		}
	}
	
	if ( sender()->name() == QString( "show_sbRADec" ) ) {
		Options::setShowRADecField( show );
		if ( show ) {
			QString s = "000d 00m 00s,   +00d 00\' 00\""; //only need this to set the width
			statusBar()->insertFixedItem( s, 2, true );
			statusBar()->setItemAlignment( 2, AlignRight | AlignVCenter );
			statusBar()->changeItem( "", 2 );
		} else {
			statusBar()->removeItem( 2 );
		}
	}

//InfoBoxes: we only change options here; these are also connected to slots in
//InfoBoxes that actually toggle the display.
	if ( sender()->name() == QString( "show_boxes" ) )
		Options::setShowInfoBoxes( show );
	if ( sender()->name() == QString( "show_time_box" ) )
		Options::setShowTimeBox( show );
	if ( sender()->name() == QString( "show_location_box" ) )
		Options::setShowGeoBox( show );
	if ( sender()->name() == QString( "show_focus_box" ) )
		Options::setShowFocusBox( show );
}

void KStars::addColorMenuItem( QString name, QString actionName ) {
	colorActionMenu->insert( new KAction( name, 0,
			this, SLOT( slotColorScheme() ), actionCollection(), actionName.local8Bit() ) );
}

void KStars::removeColorMenuItem( QString actionName ) {
	kdDebug() << "removing " << actionName << endl;
	colorActionMenu->remove( actionCollection()->action( actionName.local8Bit() ) );
}

void KStars::establishINDI()
{
	if (indimenu == NULL)
	  indimenu = new INDIMenu(this);

	if (indidriver == NULL)
	  indidriver = new INDIDriver(this);
}

