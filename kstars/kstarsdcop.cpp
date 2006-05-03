/***************************************************************************
                          kstarsdcop.cpp  -  description
                             -------------------
    begin                : Son Apr 7 2002
    copyright            : (C) 2002 by Thomas Kabelmann
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

//KStars DCOP functions

#include <QDir>
#include <QPixmap>
#include <QKeySequence>
#include <QPainter>

#include <kio/netaccess.h>
#include <kmessagebox.h>
#include <kprinter.h>
#include <ktempfile.h>
#include <kurl.h>
#include <k3listview.h>
#include <kpushbutton.h>
#include <klineedit.h>
#include <knuminput.h>

#include "colorscheme.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "skyobject.h"
#include "infoboxes.h"
#include "simclock.h"
#include "Options.h"

// INDI includes
#include "indidriver.h"
#include "indimenu.h"
#include "indielement.h"
#include "indidevice.h"
#include "indiproperty.h"
#include "devicemanager.h"

void KStars::setRaDec( double ra, double dec ) {
	map()->setDestination( new SkyPoint( ra, dec ) );
}

void KStars::setAltAz( double alt, double az ) {
 	map()->setDestinationAltAz(alt,az);
}

void KStars::lookTowards ( const QString &direction ) {
  QString dir = direction.toLower();
	if (dir == "zenith" || dir=="z") map()->invokeKey( Qt::Key_Z );
	else if (dir == "north" || dir=="n") map()->invokeKey( Qt::Key_N );
	else if (dir == "east"  || dir=="e") map()->invokeKey( Qt::Key_E );
	else if (dir == "south" || dir=="s") map()->invokeKey( Qt::Key_S );
	else if (dir == "west"  || dir=="w") map()->invokeKey( Qt::Key_W );
	else if (dir == "northeast" || dir=="ne") {
		map()->stopTracking();
		map()->clickedPoint()->setAlt( 15.0 ); map()->clickedPoint()->setAz( 45.0 );
		map()->clickedPoint()->HorizontalToEquatorial( LST(), geo()->lat() );
		map()->slotCenter();
	} else if (dir == "southeast" || dir=="se") {
		map()->stopTracking();
		map()->clickedPoint()->setAlt( 15.0 ); map()->clickedPoint()->setAz( 135.0 );
		map()->clickedPoint()->HorizontalToEquatorial( LST(), geo()->lat() );
		map()->slotCenter();
	} else if (dir == "southwest" || dir=="sw") {
		map()->stopTracking();
		map()->clickedPoint()->setAlt( 15.0 ); map()->clickedPoint()->setAz( 225.0 );
		map()->clickedPoint()->HorizontalToEquatorial( LST(), geo()->lat() );
		map()->slotCenter();
	} else if (dir == "northwest" || dir=="nw") {
		map()->stopTracking();
		map()->clickedPoint()->setAlt( 15.0 ); map()->clickedPoint()->setAz( 315.0 );
		map()->clickedPoint()->HorizontalToEquatorial( LST(), geo()->lat() );
		map()->slotCenter();
	} else {
		SkyObject *target = data()->objectNamed( direction );
		if ( target != NULL ) {
			map()->setClickedObject( target );
			map()->setClickedPoint( target );
			map()->slotCenter();
		}
	}
}

void KStars::zoom( double z ) {
	if ( z > MAXZOOM ) z = MAXZOOM;
	if ( z < MINZOOM ) z = MINZOOM;
	Options::setZoomFactor( z );
	map()->forceUpdate();
}

void KStars::setLocalTime(int yr, int mth, int day, int hr, int min, int sec) {
	data()->changeDateTime( geo()->LTtoUT( KStarsDateTime( ExtDate(yr, mth, day), QTime(hr,min,sec) ) ) );
}

void KStars::waitFor( double t ) {
	kapp->dcopClient()->suspend();
	QTimer::singleShot( int( 1000.*t ), this, SLOT( resumeDCOP() ) );
}

void KStars::waitForKey( const QString &k ) {
	data()->resumeKey = QKeySequence::fromString( k );
	if ( ! data()->resumeKey.isEmpty() ) {
		kapp->dcopClient()->suspend();
	} else {
		kDebug() << i18n( "Error [DCOP waitForKey()]: Invalid key requested." ) << endl;
	}
}

void KStars::setTracking( bool track ) {
	if ( track != Options::isTracking() ) slotTrack();
}

void KStars::popupMessage( int /*x*/, int /*y*/, const QString& /*message*/ ){
	//Show a small popup window at (x,y) with a text message
}

void KStars::drawLine( int /*x1*/, int /*y1*/, int /*x2*/, int /*y2*/, int /*speed*/ ) {
	//Draw a line on the skymap display
}

