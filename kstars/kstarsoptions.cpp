/***************************************************************************
                          kstarsoptions.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Aug 5 2001
    copyright            : (C) 2001 by Heiko Evermann
    email                : heiko@evermann.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <kapplication.h>

#include "kstars.h"
#include "kstarsoptions.h"

KStarsOptions::KStarsOptions(bool loadDefaults) {
	if ( loadDefaults ) setDefaultOptions();
}

KStarsOptions::~KStarsOptions() {
}

KStarsOptions::KStarsOptions(KStarsOptions& o) {
	// handle ALL members here !!!
	useAltAz               = o.useAltAz;
	useRefraction          = o.useRefraction;
	useAnimatedSlewing     = o.useAnimatedSlewing;
	useLatinConstellNames  = o.useLatinConstellNames;
	useLocalConstellNames  = o.useLocalConstellNames;
	useAbbrevConstellNames = o.useAbbrevConstellNames;
	fadePlanetTrails       = o.fadePlanetTrails;
	drawSAO         = o.drawSAO;
	drawMessier     = o.drawMessier;
	drawMessImages  = o.drawMessImages;
	drawNGC         = o.drawNGC;
	drawIC          = o.drawIC;
	drawOther       = o.drawOther;
	drawConstellLines = o.drawConstellLines;
	drawConstellNames = o.drawConstellNames;
	drawMilkyWay  = o.drawMilkyWay;
	fillMilkyWay  = o.fillMilkyWay;
	drawGrid      = o.drawEquator;
	drawEquator   = o.drawEquator;
 	drawHorizon   = o.drawHorizon;
	drawGround    = o.drawGround;
	drawEcliptic  = o.drawEcliptic;
	drawSun       = o.drawSun;
	drawMoon      = o.drawMoon;
	drawMercury   = o.drawMercury;
	drawVenus     = o.drawVenus;
	drawMars      = o.drawMars;
	drawJupiter   = o.drawJupiter;
	drawSaturn    = o.drawSaturn;
	drawUranus    = o.drawUranus;
	drawNeptune   = o.drawNeptune;
	drawPluto     = o.drawPluto;
	drawAsteroids = o.drawAsteroids;
	drawComets    = o.drawComets;
	drawStarName      = o.drawStarName;
	drawPlanetName    = o.drawPlanetName;
	drawAsteroidName  = o.drawAsteroidName;
	drawCometName     = o.drawCometName;
	drawPlanetImage   = o.drawPlanetImage;
	drawStarMagnitude = o.drawStarMagnitude;
	drawPlanets     = o.drawPlanets;
	drawDeepSky     = o.drawDeepSky;
	hideOnSlew    = o.hideOnSlew;
	hideStars     = o.hideStars;
	hidePlanets   = o.hidePlanets;
	hideMess      = o.hideMess;
	hideNGC       = o.hideNGC;
	hideIC        = o.hideIC;
	hideOther     = o.hideOther;
	hideMW        = o.hideMW;
	hideCNames    = o.hideCNames;
	hideCLines    = o.hideCLines;
	hideGrid      = o.hideGrid;
	isTracking     = o.isTracking;
	showMainToolBar = o.showMainToolBar;
	showViewToolBar = o.showViewToolBar;
	showInfoBoxes   = o.showInfoBoxes;
	showTimeBox     = o.showTimeBox;
	showFocusBox    = o.showFocusBox;
	showGeoBox      = o.showGeoBox;
	shadeTimeBox    = o.shadeTimeBox;
	shadeFocusBox   = o.shadeFocusBox;
	shadeGeoBox     = o.shadeGeoBox;
	posTimeBox   = o.posTimeBox;
	posFocusBox  = o.posFocusBox;
	posGeoBox    = o.posGeoBox;
	focusObject = o.focusObject;
	focusRA     = o.focusRA;
	focusDec    = o.focusDec;
	targetSymbol = o.targetSymbol;
	slewTimeScale  = o.slewTimeScale;
	ZoomLevel      = o.ZoomLevel;
	windowWidth    = o.windowWidth;
	windowHeight   = o.windowHeight;
	// magnitude limits and other star options
	magLimitDrawStar     = o.magLimitDrawStar;
	magLimitDrawStarInfo = o.magLimitDrawStarInfo;
	magLimitHideStar     = o.magLimitHideStar;
	
	magLimitAsteroid     = o.magLimitAsteroid;
	magLimitAsteroidName = o.magLimitAsteroidName;
	maxRadCometName      = o.maxRadCometName;
	
	// color options
	CScheme.copy( *o.colorScheme() );

	// location, location, location
	setCityName( o.cityName() );
	setProvinceName( o.provinceName() );
	setCountryName( o.countryName() );

	//Custom catalogs
	CatalogCount = o.CatalogCount;
	CatalogFile.clear();
	CatalogName.clear();
	drawCatalog.clear();
	for ( unsigned int i=0; i<CatalogCount; ++i ) {
		CatalogFile.append( o.CatalogFile[i] );
		CatalogName.append( o.CatalogName[i] );
		drawCatalog.append( o.drawCatalog[i] );
	}
}

void KStarsOptions::setDefaultOptions() {
	useAltAz               = true;
	useRefraction          = true;
	useAnimatedSlewing     = true;
	useLatinConstellNames  = true;
	useLocalConstellNames  = false;
	useAbbrevConstellNames = false;
	fadePlanetTrails       = true;
	drawSAO        = true;
	drawMessier    = true;
	drawMessImages = true;
	drawNGC        = true;
	drawIC         = true;
	drawOther      = true;
	drawConstellLines = true;
	drawConstellNames = true;
	drawMilkyWay = true;
	fillMilkyWay = true;
	drawGrid     = true;
	drawEquator  = true;
	drawHorizon  = true;
	drawGround   = true;
	drawEcliptic = true;
	drawSun     = true;
	drawMoon    = true;
	drawMercury = true;
	drawVenus   = true;
	drawMars    = true;
	drawJupiter = true;
	drawSaturn  = true;
	drawUranus  = true;
	drawNeptune = true;
	drawPluto   = true;
	drawAsteroids = true;
	drawComets    = true;
	drawPlanets = true;
	drawDeepSky = true;
	hideOnSlew  = true;
	hideStars   = true;
	hidePlanets = true;
	hideMess    = true;
	hideNGC     = true;
	hideIC      = true;
	hideOther   = true;
	hideMW      = true;
	hideCNames  = true;
	hideCLines  = true;
	hideGrid    = true;
	isTracking = false;
	showMainToolBar = true;
	showViewToolBar = true;
	showInfoBoxes   = true;
	showTimeBox     = true;
	showFocusBox    = true;
	showGeoBox      = true;
	shadeTimeBox    = true;
	shadeFocusBox   = true;
	shadeGeoBox     = true;
	posTimeBox  = QPoint( 0, 0 );
	posFocusBox = QPoint( 600, 0 );
	posGeoBox   = QPoint( 0, 600 );
	focusRA  = 0.0;
	focusDec = 0.0;
	targetSymbol = 0;
	slewTimeScale = 60.0;
	ZoomLevel    = DEFAULTZOOMLEVEL;
	windowWidth  = 600;
	windowHeight = 600;
	magLimitDrawStar = 8.0;
	magLimitDrawStarInfo = 3.0;
	magLimitHideStar = 5.0;
	magLimitAsteroid     = 8.0;
	magLimitAsteroidName = 4.0;
	maxRadCometName = 3.0;
	drawStarName      = true;
	drawPlanetName    = true;
	drawAsteroidName  = true;
	drawCometName     = true;
	drawPlanetImage   = true;
	drawStarMagnitude = true;
}


void KStarsOptions::setMagLimitDrawStar( float newMagnitude ) {
	magLimitDrawStar = newMagnitude;
}

bool KStarsOptions::setTargetSymbol( QString name ) {
	targetSymbol = 0;
	
	if ( name == QString( "target_symbol_circle" ) ) targetSymbol = 1;
	if ( name == QString( "target_symbol_crosshairs" ) ) targetSymbol = 2;
	if ( name == QString( "target_symbol_bullseye" ) ) targetSymbol = 3;
	if ( name == QString( "target_symbol_rectangle" ) ) targetSymbol = 4;
	
	if ( targetSymbol==0 ) return false;
	return true;
}

GeoLocation* KStarsOptions::Location() {
	return &location;
}

void KStarsOptions::setLocation(const GeoLocation& l) {
	GeoLocation l2( l );
	if ( l2.lat()->Degrees() >= 90.0 ) l2.setLat( 89.99 );
	if ( l2.lat()->Degrees() <= -90.0 ) l2.setLat( -89.99 );
	
	location = l2;
}

void KStarsOptions::setCityName(const QString& city) {
	location.setName( city );
}

void KStarsOptions::setCountryName(const QString& country) {
	location.setCountry( country );
}

void KStarsOptions::setProvinceName(const QString& prov) {
	location.setProvince( prov );
}

void KStarsOptions::setLongitude(const double l) {
	location.setLat(l);
}

void KStarsOptions::setLatitude(const double l) {
	location.setLong(l);
}

QString KStarsOptions::cityName() {
   return location.name();
}

QString KStarsOptions::countryName() {
	return location.country();
}

QString KStarsOptions::provinceName() {
	return location.province();
}

double KStarsOptions::longitude() {
	return location.lng()->Degrees();
}

double KStarsOptions::latitude() {
	return location.lat()->Degrees();
}

