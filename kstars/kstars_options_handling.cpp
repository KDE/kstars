/***************************************************************************
                          kstars_options_handling.cpp  -  description
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

#include "kstars.h"
#include "infoboxes.h"

#include <kconfig.h>

void KStars::loadOptions()
{
	KConfig *conf = kapp->config();
	//Check if kstarsrc exists.  If not, we are using default options 
	//(need to know for setting initial focus point).
	//We used to use hasGroup() here, but I have reason to suspect that this
	//this causes a crash on some systems (see bug #44869).  At Kevin
	//Krammer's suggestion, using findResource("kstarsrc") instead.
	//if ( conf->hasGroup( "Location" ) ) useDefaultOptions = false;
	//else useDefaultOptions = true;
	if ( data()->stdDirs->findResource( "config", "kstarsrc" ).isNull() ) useDefaultOptions = true;
	else useDefaultOptions = false;

	// Get initial Location from config()
	conf->setGroup( "Location" );
	// set Greenwich as default
	QString city = conf->readEntry( "City", "Greenwich" );
	QString province = conf->readEntry( "Province", "" );
	QString country = conf->readEntry( "Country", "United Kingdom" );
	double longitude = conf->readDoubleNumEntry( "Longitude", 0.0 );
	double latitude = conf->readDoubleNumEntry( "Latitude", 51.4667 );
	double timezone = conf->readDoubleNumEntry( "TimeZone", 0.0 );
	QString tzRule = conf->readEntry( "DST", "--" );
	double height = conf->readDoubleNumEntry( "Height", -10.0 );
	// find TZrule and if not found use default rule == last rule in QMap
	QMap<QString, TimeZoneRule>::Iterator it = data()->Rulebook.find( tzRule );
	options()->setLocation( GeoLocation ( longitude, latitude, city, province, country, timezone, &(it.data()), 4, height ) );

	conf->setGroup( "Catalogs" );
	options()->CatalogCount = conf->readNumEntry( "CatalogCount", 0 );

	int badCatalog(0);
	for ( unsigned int i=0; i<options()->CatalogCount; ++i ) {
		QString cname = QString( "CatalogName%1" ).arg( i );
		QString cfile = QString( "CatalogFile%1" ).arg( i );
		QString cshow = QString( "ShowCatalog%1" ).arg( i );
		options()->CatalogName.append( conf->readEntry( cname, "" ) );
		options()->CatalogFile.append( conf->readEntry( cfile, "" ) );
		options()->drawCatalog.append( conf->readBoolEntry( cshow, false ) );

		//read in custom catalog:
		QPtrList<SkyObject> oList;
		if ( data()->readCustomData( options()->CatalogFile[i], oList, false ) ) {
			data()->addCatalog( options()->CatalogName[i], oList );
		} else {
			++badCatalog;
			options()->CatalogName.remove( options()->CatalogName.at( i ) );
			options()->CatalogFile.remove( options()->CatalogFile.at( i ) );
			options()->drawCatalog.remove( options()->drawCatalog.at( i ) );
		}
	}
	options()->CatalogCount -= badCatalog; //we didn't add invalid catalogs

	//GUI options
	conf->setGroup( "GUI" );
	options()->showInfoBoxes = conf->readBoolEntry( "ShowInfoBoxes", true );
	options()->showTimeBox   = conf->readBoolEntry( "ShowTimeBox", true );
	options()->showFocusBox  = conf->readBoolEntry( "ShowFocusBox", true );
	options()->showGeoBox    = conf->readBoolEntry( "ShowGeoBox", true );
	options()->shadeTimeBox  = conf->readBoolEntry( "ShadeTimeBox", true );
	options()->shadeFocusBox = conf->readBoolEntry( "ShadeFocusBox", true );
	options()->shadeGeoBox   = conf->readBoolEntry( "ShadeGeoBox", true );
	options()->stickyTimeBox  = conf->readNumEntry( "StickyTimeBox", true );
	options()->stickyFocusBox = conf->readNumEntry( "StickyFocusBox", true );
	options()->stickyGeoBox   = conf->readNumEntry( "StickyGeoBox", true );
	
	QPoint p(0,0);
	options()->posTimeBox    = conf->readPointEntry( "PositionTimeBox", &p );
	p.setX(600);  // (600,0)
	options()->posFocusBox   = conf->readPointEntry( "PositionFocusBox", &p );
	p.setX(0); p.setY(600);  // (0,600)
	options()->posGeoBox     = conf->readPointEntry( "PositionGeoBox", &p );
	
	options()->showMainToolBar = conf->readBoolEntry( "ShowMainToolBar", true );
	options()->showViewToolBar = conf->readBoolEntry( "ShowViewToolBar", true );
	conf->setGroup( "View" );
	options()->colorScheme()->loadFromConfig( conf );
	options()->targetSymbol   = conf->readNumEntry( "TargetSymbol", 0 );
	
	options()->drawSAO        = conf->readBoolEntry( "ShowSAO", true );
	options()->drawMessier    = conf->readBoolEntry( "ShowMess", true );
	options()->drawMessImages = conf->readBoolEntry( "ShowMessImages", true );
	options()->drawNGC        = conf->readBoolEntry( "ShowNGC", true );
	options()->drawIC         = conf->readBoolEntry( "ShowIC", true );
	options()->drawConstellLines = conf->readBoolEntry( "ShowCLines", true );
	options()->drawConstellNames = conf->readBoolEntry( "ShowCNames", true );
	options()->useLatinConstellNames  = conf->readBoolEntry( "UseLatinConstellationNames", true );
	options()->useLocalConstellNames  = conf->readBoolEntry( "UseLocalConstellationNames", false );
	options()->useAbbrevConstellNames = conf->readBoolEntry( "UseAbbrevConstellationNames", false );
	options()->drawMilkyWay = conf->readBoolEntry( "ShowMilkyWay", true );
	options()->drawGrid     = conf->readBoolEntry( "ShowGrid", true );
	options()->drawEquator  = conf->readBoolEntry( "ShowEquator", true );
	options()->drawEcliptic = conf->readBoolEntry( "ShowEcliptic", true );
	options()->drawHorizon  = conf->readBoolEntry( "ShowHorizon", true );
	options()->drawGround   = conf->readBoolEntry( "ShowGround", true );
	options()->drawSun     = conf->readBoolEntry( "ShowSun", true );
	options()->drawMoon    = conf->readBoolEntry( "ShowMoon", true );
	options()->drawMercury = conf->readBoolEntry( "ShowMercury", true );
	options()->drawVenus   = conf->readBoolEntry( "ShowVenus", true );
	options()->drawMars    = conf->readBoolEntry( "ShowMars", true );
	options()->drawJupiter = conf->readBoolEntry( "ShowJupiter", true );
	options()->drawSaturn  = conf->readBoolEntry( "ShowSaturn", true );
	options()->drawUranus  = conf->readBoolEntry( "ShowUranus", true );
	options()->drawNeptune = conf->readBoolEntry( "ShowNeptune", true );
	options()->drawPluto   = conf->readBoolEntry( "ShowPluto", true );
	options()->drawPlanets = conf->readBoolEntry( "ShowPlanets", true );
	options()->drawAsteroids = conf->readBoolEntry( "ShowAsteroids", true );
	options()->drawComets  = conf->readBoolEntry( "ShowComets", true );
	options()->drawPlanets = conf->readBoolEntry( "ShowPlanets", true );
	options()->drawDeepSky = conf->readBoolEntry( "ShowDeepSky", true );
	options()->useAltAz   = conf->readBoolEntry( "UseAltAz", true );
	options()->isTracking = conf->readBoolEntry( "IsTracking", false );
	options()->focusObject = conf->readEntry( "FocusObject", "nothing" );
	options()->focusDec = conf->readDoubleNumEntry( "FocusDec", 45.0 );
	options()->focusRA  = conf->readDoubleNumEntry( "FocusRA", 180.0 );
	options()->slewTimeScale = conf->readDoubleNumEntry( "SlewTimeScale", 60.0 );
	options()->magLimitDrawStar = conf->readDoubleNumEntry( "magLimitDrawStar", 7.0 );
	options()->magLimitDrawStarInfo = conf->readDoubleNumEntry( "magLimitDrawStarInfo", 3.0 );
	options()->magLimitHideStar = conf->readDoubleNumEntry( "magLimitHideStar", 5.0 );
	options()->magLimitAsteroidName = conf->readDoubleNumEntry( "magLimitAsteroid", 8.0 );
	options()->magLimitAsteroidName = conf->readDoubleNumEntry( "magLimitAsteroidName", 4.0 );
	options()->maxRadCometName = conf->readDoubleNumEntry( "maxRadCometName", 3.0 );
	options()->drawStarName      = conf->readBoolEntry( "drawStarName", false );
	options()->drawAsteroidName  = conf->readBoolEntry( "drawAsteroidName", true );
	options()->drawCometName     = conf->readBoolEntry( "drawCometName", true );
	options()->drawPlanetName    = conf->readBoolEntry( "drawPlanetName", true );
	options()->drawPlanetImage   = conf->readBoolEntry( "drawPlanetImage", true );
	options()->drawStarMagnitude = conf->readBoolEntry( "drawStarMagnitude", false );
	options()->ZoomLevel    = conf->readNumEntry( "ZoomLevel", DEFAULTZOOMLEVEL );
	options()->windowWidth  = conf->readNumEntry( "windowWidth", 600 );
	options()->windowHeight = conf->readNumEntry( "windowHeight", 600 );
	options()->useRefraction = conf->readBoolEntry( "UseRefraction", true );
	options()->useAnimatedSlewing = conf->readBoolEntry( "AnimateSlewing", true );
	options()->useAutoLabel       = conf->readBoolEntry( "UseAutoLabel", true );
	options()->useAutoTrail       = conf->readBoolEntry( "UseAutoTrail", true );
	options()->fadePlanetTrails   = conf->readBoolEntry( "FadePlanetTrails", true );
	options()->hideOnSlew  = conf->readBoolEntry( "HideOnSlew", true );
	options()->hideStars   = conf->readBoolEntry( "HideStars", true );
	options()->hidePlanets = conf->readBoolEntry( "HidePlanets", false );
	options()->hideMess    = conf->readBoolEntry( "HideMess", false );
	options()->hideNGC     = conf->readBoolEntry( "HideNGC", true );
	options()->hideIC      = conf->readBoolEntry( "HideIC", true );
	options()->hideMW      = conf->readBoolEntry( "HideMW", true );
	options()->hideCNames  = conf->readBoolEntry( "HideCNames", false );
	options()->hideCLines  = conf->readBoolEntry( "HideCLines", false );
	options()->hideGrid    = conf->readBoolEntry( "HideGrid", true );

	// Get INDI conf from config()
	conf->setGroup( "INDI" );
	options()->INDIHost 	= conf->readEntry( "INDIHost", "localhost" );
	options()->INDIPort 	= conf->readEntry( "INDIPort", "7624");
	options()->isINDILocal	= conf->readBoolEntry( "isINDILocal", true );

}

void KStars::saveOptions() {
	// don't save options if no options or map exists
	// this is normally the case if application was closed over splashscreen
	if ( !map() || !options() ) {
		kdDebug() << k_funcinfo << "Warning: Options not saved!" << endl;
		return;
	}

	KConfig *conf = kapp->config();

	conf->setGroup( "Location" );
	conf->writeEntry( "City", options()->cityName() );
	conf->writeEntry( "Province", options()->provinceName() );
	conf->writeEntry( "Country", options()->countryName() );
	conf->writeEntry( "Longitude", options()->longitude() );
	conf->writeEntry( "Latitude", options()->latitude() );
	conf->writeEntry( "TimeZone", options()->Location()->TZ0() );
// search for key of TZrule and store it in configfile
	QMap<QString, TimeZoneRule>::Iterator it;
	for ( it = data()->Rulebook.begin(); it != data()->Rulebook.end(); ++it ) {
		if ( &(it.data()) == options()->Location()->tzrule() ) break;  // break loop if found
	}
	conf->writeEntry( "DST", it.key() );
	conf->writeEntry( "Height", options()->Location()->height() );

	conf->setGroup( "Catalogs" );
	conf->writeEntry( "CatalogCount", options()->CatalogCount );
	for ( unsigned int i=0; i<options()->CatalogCount; ++i ) {
		QString cname = QString( "CatalogName%1" ).arg( i );
		QString cfile = QString( "CatalogFile%1" ).arg( i );
		QString cshow = QString( "ShowCatalog%1" ).arg( i );
		conf->writeEntry( cname, options()->CatalogName[i] );
		conf->writeEntry( cfile, options()->CatalogFile[i] );
		conf->writeEntry( cshow, options()->drawCatalog[i] );
	}

	
	saveTimeBoxSticky( infoBoxes()->timeBox()->anchorFlag() );
	saveGeoBoxSticky( infoBoxes()->geoBox()->anchorFlag() );
	saveFocusBoxSticky( infoBoxes()->focusBox()->anchorFlag() );
	
	conf->setGroup( "GUI" );
	conf->writeEntry( "ShowInfoBoxes", options()->showInfoBoxes );
	conf->writeEntry( "ShowTimeBox", options()->showTimeBox );
	conf->writeEntry( "ShowFocusBox", options()->showFocusBox );
	conf->writeEntry( "ShowGeoBox", options()->showGeoBox );
	conf->writeEntry( "ShadeTimeBox", options()->shadeTimeBox );
	conf->writeEntry( "ShadeFocusBox", options()->shadeFocusBox );
	conf->writeEntry( "ShadeGeoBox", options()->shadeGeoBox );
	conf->writeEntry( "StickyTimeBox", options()->stickyTimeBox );
	conf->writeEntry( "StickyFocusBox", options()->stickyFocusBox );
	conf->writeEntry( "StickyGeoBox", options()->stickyGeoBox );
	conf->writeEntry( "PositionTimeBox", options()->posTimeBox );
	conf->writeEntry( "PositionFocusBox", options()->posFocusBox );
	conf->writeEntry( "PositionGeoBox", options()->posGeoBox );
	conf->writeEntry( "ShowMainToolBar", options()->showMainToolBar );
	conf->writeEntry( "ShowViewToolBar", options()->showViewToolBar );

	conf->setGroup( "View" );
	options()->colorScheme()->saveToConfig( conf );
	conf->writeEntry( "TargetSymbol", options()->targetSymbol );
	
	conf->writeEntry( "ShowSAO", 		options()->drawSAO );
	conf->writeEntry( "ShowMess", 	options()->drawMessier );
	conf->writeEntry( "ShowMessImages", 	options()->drawMessImages );
	conf->writeEntry( "ShowNGC", 		options()->drawNGC );
	conf->writeEntry( "ShowIC", 		options()->drawIC );
	conf->writeEntry( "ShowCLines", options()->drawConstellLines );
	conf->writeEntry( "ShowCNames", options()->drawConstellNames );
	conf->writeEntry( "UseLatinConstellationNames", options()->useLatinConstellNames );
	conf->writeEntry( "UseLocalConstellationNames", options()->useLocalConstellNames );
	conf->writeEntry( "UseAbbrevConstellationNames", options()->useAbbrevConstellNames );
	conf->writeEntry( "ShowMilkyWay", options()->drawMilkyWay );
	conf->writeEntry( "ShowGrid", options()->drawGrid );
	conf->writeEntry( "ShowEquator", options()->drawEquator );
	conf->writeEntry( "ShowEcliptic", options()->drawEcliptic );
	conf->writeEntry( "ShowHorizon", options()->drawHorizon );
	conf->writeEntry( "ShowGround", options()->drawGround );
	conf->writeEntry( "ShowSun", 		options()->drawSun );
	conf->writeEntry( "ShowMoon", 	options()->drawMoon );
	conf->writeEntry( "ShowMercury", 	options()->drawMercury );
	conf->writeEntry( "ShowVenus", 	options()->drawVenus );
	conf->writeEntry( "ShowMars", 	options()->drawMars );
	conf->writeEntry( "ShowJupiter", 	options()->drawJupiter );
	conf->writeEntry( "ShowSaturn", 	options()->drawSaturn );
	conf->writeEntry( "ShowUranus", 	options()->drawUranus );
	conf->writeEntry( "ShowNeptune", 	options()->drawNeptune );
	conf->writeEntry( "ShowPluto", 	options()->drawPluto );
	conf->writeEntry( "ShowAsteroids", 	options()->drawAsteroids );
	conf->writeEntry( "ShowComets", 	options()->drawComets );
	conf->writeEntry( "ShowPlanets", 	options()->drawPlanets );
	conf->writeEntry( "ShowDeepSky", 	options()->drawDeepSky );
	conf->writeEntry( "IsTracking", 	options()->isTracking );
	if ( map()->focusObject() != NULL ) {
		conf->writeEntry( "FocusObject",  map()->focusObject()->name() );
	} else {
		conf->writeEntry( "FocusObject", i18n( "not focused on any object", "nothing" ) );
	}
	conf->writeEntry( "UseAltAz", 	options()->useAltAz );
	conf->writeEntry( "FocusRA", map()->focus()->ra()->Hours() );
	conf->writeEntry( "FocusDec", map()->focus()->dec()->Degrees() );
	conf->writeEntry( "SlewTimeScale", options()->slewTimeScale );
	conf->writeEntry( "ZoomLevel", options()->ZoomLevel );
	conf->writeEntry( "windowWidth", width() );
	conf->writeEntry( "windowHeight", height() );
	conf->writeEntry( "magLimitDrawStar", 	 options()->magLimitDrawStar );
	conf->writeEntry( "magLimitDrawStarInfo",options()->magLimitDrawStarInfo );
	conf->writeEntry( "magLimitHideStar",options()->magLimitHideStar );
	conf->writeEntry( "magLimitAsteroid",options()->magLimitAsteroid );
	conf->writeEntry( "magLimitAsteroidName",options()->magLimitAsteroidName );
	conf->writeEntry( "maxRadCometName",options()->maxRadCometName );
	conf->writeEntry( "drawStarName", options()->drawStarName );
	conf->writeEntry( "drawAsteroidName", options()->drawAsteroidName );
	conf->writeEntry( "drawCometName", options()->drawCometName );
	conf->writeEntry( "drawPlanetName", options()->drawPlanetName );
	conf->writeEntry( "drawPlanetImage", options()->drawPlanetImage );
	conf->writeEntry( "drawStarMagnitude",   options()->drawStarMagnitude );
	conf->writeEntry( "UseRefraction", options()->useRefraction );
	conf->writeEntry( "AnimateSlewing", options()->useAnimatedSlewing );
	conf->writeEntry( "UseAutoLabel", options()->useAutoLabel );
	conf->writeEntry( "UseAutoTrail", options()->useAutoTrail );
	conf->writeEntry( "FadePlanetTrails", options()->fadePlanetTrails );
	conf->writeEntry( "HideOnSlew", options()->hideOnSlew );
	conf->writeEntry( "HideStars", options()->hideStars );
	conf->writeEntry( "HidePlanets", options()->hidePlanets );
	conf->writeEntry( "HideMess", options()->hideMess );
	conf->writeEntry( "HideNGC", options()->hideNGC );
	conf->writeEntry( "HideIC", options()->hideIC );
	conf->writeEntry( "HideMW", options()->hideMW );
	conf->writeEntry( "HideCNames", options()->hideCNames );
	conf->writeEntry( "HideCLines", options()->hideCLines );
	conf->writeEntry( "HideGrid", options()->hideGrid );

	// Get INDI conf from config()
	conf->setGroup( "INDI" );
	conf->writeEntry( "INDIHost", options()->INDIHost);
	conf->writeEntry( "INDIPort", options()->INDIPort);
	conf->writeEntry( "isINDILocal", options()->isINDILocal);

	conf->sync();
}

void KStars::saveTimeBoxShaded( bool s ) {
	options()->shadeTimeBox = s;
}

void KStars::saveGeoBoxShaded( bool s ) {
	options()->shadeGeoBox = s;
}

void KStars::saveFocusBoxShaded( bool s ) {
	options()->shadeFocusBox = s;
}

void KStars::saveTimeBoxSticky( int s ) {
	options()->stickyTimeBox = s;
}

void KStars::saveGeoBoxSticky( int s ) {
	options()->stickyGeoBox = s;
}

void KStars::saveFocusBoxSticky( int s ) {
	options()->stickyFocusBox = s;
}

void KStars::saveTimeBoxPos( QPoint p ) {
	options()->posTimeBox = p;
}

void KStars::saveGeoBoxPos( QPoint p ) {
	options()->posGeoBox = p;
}

void KStars::saveFocusBoxPos( QPoint p ) {
	options()->posFocusBox = p;
}