void KStars::setGeoLocation( const QString &city, const QString &province, const QString &country ) {
	//Set the geographic location
	bool cityFound( false );

	foreach ( GeoLocation *loc, data()->geoList ) {
		if ( loc->translatedName() == city &&
					( province.isEmpty() || loc->translatedProvince() == province ) &&
					loc->translatedCountry() == country ) {

			cityFound = true;

			data()->setLocation( *loc );

			//notify on-screen GeoBox
			infoBoxes()->geoChanged( loc );

			//configure time zone rule
			KStarsDateTime ltime = loc->UTtoLT( data()->ut() );
			loc->tzrule()->reset_with_ltime( ltime, loc->TZ0(), data()->isTimeRunningForward() );
			data()->setNextDSTChange( loc->tzrule()->nextDSTChange() );

			//reset LST
			data()->syncLST();

			//make sure planets, etc. are updated immediately
			data()->setFullTimeUpdate();

			// If the sky is in Horizontal mode and not tracking, reset focus such that
			// Alt/Az remain constant.
			if ( ! Options::isTracking() && Options::useAltAz() ) {
				map()->focus()->HorizontalToEquatorial( LST(), geo()->lat() );
			}

			// recalculate new times and objects
			data()->setSnapNextFocus();
			updateTime();

			//no need to keep looking, we're done.
			break;
		}
	}

	if ( !cityFound ) {
		if ( province.isEmpty() )
			kDebug() << i18n( "Error [DCOP setGeoLocation]: city " ) << city << ", "
					<< country << i18n( " not found in database." ) << endl;
		else
			kDebug() << i18n( "Error [DCOP setGeoLocation]: city " ) << city << ", "
					<< province << ", " << country << i18n( " not found in database." ) << endl;
	}
}

void KStars::readConfig() {
	//Load config file values into Options object
	Options::self()->readConfig();

	applyConfig();

	//Reset date, if one was stored
	if ( data()->StoredDate.isValid() ) {
		data()->changeDateTime( geo()->LTtoUT( data()->StoredDate ) );
		data()->StoredDate.setDJD( (long double)INVALID_DAY ); //invalidate StoredDate
	}

	map()->forceUpdate();
}

void KStars::writeConfig() {
	Options::writeConfig();

	//Store current simulation time
	data()->StoredDate.setDJD( data()->lt().djd() );
}

QString KStars::getOption( const QString &name ) {
	//Some config items are not stored in the Options object while
	//the program is running; catch these here and returntheir current value.
	if ( name == "FocusRA" ) { return QString::number( map()->focus()->ra()->Hours(), 'f', 6 ); }
	if ( name == "FocusDec" ) { return QString::number( map()->focus()->dec()->Degrees(), 'f', 6 ); }

	KConfigSkeletonItem *it = Options::self()->findItem( name );
	if ( it ) return it->property().toString();
	else return QString();
}

