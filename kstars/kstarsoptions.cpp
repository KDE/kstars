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

#include <kapp.h>
#include <kconfig.h>

KStarsOptions::KStarsOptions()
	:	useAltAz (true )
	, drawBSC( true )
	, drawMessier( true )
	, drawMessImages( true )
	, drawNGC( true )
	, drawIC( true )
	, drawConstellLines( true )
	, drawConstellNames( true )
	, useLatinConstellNames ( true )
	, useLocalConstellNames ( false )
	, useAbbrevConstellNames ( false )
	, drawMilkyWay( true )
	, fillMilkyWay( true )
	, drawGrid( true )
	, drawEquator( true )
	, drawHorizon( true )
	, drawGround( true )
	, drawEcliptic( true )
	, drawSun( true )
	, drawMoon( true )
	, drawMercury( true )
	, drawVenus( true )
	, drawMars( true )
	, drawJupiter( true )
	, drawSaturn( true )
	, drawUranus( true )
	, drawNeptune( true )
	, drawPluto( true )
	, isTracking( false )
	, focusObject()
  , focusRA( 0.0 )
  , focusDec( 0.0 )
  , windowWidth( 600 )
  , windowHeight( 600 )
//  , magLimitDrawStar( 8.0 )		// read entry below
  , magLimitDrawStarInfo( 3.0 )
  , drawStarName( true )
  , drawStarMagnitude( true )
	, starColorMode( 0 )
  , starColorIntensity( 4 )
 	, colorSky()
	, colorMess()
	, colorNGC()
	, colorIC()
	, colorHST()
	, colorMW()
	, colorEq()
	, colorEcl()
	, colorHorz()
	, colorGrid()
	, colorCLine()
	, colorCName()
	, colorSName()
	,	CityName()
  , ProvinceName()
	,	CountryName()
{
// read entry for loading not more stars than needed
	kapp->config()->setGroup( "View" );
	magLimitDrawStar = kapp->config()->readDoubleNumEntry( "magLimitDrawStar", 8.0 );
}

KStarsOptions::~KStarsOptions()
{

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
	drawBSC      = dataSource->drawBSC;
	drawMessier  = dataSource->drawMessier;
	drawMessImages  = dataSource->drawMessImages;
	drawNGC      = dataSource->drawNGC;
	drawIC       = dataSource->drawIC;
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
	isTracking     = dataSource->isTracking;
	focusObject    = dataSource->focusObject;
	focusRA     = dataSource->focusRA;
	focusDec    = dataSource->focusDec;
	windowWidth    = dataSource->windowWidth;
	windowHeight   = dataSource->windowHeight;
	// magnitude limits and other star options
  magLimitDrawStar			= dataSource->magLimitDrawStar;
	magLimitDrawStarInfo	= dataSource->magLimitDrawStarInfo;
	drawStarName					= dataSource->drawStarName;
	drawStarMagnitude			= dataSource->drawStarMagnitude;
	starColorMode = dataSource->starColorMode;
	starColorIntensity			= dataSource->starColorIntensity;

	// color options
	colorSky		= dataSource->colorSky;
//	colorStar		=	dataSource->colorStar;
	colorMess		=	dataSource->colorMess;
	colorNGC		=	dataSource->colorNGC;
	colorIC			=	dataSource->colorIC;
 	colorHST		= dataSource->colorHST;
	colorMW			=	dataSource->colorMW;
	colorEq			=	dataSource->colorEq;
	colorHorz		=	dataSource->colorHorz;
	colorGrid		=	dataSource->colorGrid;
	colorCLine	=	dataSource->colorCLine;
	colorCName	=	dataSource->colorCName;

	// location, location, location
  CityName = dataSource->CityName;
	ProvinceName = dataSource->ProvinceName;
  CountryName = dataSource->CountryName;
}

void KStarsOptions::setMagLimitDrawStar( float newMagnitude ) {
	magLimitDrawStar = newMagnitude;
}

