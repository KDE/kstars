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

#include <kshortcut.h>

#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "skyobject.h"
#include "ksutils.h"
#include "infoboxes.h"
#include "simclock.h"
#include "Options.h"

void KStars::setRaDec( double ra, double dec ) {
	map()->setDestination( new SkyPoint( ra, dec ) );
}

void KStars::setAltAz( double alt, double az ) {
 	map()->setDestinationAltAz(alt,az);
}

void KStars::lookTowards ( const QString direction ) {
  QString dir = direction.lower();
	if (dir == "zenith" || dir=="z") map()->invokeKey( KKey( "Z" ).keyCodeQt() );
	else if (dir == "north" || dir=="n") map()->invokeKey( KKey( "N" ).keyCodeQt() );
	else if (dir == "east"  || dir=="e") map()->invokeKey( KKey( "E" ).keyCodeQt() );
	else if (dir == "south" || dir=="s") map()->invokeKey( KKey( "S" ).keyCodeQt() );
	else if (dir == "west"  || dir=="w") map()->invokeKey( KKey( "W" ).keyCodeQt() );
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
	if ( track != Options::isTracking() ) slotTrack();
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

			data()->setLocation( *loc );

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