void KStars::changeViewOption( const QString &op, const QString &val ) {
	bool bOk(false), nOk(false), dOk(false);

	//parse bool value
	bool bVal(false);
	if ( val.toLower() == "true" ) { bOk = true; bVal = true; }
	if ( val.toLower() == "false" ) { bOk = true; bVal = false; }
	if ( val == "1" ) { bOk = true; bVal = true; }
	if ( val == "0" ) { bOk = true; bVal = false; }

	//parse int value
	int nVal = val.toInt( &nOk );

	//parse double value
	double dVal = val.toDouble( &dOk );

	//[GUI]
	if ( op == "ShowInfoBoxes"   && bOk ) Options::setShowInfoBoxes(   bVal );
	if ( op == "ShowTimeBox"     && bOk ) Options::setShowTimeBox(     bVal );
	if ( op == "ShowGeoBox"      && bOk ) Options::setShowGeoBox(      bVal );
	if ( op == "ShowFocusBox"    && bOk ) Options::setShowFocusBox(    bVal );
	if ( op == "ShadeTimeBox"    && bOk ) Options::setShadeTimeBox(    bVal );
	if ( op == "ShadeGeoBox"     && bOk ) Options::setShadeGeoBox(     bVal );
	if ( op == "ShadeFocusBox"   && bOk ) Options::setShadeFocusBox(   bVal );
	if ( op == "ShowMainToolBar" && bOk ) Options::setShowMainToolBar( bVal );
	if ( op == "ShowViewToolBar" && bOk ) Options::setShowViewToolBar( bVal );

	//[View]
	if ( op == "FOVName"                ) Options::setFOVName(         val );
	if ( op == "FOVSize"         && dOk ) Options::setFOVSize( (float)dVal );
	if ( op == "FOVShape"        && nOk ) Options::setFOVShape(       nVal );
	if ( op == "FOVColor"               ) Options::setFOVColor(        val );
	if ( op == "ShowStars"       && bOk ) Options::setShowStars(      bVal );
	if ( op == "ShowMessier"     && bOk ) Options::setShowMessier(    bVal );
	if ( op == "ShowMessierImages" && bOk ) Options::setShowMessierImages( bVal );
	if ( op == "ShowNGC"         && bOk ) Options::setShowNGC( bVal );
	if ( op == "ShowIC"          && bOk ) Options::setShowIC(  bVal );
	if ( op == "ShowCLines"      && bOk ) Options::setShowCLines(   bVal );
	if ( op == "ShowCBounds"     && bOk ) Options::setShowCBounds(  bVal );
	if ( op == "ShowCNames"      && bOk ) Options::setShowCNames(   bVal );
	if ( op == "ShowMilkyWay"    && bOk ) Options::setShowMilkyWay( bVal );
	if ( op == "ShowGrid"        && bOk ) Options::setShowGrid(     bVal );
	if ( op == "ShowEquator"     && bOk ) Options::setShowEquator(  bVal );
	if ( op == "ShowEcliptic"    && bOk ) Options::setShowEcliptic( bVal );
	if ( op == "ShowHorizon"     && bOk ) Options::setShowHorizon(  bVal );
	if ( op == "ShowGround"      && bOk ) Options::setShowGround(   bVal );
	if ( op == "ShowSun"         && bOk ) Options::setShowSun(      bVal );
	if ( op == "ShowMoon"        && bOk ) Options::setShowMoon(     bVal );
	if ( op == "ShowMercury"     && bOk ) Options::setShowMercury(  bVal );
	if ( op == "ShowVenus"       && bOk ) Options::setShowVenus(    bVal );
	if ( op == "ShowMars"        && bOk ) Options::setShowMars(     bVal );
	if ( op == "ShowJupiter"     && bOk ) Options::setShowJupiter(  bVal );
	if ( op == "ShowSaturn"      && bOk ) Options::setShowSaturn(   bVal );
	if ( op == "ShowUranus"      && bOk ) Options::setShowUranus(   bVal );
	if ( op == "ShowNeptune"     && bOk ) Options::setShowNeptune(  bVal );
	if ( op == "ShowPluto"       && bOk ) Options::setShowPluto(    bVal );
	if ( op == "ShowAsteroids"   && bOk ) Options::setShowAsteroids( bVal );
	if ( op == "ShowComets"      && bOk ) Options::setShowComets(  bVal );
	if ( op == "ShowPlanets"     && bOk ) Options::setShowPlanets( bVal );
	if ( op == "ShowDeepSky"     && bOk ) Options::setShowDeepSky( bVal );
	if ( op == "ShowStarNames"      && bOk ) Options::setShowStarNames(      bVal );
	if ( op == "ShowStarMagnitudes" && bOk ) Options::setShowStarMagnitudes( bVal );
	if ( op == "ShowAsteroidNames"  && bOk ) Options::setShowAsteroidNames(  bVal );
	if ( op == "ShowCometNames"     && bOk ) Options::setShowCometNames(     bVal );
	if ( op == "ShowPlanetNames"    && bOk ) Options::setShowPlanetNames(    bVal );
	if ( op == "ShowPlanetImages"   && bOk ) Options::setShowPlanetImages(   bVal );
	if ( op == "HideOnSlew"  && bOk ) Options::setHideOnSlew(  bVal );
	if ( op == "HideStars"   && bOk ) Options::setHideStars(   bVal );
	if ( op == "HidePlanets" && bOk ) Options::setHidePlanets( bVal );
	if ( op == "HideMessier" && bOk ) Options::setHideMessier( bVal );
	if ( op == "HideNGC"     && bOk ) Options::setHideNGC(     bVal );
	if ( op == "HideIC"      && bOk ) Options::setHideIC(      bVal );
	if ( op == "HideMilkyWay" && bOk ) Options::setHideMilkyWay( bVal );
	if ( op == "HideCNames"  && bOk ) Options::setHideCNames(  bVal );
	if ( op == "HideCLines"  && bOk ) Options::setHideCLines(  bVal );
	if ( op == "HideCBounds" && bOk ) Options::setHideCBounds( bVal );
	if ( op == "HideGrid"    && bOk ) Options::setHideGrid(    bVal );

	if ( op == "UseAltAz"         && bOk ) Options::setUseAltAz(      bVal );
	if ( op == "UseRefraction"    && bOk ) Options::setUseRefraction( bVal );
	if ( op == "UseAutoLabel"     && bOk ) Options::setUseAutoLabel(  bVal );
	if ( op == "UseHoverLabel"    && bOk ) Options::setUseHoverLabel( bVal );
	if ( op == "UseAutoTrail"     && bOk ) Options::setUseAutoTrail(  bVal );
	if ( op == "UseAnimatedSlewing"   && bOk ) Options::setUseAnimatedSlewing( bVal );
	if ( op == "FadePlanetTrails" && bOk ) Options::setFadePlanetTrails( bVal );
	if ( op == "SlewTimeScale"    && dOk ) Options::setSlewTimeScale(    dVal );
	if ( op == "ZoomFactor"       && dOk ) Options::setZoomFactor(       dVal );
	if ( op == "MagLimitDrawStar"     && dOk )    Options::setMagLimitDrawStar(    dVal );
	if ( op == "MagLimitDrawDeepSky"     && dOk ) Options::setMagLimitDrawDeepSky( dVal );
	if ( op == "MagLimitDrawStarZoomOut" && dOk ) Options::setMagLimitDrawStarZoomOut(        dVal );
	if ( op == "MagLimitDrawDeepSkyZoomOut" && dOk ) Options::setMagLimitDrawDeepSkyZoomOut(  dVal );
	if ( op == "MagLimitDrawStarInfo" && dOk ) Options::setMagLimitDrawStarInfo( dVal );
	if ( op == "MagLimitHideStar"     && dOk ) Options::setMagLimitHideStar(     dVal );
	if ( op == "MagLimitAsteroid"     && dOk ) Options::setMagLimitAsteroid(     dVal );
	if ( op == "MagLimitAsteroidName" && dOk ) Options::setMagLimitAsteroidName( dVal );
	if ( op == "MaxRadCometName"      && dOk ) Options::setMaxRadCometName(      dVal );

	//these three are a "radio group"
	if ( op == "UseLatinConstellationNames" && bOk ) {
		Options::setUseLatinConstellNames( true );
		Options::setUseLocalConstellNames( false );
		Options::setUseAbbrevConstellNames( false );
	}
	if ( op == "UseLocalConstellationNames" && bOk ) {
		Options::setUseLatinConstellNames( false );
		Options::setUseLocalConstellNames( true );
		Options::setUseAbbrevConstellNames( false );
	}
	if ( op == "UseAbbrevConstellationNames" && bOk ) {
		Options::setUseLatinConstellNames( false );
		Options::setUseLocalConstellNames( false );
		Options::setUseAbbrevConstellNames( true );
	}

	map()->forceUpdate();
}

