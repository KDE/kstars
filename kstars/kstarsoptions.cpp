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

#include <qglobal.h>
#if (QT_VERSION <= 299)
#include <kapp.h>
#else 
#include <kapplication.h>
#endif

KStarsOptions::KStarsOptions()
{
	setDefaultOptions();
}

void KStarsOptions::copy( KStarsOptions* dataSource )
{
	if ( 0 == dataSource ) {
		// this should not happen
  	return;
	}
	// handle ALL members here !!!
	// coordinate system
	useAltAz     = dataSource->useAltAz;

	// draw options
	drawSAO      = dataSource->drawSAO;
	drawMessier  = dataSource->drawMessier;
	drawMessImages  = dataSource->drawMessImages;
	drawNGC      = dataSource->drawNGC;
	drawIC       = dataSource->drawIC;
	drawOther    = dataSource->drawOther;
	drawConstellLines = dataSource->drawConstellLines;
	drawConstellNames = dataSource->drawConstellNames;
	useLatinConstellNames = dataSource->useLatinConstellNames;
	useLocalConstellNames = dataSource->useLocalConstellNames;
	useAbbrevConstellNames = dataSource->useAbbrevConstellNames;
	drawMilkyWay  = dataSource->drawMilkyWay;
	fillMilkyWay  = dataSource->fillMilkyWay;
	drawGrid     = dataSource->drawEquator;
	drawEquator  = dataSource->drawEquator;
 	drawHorizon  = dataSource->drawHorizon;
	drawGround   = dataSource->drawGround;
	drawEcliptic = dataSource->drawEcliptic;
	drawSun      = dataSource->drawSun;
	drawMoon     = dataSource->drawMoon;
	drawMercury     = dataSource->drawMercury;
	drawVenus     = dataSource->drawVenus;
	drawMars     = dataSource->drawMars;
	drawJupiter     = dataSource->drawJupiter;
	drawSaturn     = dataSource->drawSaturn;
	drawUranus     = dataSource->drawUranus;
	drawNeptune     = dataSource->drawNeptune;
	drawPluto     = dataSource->drawPluto;
	drawPlanets     = dataSource->drawPlanets;
	drawDeepSky     = dataSource->drawDeepSky;
	useRefraction = dataSource->useRefraction;
	useAnimatedSlewing = dataSource->useAnimatedSlewing;
	hideOnSlew    = dataSource->hideOnSlew;
	hideStars    = dataSource->hideStars;
	hidePlanets    = dataSource->hidePlanets;
	hideMess     = dataSource->hideMess;
	hideNGC      = dataSource->hideNGC;
	hideIC       = dataSource->hideIC;
	hideOther    = dataSource->hideOther;
	hideMW       = dataSource->hideMW;
	hideCNames   = dataSource->hideCNames;
	hideCLines   = dataSource->hideCLines;
	hideGrid     = dataSource->hideGrid;
	isTracking     = dataSource->isTracking;
	showInfoPanel = dataSource->showInfoPanel;
	showIPTime    = dataSource->showIPTime;
	showIPFocus   = dataSource->showIPFocus;
	showIPGeo     = dataSource->showIPGeo;
	showMainToolBar = dataSource->showMainToolBar;
	showViewToolBar = dataSource->showViewToolBar;
	focusObject    = dataSource->focusObject;
	focusRA     = dataSource->focusRA;
	focusDec    = dataSource->focusDec;
	slewTimeScale  = dataSource->slewTimeScale;
	windowWidth    = dataSource->windowWidth;
	windowHeight   = dataSource->windowHeight;
	// magnitude limits and other star options
  magLimitDrawStar     = dataSource->magLimitDrawStar;
	magLimitDrawStarInfo = dataSource->magLimitDrawStarInfo;
	magLimitHideStar     = dataSource->magLimitHideStar;
	drawStarName         = dataSource->drawStarName;
	drawPlanetName       = dataSource->drawPlanetName;
	drawPlanetImage      = dataSource->drawPlanetImage;
	drawStarMagnitude		 = dataSource->drawStarMagnitude;
	starColorMode        = dataSource->starColorMode;
	starColorIntensity   = dataSource->starColorIntensity;

	// color options
	colorSky		= dataSource->colorSky;
	colorMess   = dataSource->colorMess;
	colorNGC    = dataSource->colorNGC;
	colorIC     = dataSource->colorIC;
 	colorHST    = dataSource->colorHST;
	colorMW			=	dataSource->colorMW;
	colorEq			=	dataSource->colorEq;
	colorEcl    = dataSource->colorEcl;
	colorHorz		= dataSource->colorHorz;
	colorGrid   = dataSource->colorGrid;
	colorCLine  = dataSource->colorCLine;
	colorCName  = dataSource->colorCName;
	colorPName  = dataSource->colorPName;
	colorSName  = dataSource->colorSName;

	// location, location, location
	CityName = dataSource->CityName;
	ProvinceName = dataSource->ProvinceName;
	CountryName = dataSource->CountryName;

	//Custom catalogs
	CatalogCount = dataSource->CatalogCount;
	CatalogFile.clear();
	CatalogName.clear();
	drawCatalog.clear();
	for ( unsigned int i=0; i<CatalogCount; ++i ) {
		CatalogFile.append( dataSource->CatalogFile[i] );
		CatalogName.append( dataSource->CatalogName[i] );
		drawCatalog.append( dataSource->drawCatalog[i] );
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
	showInfoPanel= true;
	showIPTime= true;
	showIPFocus= true;
	showIPGeo= true;
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
	starColorMode = 0;
	starColorIntensity = 4;
/*
 	colorSky()
	colorMess()
	colorNGC()
	colorIC()
	colorHST()
	colorMW()
	colorEq()
	colorEcl()
	colorHorz()
	colorGrid()
	colorCLine()
	colorCName()
	colorPName()
	colorSName()
	CityName()
	ProvinceName()
	CountryName()
	CatalogCount(0)
	CatalogName()
	CatalogFile()
	drawCatalog()
*/
}

void KStarsOptions::setMagLimitDrawStar( float newMagnitude ) {
	magLimitDrawStar = newMagnitude;
}

