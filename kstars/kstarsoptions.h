/***************************************************************************
                          kstarsoptions.h  -  K Desktop Planetarium
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


#ifndef KSTARSOPTIONS_H
#define KSTARSOPTIONS_H

#include <qstring.h>

class KStarsOptions
{
public:
	KStarsOptions();
  virtual ~KStarsOptions();

	// Remember to copy all members in the Copy method !!!
	void copy( KStarsOptions* dataSource );

	// Use Horizontal (a.k.a. Altitude-Azimuth) coordinate system?
  // (false=equatorial coordinate system)
	bool useAltAz;

	// to draw or not to draw, that is the question.
	// (Hamlet!)

	//     What a piece of work is KDE!
	//      How noble in reason!
  //       how infinite in faculties!
  //        in Class and Object, how expressive and admirable!
  //         in KStdAction how like an angel!
  //          in apprehension, how like a god!
	//     (more hacked Hamlet :)

	bool drawBSC;
	bool drawMessier;
	bool drawMessImages;
	bool drawNGC;
	bool drawIC;
	bool drawConstellLines;
	bool drawConstellNames;
	bool useLatinConstellNames;
	bool useLocalConstellNames;
	bool useAbbrevConstellNames;
	bool drawMilkyWay;
	bool fillMilkyWay;
	bool drawGrid;
	bool drawEquator;
	bool drawHorizon;
	bool drawGround;
	bool drawEcliptic;
	bool drawSun;
	bool drawMoon;
	bool drawMercury;
	bool drawVenus;
	bool drawMars;
	bool drawJupiter;
	bool drawSaturn;
	bool drawUranus;
	bool drawNeptune;
	bool drawPluto;
	bool isTracking;
	QString focusObject;
	float focusRA, focusDec;	
	int windowWidth, windowHeight;
	float magLimitDrawStar;
	float magLimitDrawStarInfo;
	bool  drawStarName;
	bool  drawStarMagnitude;	

	//the colors of things
	QString colorSky;   //Sky background
	QString	colorMess;  //Messier catalog
	QString	colorNGC;   //NGC catalog
	QString	colorIC;    //IC catalog
 	QString	colorHST;   //object w/ HST image
	QString	colorMW;    //milky way contour
	QString	colorEq;    //celestial equator
	QString	colorEcl;   //ecliptic
	QString	colorHorz;  //horizon
	QString	colorGrid;  //coordinate grid
	QString	colorCLine; //constellation lines
	QString	colorCName; //constellation names
	QString	colorSName; //star names

	int starColorMode;  // 0 = temperature colors; 1 = all red; 2 = all black; 3 = all white
	// intensity of stars
	int starColorIntensity;
	
	//Where are we?
	QString CityName;
	QString ProvinceName, CountryName;
};


#endif // KSTARSOPTIONS_H