void KStars::setColor( const QString &name, const QString &value ) {
	ColorScheme *cs = data()->colorScheme();
	if ( cs->hasColorNamed( name ) ) {
		cs->setColor( name, value );
		map()->forceUpdate();
	}
}

void KStars::loadColorScheme( const QString &_name ) {
	QString name( _name );
	QString filename = name.toLower().trimmed();
	bool ok( false );

	//Parse default names which don't follow the regular file-naming scheme
	if ( name == i18nc("use default color scheme", "Default Colors") ) filename = "default.colors";
	if ( name == i18nc("use 'star chart' color scheme", "Star Chart") ) filename = "chart.colors";
	if ( name == i18nc("use 'night vision' color scheme", "Night Vision") ) filename = "night.colors";

	//Try the filename if it ends with ".colors"
	if ( filename.endsWith( ".colors" ) )
		ok = data()->colorScheme()->load( filename );

	//If that didn't work, try assuming that 'name' is the color scheme name
	//convert it to a filename exactly as ColorScheme::save() does
	if ( ! ok ) {
		if ( !filename.isEmpty() ) {
			for( int i=0; i<filename.length(); ++i)
				if ( filename.at(i)==' ' ) filename.replace( i, 1, "-" );

			filename = filename.append( ".colors" );
			ok = data()->colorScheme()->load( filename );
		}

		if ( ! ok ) kDebug() << i18n( "Unable to load color scheme named %1. Also tried %2.", name, filename );
	}

	if ( ok ) {
		//set the application colors for the Night Vision scheme
		if ( Options::darkAppColors() == false && filename == "night.colors" )  {
			Options::setDarkAppColors( true );
			OriginalPalette = QApplication::palette();
			QApplication::setPalette( DarkPalette );
		}

		if ( Options::darkAppColors() && filename != "night.colors" ) {
			Options::setDarkAppColors( false );
			QApplication::setPalette( OriginalPalette );
		}

		map()->forceUpdate();
	}
}

