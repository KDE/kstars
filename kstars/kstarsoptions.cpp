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

#include "kstarsoptions.h"
#include <kconfig.h>

#include <kapplication.h>

KStarsOptions::KStarsOptions(bool loadDefaults) {
	if ( loadDefaults ) setDefaultOptions();
}

KStarsOptions::~KStarsOptions() {
}

KStarsOptions::KStarsOptions(KStarsOptions& o) {
	// handle ALL members here !!!
	// coordinate system
	useAltAz     = o.useAltAz;

	// draw options
	drawSAO      = o.drawSAO;
	drawMessier  = o.drawMessier;
	drawMessImages  = o.drawMessImages;
	drawNGC      = o.drawNGC;
	drawIC       = o.drawIC;
	drawOther    = o.drawOther;
	drawConstellLines = o.drawConstellLines;
	drawConstellNames = o.drawConstellNames;
	useLatinConstellNames = o.useLatinConstellNames;
	useLocalConstellNames = o.useLocalConstellNames;
	useAbbrevConstellNames = o.useAbbrevConstellNames;
	drawMilkyWay  = o.drawMilkyWay;
	fillMilkyWay  = o.fillMilkyWay;
	drawGrid     = o.drawEquator;
	drawEquator  = o.drawEquator;
 	drawHorizon  = o.drawHorizon;
	drawGround   = o.drawGround;
	drawEcliptic = o.drawEcliptic;
	drawSun      = o.drawSun;
	drawMoon     = o.drawMoon;
	drawMercury     = o.drawMercury;
	drawVenus     = o.drawVenus;
	drawMars     = o.drawMars;
	drawJupiter     = o.drawJupiter;
	drawSaturn     = o.drawSaturn;
	drawUranus     = o.drawUranus;
	drawNeptune     = o.drawNeptune;
	drawPluto     = o.drawPluto;
	drawPlanets     = o.drawPlanets;
	drawDeepSky     = o.drawDeepSky;
	useRefraction = o.useRefraction;
	useAnimatedSlewing = o.useAnimatedSlewing;
	hideOnSlew    = o.hideOnSlew;
	hideStars    = o.hideStars;
	hidePlanets    = o.hidePlanets;
	hideMess     = o.hideMess;
	hideNGC      = o.hideNGC;
	hideIC       = o.hideIC;
	hideOther    = o.hideOther;
	hideMW       = o.hideMW;
	hideCNames   = o.hideCNames;
	hideCLines   = o.hideCLines;
	hideGrid     = o.hideGrid;
	isTracking     = o.isTracking;
	showInfoBoxes = o.showInfoBoxes;
	showTimeBox   = o.showTimeBox;
	showFocusBox  = o.showFocusBox;
	showGeoBox    = o.showGeoBox;
	showMainToolBar = o.showMainToolBar;
	showViewToolBar = o.showViewToolBar;
	focusObject    = o.focusObject;
	focusRA     = o.focusRA;
	focusDec    = o.focusDec;
	slewTimeScale  = o.slewTimeScale;
	windowWidth    = o.windowWidth;
	windowHeight   = o.windowHeight;
	// magnitude limits and other star options
	magLimitDrawStar     = o.magLimitDrawStar;
	magLimitDrawStarInfo = o.magLimitDrawStarInfo;
	magLimitHideStar     = o.magLimitHideStar;
	drawStarName         = o.drawStarName;
	drawPlanetName       = o.drawPlanetName;
	drawPlanetImage      = o.drawPlanetImage;
	drawStarMagnitude		 = o.drawStarMagnitude;

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
	useAltAz = true;
	drawSAO =true;
	drawMessier = true;
	drawMessImages = true;
	drawNGC = true;
	drawIC = true;
	drawOther =true;
	drawConstellLines = true;
	drawConstellNames = true;
	useLatinConstellNames = true;
	useLocalConstellNames = false;
	useAbbrevConstellNames = false;
	drawMilkyWay = true;
	fillMilkyWay = true;
	drawGrid = true;
	drawEquator = true;
	drawHorizon = true;
	drawGround = true;
	drawEcliptic= true;
	drawSun= true;
	drawMoon= true;
	drawMercury= true;
	drawVenus= true;
	drawMars= true;
	drawJupiter= true;
	drawSaturn= true;
	drawUranus= true;
	drawNeptune= true;
	drawPluto= true;
	drawPlanets= true;
	drawDeepSky= true;
	useRefraction= true;
	useAnimatedSlewing= true;
	hideOnSlew= true;
	hideStars= true;
	hidePlanets= true;
	hideMess= true;
	hideNGC= true;
	hideIC= true;
	hideOther= true;
	hideMW= true;
	hideCNames= true;
	hideCLines= true;
	hideGrid= true;
	isTracking= false;
	showInfoBoxes= true;
	showTimeBox= true;
	showFocusBox= true;
	showGeoBox= true;
	showMainToolBar= true;
	showViewToolBar= true;
//	focusObject()
	focusRA = 0.0;
	focusDec = 0.0;
	slewTimeScale = 60.0;
	windowWidth = 600;
	windowHeight = 600;
//  magLimitDrawStar = 8.0;		// read entry below
	magLimitDrawStarInfo = 3.0;
	magLimitHideStar = 5.0;
	drawStarName= true;
	drawPlanetName= true;
	drawPlanetImage= true;
	drawStarMagnitude= true;
}

void KStarsOptions::setMagLimitDrawStar( float newMagnitude ) {
	magLimitDrawStar = newMagnitude;
}

GeoLocation* KStarsOptions::Location() {
	return &location;
}

void KStarsOptions::setLocation(const GeoLocation& l) {
	location = l;
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
	return location.lng().Degrees();
}

double KStarsOptions::latitude() {
	return location.lat().Degrees();
}

