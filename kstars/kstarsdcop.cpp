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

#include <kaccel.h>

#include "kstars.h"
#include "kstarsoptions.h"
#include "ksutils.h"
#include "infoboxes.h"

void KStars::setRaDec( double ra, double dec ) {
	map()->setDestination( new SkyPoint( ra, dec ) );
}

void KStars::setAltAz( double alt, double az ) {
 	map()->setDestinationAltAz(alt,az);
}

void KStars::lookTowards ( const QString direction ) {
  QString dir = direction.lower();
	if (dir == "zenith" || dir=="z") map()->invokeKey( KAccel::stringToKey( "Z" ) );
	else if (dir == "north" || dir=="n") map()->invokeKey( KAccel::stringToKey( "N" ) );
	else if (dir == "east"  || dir=="e") map()->invokeKey( KAccel::stringToKey( "E" ) );
	else if (dir == "south" || dir=="s") map()->invokeKey( KAccel::stringToKey( "S" ) );
	else if (dir == "west"  || dir=="w") map()->invokeKey( KAccel::stringToKey( "W" ) );
	else if (dir == "northeast" || dir=="ne") {
		map()->setClickedObject( NULL );
		map()->clickedPoint()->setAlt( 15.0 ); map()->clickedPoint()->setAz( 45.0 );
		map()->clickedPoint()->HorizontalToEquatorial( LST(), geo()->lat() );
		map()->slotCenter();
	} else if (dir == "southeast" || dir=="se") {
		map()->setClickedObject( NULL );
		map()->clickedPoint()->setAlt( 15.0 ); map()->clickedPoint()->setAz( 135.0 );
		map()->clickedPoint()->HorizontalToEquatorial( LST(), geo()->lat() );
		map()->slotCenter();
	} else if (dir == "southwest" || dir=="sw") {
		map()->setClickedObject( NULL );
		map()->clickedPoint()->setAlt( 15.0 ); map()->clickedPoint()->setAz( 225.0 );
		map()->clickedPoint()->HorizontalToEquatorial( LST(), geo()->lat() );
		map()->slotCenter();
	} else if (dir == "northwest" || dir=="nw") {
		map()->setClickedObject( NULL );
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
	options()->ZoomFactor = z;
}

void KStars::setLocalTime(int yr, int mth, int day, int hr, int min, int sec) {
	data()->changeTime( QDate(yr, mth, day), QTime(hr,min,sec));
}

void KStars::waitFor( double t ) {
	kapp->dcopClient()->suspend();
	QTimer::singleShot( int( 1000.*t ), this, SLOT( resumeDCOP() ) );
}

void KStars::waitForKey( const QString k ) {
	data()->resumeKey = KKey( k );
	if ( ! data()->resumeKey.isNull() ) {
		kapp->dcopClient()->suspend();
	} else {
		kdDebug() << i18n( "Error [DCOP waitForKey()]: Invalid key requested." ) << endl;
	}
}

void KStars::setTracking( bool track ) {
	if ( track != options()->isTracking ) slotTrack();
}

void KStars::popupMessage( int x, int y, QString message ){
	//Show a small popup window at (x,y) with a text message
}

void KStars::drawLine( int x1, int y1, int x2, int y2, int speed ) {
	//Draw a line on the skymap display
}

void KStars::setGeoLocation( QString city, QString province, QString country ) {
	//Set the geographic location
	bool cityFound( false );

	for (GeoLocation *loc = data()->geoList.first(); loc; loc = data()->geoList.next()) {
		if ( loc->translatedName() == city &&
					( province.isEmpty() || loc->translatedProvince() == province ) &&
					loc->translatedCountry() == country ) {

			cityFound = true;

			options()->setLocation( *loc );

			//notify on-screen GeoBox
			infoBoxes()->geoChanged( loc );

			//configure time zone rule
			QDateTime ltime = data()->UTime.addSecs( int( 3600 * loc->TZ0() ) );
			loc->tzrule()->reset_with_ltime( ltime, loc->TZ0(), data()->isTimeRunningForward() );
			data()->setNextDSTChange( KSUtils::UTtoJD( loc->tzrule()->nextDSTChange() ) );

			//reset LST
			data()->setLST( data()->clock()->UTC() );

			//make sure planets, etc. are updated immediately
			data()->setFullTimeUpdate();

			// If the sky is in Horizontal mode and not tracking, reset focus such that
			// Alt/Az remain constant.
			if ( ! options()->isTracking && options()->useAltAz ) {
				map()->focus()->HorizontalToEquatorial( LST(), geo()->lat() );
			}

			// recalculate new times and objects
			options()->setSnapNextFocus();
			updateTime();

			//no need to keep looking, we're done.
			break;
		}
	}

	if ( !cityFound ) {
		if ( province.isEmpty() )
			kdDebug() << i18n( "Error [DCOP setGeoLocation]: city " ) << city << ", "
					<< country << i18n( " not found in database." ) << endl;
		else
			kdDebug() << i18n( "Error [DCOP setGeoLocation]: city " ) << city << ", "
					<< province << ", " << country << i18n( " not found in database." ) << endl;
	}
}

void KStars::changeViewOption( const QString op, const QString val ) {
	bool bOk(false), nOk(false), dOk(false);

	//parse bool value
	bool bVal(false);
	if ( val.lower() == "true" ) { bOk = true; bVal = true; }
	if ( val.lower() == "false" ) { bOk = true; bVal = false; }
	if ( val == "1" ) { bOk = true; bVal = true; }
	if ( val == "0" ) { bOk = true; bVal = false; }

	//parse int value
	int nVal = val.toInt( &nOk );

	//parse double value
	double dVal = val.toDouble( &dOk );

	//[GUI]
	if ( op == "ShowInfoBoxes"   && bOk ) options()->showInfoBoxes   = bVal;
	if ( op == "ShowTimeBox"     && bOk ) options()->showTimeBox     = bVal;
	if ( op == "ShowGeoBox"      && bOk ) options()->showGeoBox      = bVal;
	if ( op == "ShowFocusBox"    && bOk ) options()->showFocusBox    = bVal;
	if ( op == "ShadeTimeBox"    && bOk ) options()->shadeTimeBox    = bVal;
	if ( op == "ShadeGeoBox"     && bOk ) options()->shadeGeoBox     = bVal;
	if ( op == "ShadeFocusBox"   && bOk ) options()->shadeFocusBox   = bVal;
	if ( op == "ShowMainToolBar" && bOk ) options()->showMainToolBar = bVal;
	if ( op == "ShowViewToolBar" && bOk ) options()->showViewToolBar = bVal;

	//[View]
	if ( op == "TargetSymbol"    && nOk ) options()->targetSymbol    = nVal;
	if ( op == "ShowSAO"         && bOk ) options()->drawSAO         = bVal;
	if ( op == "ShowMess"        && bOk ) options()->drawMessier     = bVal;
	if ( op == "ShowMessImages"  && bOk ) options()->drawMessImages  = bVal;
	if ( op == "ShowNGC"         && bOk ) options()->drawNGC         = bVal;
	if ( op == "ShowIC"          && bOk ) options()->drawIC          = bVal;
	if ( op == "ShowCLines"      && bOk ) options()->drawConstellLines = bVal;
	if ( op == "ShowCNames"      && bOk ) options()->drawConstellNames = bVal;
	if ( op == "ShowMilkyWay"    && bOk ) options()->drawMilkyWay    = bVal;
	if ( op == "ShowGrid"        && bOk ) options()->drawGrid        = bVal;
	if ( op == "ShowEquator"     && bOk ) options()->drawEquator     = bVal;
	if ( op == "ShowEcliptic"    && bOk ) options()->drawEcliptic    = bVal;
	if ( op == "ShowHorizon"     && bOk ) options()->drawHorizon     = bVal;
	if ( op == "ShowGround"      && bOk ) options()->drawGround      = bVal;
	if ( op == "ShowSun"         && bOk ) options()->drawSun         = bVal;
	if ( op == "ShowMoon"        && bOk ) options()->drawMoon        = bVal;
	if ( op == "ShowMercury"     && bOk ) options()->drawMercury     = bVal;
	if ( op == "ShowVenus"       && bOk ) options()->drawVenus       = bVal;
	if ( op == "ShowMars"        && bOk ) options()->drawMars        = bVal;
	if ( op == "ShowJupiter"     && bOk ) options()->drawJupiter     = bVal;
	if ( op == "ShowSaturn"      && bOk ) options()->drawSaturn      = bVal;
	if ( op == "ShowUranus"      && bOk ) options()->drawUranus      = bVal;
	if ( op == "ShowNeptune"     && bOk ) options()->drawNeptune     = bVal;
	if ( op == "ShowPluto"       && bOk ) options()->drawPluto       = bVal;
	if ( op == "ShowAsteroids"   && bOk ) options()->drawAsteroids   = bVal;
	if ( op == "ShowComets"      && bOk ) options()->drawComets      = bVal;
	if ( op == "ShowPlanets"     && bOk ) options()->drawPlanets     = bVal;
	if ( op == "ShowDeepSky"     && bOk ) options()->drawDeepSky     = bVal;
	if ( op == "drawStarName"      && bOk ) options()->drawStarName      = bVal;
	if ( op == "drawStarMagnitude" && bOk ) options()->drawStarMagnitude = bVal;
	if ( op == "drawAsteroidName"  && bOk ) options()->drawAsteroidName  = bVal;
	if ( op == "drawCometName"     && bOk ) options()->drawCometName     = bVal;
	if ( op == "drawPlanetName"    && bOk ) options()->drawPlanetName    = bVal;
	if ( op == "drawPlanetImage"   && bOk ) options()->drawPlanetImage   = bVal;
	if ( op == "HideOnSlew"  && bOk ) options()->hideOnSlew  = bVal;
	if ( op == "HideStars"   && bOk ) options()->hideStars   = bVal;
	if ( op == "HidePlanets" && bOk ) options()->hidePlanets = bVal;
	if ( op == "HideMess"    && bOk ) options()->hideMess    = bVal;
	if ( op == "HideNGC"     && bOk ) options()->hideNGC     = bVal;
	if ( op == "HideIC"      && bOk ) options()->hideIC      = bVal;
	if ( op == "HideMW"      && bOk ) options()->hideMW      = bVal;
	if ( op == "HideCNames"  && bOk ) options()->hideCNames  = bVal;
	if ( op == "HideCLines"  && bOk ) options()->hideCLines  = bVal;
	if ( op == "HideGrid"    && bOk ) options()->hideGrid    = bVal;

	if ( op == "UseAltAz"         && bOk ) options()->useAltAz           = bVal;
	if ( op == "UseRefraction"    && bOk ) options()->useRefraction      = bVal;
	if ( op == "UseAutoLabel"     && bOk ) options()->useAutoLabel       = bVal;
	if ( op == "UseAutoTrail"     && bOk ) options()->useAutoTrail       = bVal;
	if ( op == "AnimateSlewing"   && bOk ) options()->useAnimatedSlewing = bVal;
	if ( op == "FadePlanetTrails" && bOk ) options()->fadePlanetTrails   = bVal;
	if ( op == "SlewTimeScale"    && dOk ) options()->slewTimeScale      = dVal;
	if ( op == "ZoomFactor"       && dOk ) options()->ZoomFactor         = dVal;
	if ( op == "magLimitDrawStar"     && dOk ) options()->magLimitDrawStar     = dVal;
	if ( op == "magLimitDrawStarInfo" && dOk ) options()->magLimitDrawStarInfo = dVal;
	if ( op == "magLimitHideStar"     && dOk ) options()->magLimitHideStar     = dVal;
	if ( op == "magLimitAsteroid"     && dOk ) options()->magLimitAsteroid     = dVal;
	if ( op == "magLimitAsteroidName" && dOk ) options()->magLimitAsteroidName = dVal;
	if ( op == "maxRadCometName"      && dOk ) options()->maxRadCometName      = dVal;

	//these three are a "radio group"
	if ( op == "UseLatinConstellationNames" && bOk ) {
		options()->useLatinConstellNames = true;
		options()->useLocalConstellNames = false;
		options()->useAbbrevConstellNames = false;
	}
	if ( op == "UseLocalConstellationNames" && bOk ) {
		options()->useLatinConstellNames = false;
		options()->useLocalConstellNames = true;
		options()->useAbbrevConstellNames = false;
	}
	if ( op == "UseAbbrevConstellationNames" && bOk ) {
		options()->useLatinConstellNames = false;
		options()->useLocalConstellNames = false;
		options()->useAbbrevConstellNames = true;
	}

	map()->forceUpdate();
}