void KStars::exportImage( const QString &url, int w, int h ) {
	//If the filename string contains no "/" separators, assume the
	//user wanted to place a file in their home directory.
	KUrl fileURL;
	if ( ! url.contains( "/" ) ) fileURL = QDir::homePath() + '/' + url;
	else fileURL = url;

	KTempFile tmpfile;
	QString fname;
	tmpfile.setAutoDelete(true);

	QPixmap skyimage( map()->width(), map()->height() );
	QPixmap outimage( w, h );
	outimage.fill();

	if ( fileURL.isValid() ) {
		if ( fileURL.isLocalFile() ) {
			fname = fileURL.path();
		} else {
			fname = tmpfile.name();
		}

		//Determine desired image format from filename extension
		QString ext = fname.mid( fname.lastIndexOf(".")+1 );
		const char* format = "PNG";
		if ( ext.toLower() == "png" ) { format = "PNG"; }
		else if ( ext.toLower() == "jpg" || ext.toLower() == "jpeg" ) { format = "JPG"; }
		else if ( ext.toLower() == "gif" ) { format = "GIF"; }
		else if ( ext.toLower() == "pnm" ) { format = "PNM"; }
		else if ( ext.toLower() == "bmp" ) { format = "BMP"; }
		else { kWarning() << i18n( "Could not parse image format of %1; assuming PNG.", fname ) << endl; }

		map()->exportSkyImage( &skyimage );
		kapp->processEvents();

		//skyImage is the size of the sky map.  The requested image size is w x h.
		//If w x h is smaller than the skymap, then we simply crop the image.
		//If w x h is larger than the skymap, pad the skymap image with a white border.
		if ( w == map()->width() && h == map()->height() ) {
                        outimage = skyimage.copy();
                } else {
                        int dx(0), dy(0), sx(0), sy(0);
			int sw(map()->width()), sh(map()->height());
			if ( w > map()->width() ) {
				dx = (w - map()->width())/2;
			} else {
				sx = (map()->width() - w)/2;
				sw = w;
			}
			if ( h > map()->height() ) {
				dy = (h - map()->height())/2;
			} else {
				sy = (map()->height() - h)/2;
				sh = h;
			}

                        QPainter p;
                        p.begin( &outimage );
                        p.fillRect( outimage.rect(), QBrush( Qt::white ) );
                        p.drawImage( dx, dy, skyimage.toImage(), sx, sy, sw, sh );
			p.end();
		}

		if ( ! outimage.save( fname, format ) ) kDebug() << i18n( "Error: Unable to save image: %1 ", fname ) << endl;
		else kDebug() << i18n( "Image saved to file: %1", fname ) << endl;

		if ( tmpfile.name() == fname ) { //attempt to upload image to remote location
			if ( ! KIO::NetAccess::upload( tmpfile.name(), fileURL, this ) ) {
				QString message = i18n( "Could not upload image to remote location: %1", fileURL.prettyURL() );
				KMessageBox::sorry( 0, message, i18n( "Could not upload file" ) );
			}
		}
	}
}

void KStars::printImage( bool usePrintDialog, bool useChartColors ) {
	KPrinter printer( true, QPrinter::HighResolution );
	printer.setFullPage( false );

	//Set up the printer (either with the Print Dialog,
	//or using the default settings)
	bool ok( false );
	if ( usePrintDialog )
		ok = printer.setup( this, i18n("Print Sky") );
	else
		ok = printer.autoConfigure();

	if( ok ) {
		kapp->setOverrideCursor( Qt::WaitCursor );

		//Save current colorscheme and switch to Star Chart colors
		//(if requested)
		ColorScheme cs;
		if ( useChartColors ) {
			cs.copy( * data()->colorScheme() );
			loadColorScheme( "chart.colors" );
		}

		map()->setMapGeometry();
		map()->exportSkyImage( &printer );

		//Restore old color scheme if necessary
		//(if printing was aborted, the colorscheme is still restored)
		if ( useChartColors ) {
			data()->colorScheme()->copy( cs );
			map()->forceUpdate();
		}

		kapp->restoreOverrideCursor();
	}
}

void KStars::startINDI (const QString &deviceName, bool useLocal)
{

  establishINDI();
  QList<QTreeWidgetItem *> found;

  if (!indidriver || !indimenu)
  {
    kDebug() << "establishINDI() failed." << endl;
    return;
  }
	QTreeWidgetItem *driverItem = NULL;
	found = indidriver->ui->localTreeWidget->findItems(deviceName, Qt::MatchExactly | Qt::MatchRecursive);

	if (found.empty())
	{
	   kDebug() << "Device " << deviceName << " not found!" << endl;
	   return;
	}

	driverItem = found.first();

	// If device is already running, we need to shut it down first
	if (indidriver->isDeviceRunning(deviceName))
	{
		indidriver->ui->localTreeWidget->setCurrentItem(driverItem);
		indidriver->processDeviceStatus(1);
	}

	// Set custome label for device
	indimenu->setCustomLabel(deviceName);
 	// Set DCOP device name
	indimenu->setCurrentDevice(deviceName);

	// Select it
	indidriver->ui->localTreeWidget->setCurrentItem(driverItem);

	// Start it either locally or as series
	if (useLocal)
		indidriver->ui->localR->setChecked(true);
	else
		indidriver->ui->serverR->setChecked(true);

	// Run it
	indidriver->processDeviceStatus(0);
}

