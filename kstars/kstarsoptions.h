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
		/**Constructor. Set default values to all options if param is true.
			*/
		KStarsOptions(bool loadDefaults = true);

		/**Destructor (empty)*/
	  ~KStarsOptions();

		/**Copy constructor
		Copy options values from another KStarsOptions object.  If you add options
		*to KStars, make **SURE** to include them here!
		*/
		KStarsOptions( KStarsOptions& o );

		/**Currently only used in this constructor, but later it may be used for a option wizard.
			*/
		void setDefaultOptions();

		void setMagLimitDrawStar( float newMagnitude );	
	
		/**@returns pointer to the ColorScheme object
			*/
		ColorScheme *colorScheme() { return &CScheme; }

		/**Set new Location.
			*/
		void setLocation(const GeoLocation& l);

		GeoLocation* Location();

		/**CityName, CountryName and ProvinceName are stored in GeoLoaction object.
			*/

		void setCityName(const QString& city);

		void setCountryName(const QString& country);

		void setProvinceName(const QString& prov);

		void setLongitude(const double l);

		void setLatitude(const double l);

		QString cityName();

		QString countryName();

		QString provinceName();

		double longitude();

		double latitude();

		bool snapNextFocus() const { return snapToFocus; }
		void setSnapNextFocus(bool b=true) { snapToFocus = b; }

		bool setTargetSymbol( QString name );

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
		int targetSymbol;
		
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
