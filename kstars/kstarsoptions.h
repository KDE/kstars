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
#include <qpoint.h>
#include <qvaluelist.h>
#include <qstringlist.h>

#include "colorscheme.h"
#include "geolocation.h"

/**KStarsOptions manages all of the user-configurable options available in KStars.
	*@short KStars options.
	*@author Heiko Evermann	
	*@version 0.9
	*/

class KStarsOptions {
	public:
		/**Constructor. 
			*@param loadDefaults if true, set default values to all options.
			*/
		KStarsOptions(bool loadDefaults = true);

		/**Destructor (empty)*/
	  ~KStarsOptions();

		/**Copy constructor.
			*Copy options values from another KStarsOptions object.  If you add options
			*to KStars, make **SURE** to include them here!
			*@param o the KStarsOptions objects to copy
			*/
		KStarsOptions( KStarsOptions& o );

		/**Set all options to their factory-default values.
			*Currently only used in this constructor, but later it may be used for a option wizard.
			*/
		void setDefaultOptions();

		/**Set the faintest-magnitude-star-drawn value.
			*@param newMagnitude the new limiting magnitude for stars.
			*/
		void setMagLimitDrawStar( float newMagnitude );	
	
		/**@returns pointer to the ColorScheme object
			*/
		ColorScheme *colorScheme() { return &CScheme; }

		/**Set new Geographic Location.  This actually only sets the longitude and latitude from the 
			*given location; there are separate functions for setting the names. (why is it done this way?)
			*@param l the new GeoLocation to set.
			*/
		void setLocation(const GeoLocation& l);

		/**@returns the current Geographic Location as a pointer to a GeoLocation object.
			*/
		GeoLocation* Location();

		/**Modify the current GeoLocation to have the given City name.
			*@param city The new City name.
			*/
		void setCityName(const QString& city);

		/**Modify the current GeoLocation to have the given Country name.
			*@param country The new Country name.
			*/
		void setCountryName(const QString& country);

		/**Modify the current GeoLocation to have the given Province name.
			*@param prov The new Province name.
			*/
		void setProvinceName(const QString& prov);

		/**Modify the current GeoLocation's Longitude.
			*@param l The new Longitude
			*/
		void setLongitude(const double l);

		/**Modify the current GeoLocation's Latitude.
			*@param l The new Latitude
			*/
		void setLatitude(const double l);

		/**@returns the current GeoLocation's city name.
			*/
		QString cityName();

		/**@returns the current GeoLocation's country name.
			*/
		QString countryName();

		/**@returns the current GeoLocation's province name.
			*/
		QString provinceName();

		/**@returns the current GeoLocation's longitude as a double.
			*/
		double longitude();

		/**@returns the current GeoLocation's latitude as a double.
			*/
		double latitude();

		/**@returns snapToFocus setting (should this be in kstarsoptions?)
			*/
		bool snapNextFocus() const { return snapToFocus; }
		
		/**Set the snapToFocus variable.  If true, then the next focus change will not use 
			*animated slewing. (should this be in kstarsoptions?)
			*/
		void setSnapNextFocus(bool b=true) { snapToFocus = b; }

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

		//control drawing of various items in the skymap
		bool drawSAO;
		bool drawMessier;
		bool drawMessImages;
		bool drawNGC;
		bool drawIC;
		bool drawOther;
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
		bool drawStarName;
		bool drawPlanetName;
		bool drawPlanetImage;
		bool drawStarMagnitude;	
		bool drawPlanets;  //Meta-option to control all planets with viewToolBar button
		bool drawDeepSky;  //Meta-option to control all deep-sky objects with viewToolBar button

		//Advanced Display Options (JH: 20 Feb 2002)
		bool useRefraction;
		bool useAnimatedSlewing;
		bool hideOnSlew;
		bool hideStars, hidePlanets, hideMess, hideNGC, hideIC, hideOther;
		bool hideMW, hideCNames, hideCLines, hideGrid;

		//snapToFocus is set true for one-time skipping of animated slews
		bool snapToFocus;

		//GUI options
		bool showMainToolBar, showViewToolBar;
		bool showInfoBoxes;
		bool showTimeBox, showFocusBox, showGeoBox;
		bool shadeTimeBox, shadeFocusBox, shadeGeoBox;
		QPoint posTimeBox, posFocusBox, posGeoBox;

		//Custom Catalogs
		unsigned int CatalogCount;
		QValueList<bool> drawCatalog;
		QStringList CatalogName, CatalogFile;

		bool isTracking;
		QString focusObject;
		float focusRA, focusDec;	
		float slewTimeScale;
		int windowWidth, windowHeight;

		float magLimitDrawStar;
		float magLimitHideStar;
		float magLimitDrawStarInfo;

		//the colors of things
		ColorScheme CScheme;

		private:
			GeoLocation location;  // store all location data here
};


#endif // KSTARSOPTIONS_H