void KStars::setINDIDevice (const QString &deviceName)
{

  if (!indidriver || !indimenu)
  {
    kDebug() << "establishINDI() failed." << endl;
    return;
  }

  indimenu->setCurrentDevice(deviceName);

}

void KStars::shutdownINDI (const QString &deviceName)
{
  QList<QTreeWidgetItem *> found;

  if (!indidriver || !indimenu)
  {
    kDebug() << "establishINDI() failed." << endl;
    return;
  }

	QTreeWidgetItem *driverItem = NULL;
	found = indidriver->ui->localTreeWidget->findItems(deviceName, Qt::MatchExactly | Qt::MatchRecursive);

	if (found.empty())
	{
	   kDebug() << "Device " << deviceName << " not found!" << endl;
	   return;
	}

	driverItem = found.first();

	indidriver->ui->localTreeWidget->setCurrentItem(driverItem);
	indidriver->processDeviceStatus(1);
}


void KStars::switchINDI(bool turnOn)
{
  INDI_D *dev;
  INDI_P *prop;

  if (!indidriver || !indimenu)
  {
    kDebug() << "switchINDI: establishINDI() failed." << endl;
    return;
  }

  dev = indimenu->findDevice(indimenu->getCurrentDevice());
  if (!dev)
    dev = indimenu->findDeviceByLabel(indimenu->getCurrentDevice());
  if (!dev)
  {
    kDebug() << "Device " << indimenu->getCurrentDevice() << " not found!" << endl;
    return;
  }

  if (turnOn && dev->isOn() || (!turnOn && !dev->isOn()))
   return;

  prop = dev->findProp("CONNECTION");
  if (!prop) return;

  if (turnOn)
    prop->newSwitch(0);
  else
    prop->newSwitch(1);

}


void KStars::setINDIPort(const QString &port)
{
  INDI_D *dev;
  INDI_P *prop;
  INDI_E *el;

   if (!indidriver || !indimenu)
  {
    kDebug() << "setINDIPort: establishINDI() failed." << endl;
    return;
  }

  dev = indimenu->findDevice(indimenu->getCurrentDevice());
  if (!dev)
    dev = indimenu->findDeviceByLabel(indimenu->getCurrentDevice());
  if (!dev)
  {
    kDebug() << "Device " << indimenu->getCurrentDevice() << " not found!" << endl;
    return;
  }

  prop = dev->findProp("DEVICE_PORT");
  if (!prop) return;

  el   = prop->findElement("PORT");

  if (!el->write_w)
   return;

  el->write_w->setText(port);

  prop->newText();

}


void KStars::setINDITargetCoord(double RA, double DEC)
{
  INDI_D *dev;
  INDI_P *prop;
  INDI_E *el;

  if (!indidriver || !indimenu)
  {
    kDebug() << "setINDITarget: establishINDI() failed." << endl;
    return;
  }

  dev = indimenu->findDevice(indimenu->getCurrentDevice());
  if (!dev)
     dev = indimenu->findDeviceByLabel(indimenu->getCurrentDevice());
  if (!dev)
  {
    kDebug() << "Device " << indimenu->getCurrentDevice() << " not found!" << endl;
    return;
  }

  prop = dev->findProp("EQUATORIAL_EOD_COORD");
  if (!prop) return;

  el   = prop->findElement("RA");
  if (!el) return;
  if (!el->write_w) return;

  el->write_w->setText(QString::number(RA));

  el  = prop->findElement("DEC");
  if (!el) return;
  if (!el->write_w) return;

  el->write_w->setText(QString::number(DEC));

  prop->newText();

}


void KStars::setINDITargetName(const QString &objectName)
{
  INDI_D *dev;
  INDI_P *prop;
  INDI_E *el;

  if (!indidriver || !indimenu)
  {
    kDebug() << "setINDITarget: establishINDI() failed." << endl;
    return;
  }

  SkyObject *target = data()->objectNamed( objectName );
  if (!target) return;

  dev = indimenu->findDevice(indimenu->getCurrentDevice());
  if (!dev)
    dev = indimenu->findDeviceByLabel(indimenu->getCurrentDevice());
  if (!dev)
  {
    kDebug() << "Device " << indimenu->getCurrentDevice() << " not found!" << endl;
    return;
  }

  prop = dev->findProp("EQUATORIAL_EOD_COORD");
  if (!prop) return;

  el   = prop->findElement("RA");
  if (!el) return;
  if (!el->write_w) return;

  el->write_w->setText(QString::number(target->ra()->Hours()));

  el  = prop->findElement("DEC");
  if (!el) return;
  if (!el->write_w) return;

  el->write_w->setText(QString::number(target->dec()->Degrees()));

  prop->newText();

}


void KStars::setINDIAction(const QString &action)
{
  INDI_D *dev;
  INDI_E *el;

  if (!indidriver || !indimenu)
  {
    kDebug() << "setINDIAction: establishINDI() failed." << endl;
    return;
  }

  dev = indimenu->findDevice(indimenu->getCurrentDevice());
  if (!dev)
    dev = indimenu->findDeviceByLabel(indimenu->getCurrentDevice());
  if (!dev)
  {
    kDebug() << "Device " << indimenu->getCurrentDevice() << " not found!" << endl;
    return;
  }

  el = dev->findElem(action);
  if (!el) return;

  el->pp->activateSwitch(action);

}

void KStars::waitForINDIAction(const QString &action)
{

  INDI_D *dev;
  INDI_P *prop;
  INDI_E *el;

  if (!indidriver || !indimenu)
  {
    kDebug() << "waitForINDIAction: establishINDI() failed." << endl;
    return;
  }

  dev = indimenu->findDevice(indimenu->getCurrentDevice());
  if (!dev)
    dev = indimenu->findDeviceByLabel(indimenu->getCurrentDevice());
  if (!dev)
  {
    kDebug() << "Device " << indimenu->getCurrentDevice() << " not found!" << endl;
    return;
  }

  prop = dev->findProp(action);

  if (prop == NULL)
  {
    el = dev->findElem(action);
    if (!el) return;

    QObject::connect(el->pp, SIGNAL(okState()), this, SLOT(resumeDCOP(void )));
  }
  else
    QObject::connect(prop, SIGNAL(okState()), this, SLOT(resumeDCOP(void )));

  kapp->dcopClient()->suspend();

}


void KStars::setINDIFocusSpeed(unsigned int speed)
{
  INDI_D *dev;
  INDI_P *prop;
  INDI_E *el;

  if (!indidriver || !indimenu)
  {
    kDebug() << "setINDIFocusSpeed: establishINDI() failed." << endl;
    return;
  }

  dev = indimenu->findDevice(indimenu->getCurrentDevice());
  if (!dev)
    dev = indimenu->findDeviceByLabel(indimenu->getCurrentDevice());
  if (!dev)
  {
    kDebug() << "Device " << indimenu->getCurrentDevice() << " not found!" << endl;
    return;
  }

  prop = dev->findProp("FOCUS_SPEED");
  if (!prop) return;

  el   = prop->findElement("SPEED");
  if (!el) return;
  if (!el->write_w) return;

  el->write_w->setText(QString::number(speed));

  prop->newText();

}


void KStars::startINDIFocus(int focusDir)
{
  if (!indidriver || !indimenu)
  {
    kDebug() << "setINDIFocus: establishINDI() failed!" << endl;
    return;
  }

  if (focusDir == 0)
    setINDIAction("IN");
  else if (focusDir == 1)
    setINDIAction("OUT");

}


void KStars::setINDIGeoLocation(double longitude, double latitude)
{
  INDI_D *dev;
  INDI_P *prop;
  INDI_E *el;

  if (!indidriver || !indimenu)
  {
    kDebug() << "setINDIGeoLocation: establishINDI() failed." << endl;
    return;
  }

  dev = indimenu->findDevice(indimenu->getCurrentDevice());
  if (!dev)
    dev = indimenu->findDeviceByLabel(indimenu->getCurrentDevice());
  if (!dev)
  {
    kDebug() << "Device " << indimenu->getCurrentDevice() << " not found!" << endl;
    return;
  }

  prop = dev->findProp("GEOGRAPHICAL_COORD");
  if (!prop) return;

  el   = prop->findElement("LONG");
  if (!el) return;
  if (!el->write_w) return;

  el->write_w->setText(QString::number(longitude));

  el  = prop->findElement("LAT");
  if (!el) return;
  if (!el->write_w) return;

  el->write_w->setText(QString::number(latitude));

  prop->newText();

}


void KStars::setINDIFocusTimeout(int timeout)
{
  INDI_D *dev;
  INDI_P *prop;
  INDI_E *el;

  if (!indidriver || !indimenu)
  {
    kDebug() << "startINDIFocus: establishINDI() failed." << endl;
    return;
  }

  dev = indimenu->findDevice(indimenu->getCurrentDevice());
  if (!dev)
    dev = indimenu->findDeviceByLabel(indimenu->getCurrentDevice());
  if (!dev)
  {
    kDebug() << "Device " << indimenu->getCurrentDevice() << " not found!" << endl;
    return;
  }

  prop = dev->findProp("FOCUS_TIMEOUT");
  if (!prop)
   return;

  el   = prop->findElement("TIMEOUT");
  if (!el) return;

  if (el->write_w)
    el->write_w->setText(QString::number(timeout));
  else if (el->spin_w)
    el->spin_w->setValue(timeout);

  prop->newText();

}


void KStars::startINDIExposure(int timeout)
{
  INDI_D *dev;
  INDI_P *prop;
  INDI_E *el;

  if (!indidriver || !indimenu)
  {
    kDebug() << "startINDIExposure: establishINDI() failed." << endl;
    return;
  }

  dev = indimenu->findDevice(indimenu->getCurrentDevice());
  if (!dev)
    dev = indimenu->findDeviceByLabel(indimenu->getCurrentDevice());
  if (!dev)
  {
    kDebug() << "Device " << indimenu->getCurrentDevice() << " not found!" << endl;
    return;
  }

  prop = dev->findProp("CCD_EXPOSE_DURATION");
  if (!prop) return;

  el   = prop->findElement("EXPOSE_DURATION");
  if (!el) return;

  if (el->write_w)
    el->write_w->setText(QString::number(timeout));
  else if (el->spin_w)
    el->spin_w->setValue(timeout);


  prop->newText();

}

void KStars::setINDIFilterNum(int filter_num)
{
  INDI_D *dev;
  INDI_P *prop;
  INDI_E *el;

  if (!indidriver || !indimenu)
  {
    kDebug() << "setINDIFilterNum: establishINDI() failed." << endl;
    return;
  }

  dev = indimenu->findDevice(indimenu->getCurrentDevice());
  if (!dev)
    dev = indimenu->findDeviceByLabel(indimenu->getCurrentDevice());
  if (!dev)
  {
    kDebug() << "Device " << indimenu->getCurrentDevice() << " not found!" << endl;
    return;
  }

  prop = dev->findProp("FILTER_SLOT");
  if (!prop) return;

  el   = prop->findElement("SLOT");
  if (!el) return;

  if (el->write_w)
    el->write_w->setText(QString::number(filter_num));
  else if (el->spin_w)
    el->spin_w->setValue(filter_num);

  prop->newText();

}

void KStars::setINDIUTC(const QString &UTCDateTime)
{
  INDI_D *dev;
  INDI_P *prop;
  INDI_E *el;

  if (!indidriver || !indimenu)
  {
    kDebug() << "startINDIUTC: establishINDI() failed." << endl;
    return;
  }

  dev = indimenu->findDevice(indimenu->getCurrentDevice());
  if (!dev)
    dev = indimenu->findDeviceByLabel(indimenu->getCurrentDevice());
  if (!dev)
  {
    kDebug() << "Device " << indimenu->getCurrentDevice() << " not found!" << endl;
    return;
  }

  prop = dev->findProp("TIME");
  if (!prop) return;

  el   = prop->findElement("UTC");
  if (!el) return;
  if (!el->write_w) return;

  el->write_w->setText(UTCDateTime);

  prop->newText();

}

void KStars::setINDIScopeAction(const QString &action)
{
  setINDIAction(action);
}

void KStars::setINDIFrameType(const QString &type)
{
  setINDIAction(type);
}

void KStars::setINDICCDTemp(int temp)
{
  INDI_D *dev;
  INDI_P *prop;
  INDI_E *el;

  if (!indidriver || !indimenu)
  {
    kDebug() << "setINDICCDTemp: establishINDI() failed." << endl;
    return;
  }

  dev = indimenu->findDevice(indimenu->getCurrentDevice());
  if (!dev)
    dev = indimenu->findDeviceByLabel(indimenu->getCurrentDevice());
  if (!dev)
  {
    kDebug() << "Device " << indimenu->getCurrentDevice() << " not found!" << endl;
    return;
  }

  prop = dev->findProp("CCD_TEMPERATURE");
  if (!prop) return;

  el   = prop->findElement("TEMPERATURE");
  if (!el) return;

  if (el->write_w)
    el->write_w->setText(QString::number(temp));
  else if (el->spin_w)
    el->spin_w->setValue(temp);


  prop->newText();

}
