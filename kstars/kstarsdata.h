/***************************************************************************
                          kstarsdata.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Jul 29 2001
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

#ifndef KSTARSDATA_H
#define KSTARSDATA_H

#include <qlist.h>
#include <qfile.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qdatetime.h>
#include <kstddirs.h>
#include <klocale.h>

#include "geolocation.h"
#include "skyobject.h"
#include "starobject.h"
#include "kstarsoptions.h"
#include "ksplanet.h"
#include "kspluto.h"
#include "kssun.h"
#include "ksmoon.h"
#include "skypoint.h"
#include "skyobjectname.h"

class KStarsData
{
public:
	KStarsData();
  virtual ~KStarsData();

	/**
		*Attempt to open the data file named filename, using the QFile object "file".
		*First look in the standard KDE directories, then looc in a local "data"
		*subdirectory.  If the data file cannot be found or opened, display a warning
		*messagebox.
		*@short Open a data file.
		*@author Jason Harris
		*@param &file The QFile object to be opened
		*@param filename The name of the data file.
		*@param doWarn If true, show a warning on failure (default is true).
		*@param required If true, the warning message indicates that KStars can't function without the file.
		*/
	static bool openDataFile( QFile &file, QString filename );
	bool readCityData( void );
	bool readStarData( void );
	bool readNGCData( void );
	bool readCLineData( void );
	bool readMWData( void );
	bool readCNameData( void );
	bool readURLData( QString url, int type=0 );
	long double getJD( QDateTime t);
	bool appendNewStarData( float newMag );
	void deleteUnusedStarData( float newMag );
	
	QList<GeoLocation> geoList;
	QList<SkyObject> *objList;
	QList<StarObject> starList;
	QList<SkyObject> messList;
	QList<SkyObject> ngcList;
	QList<SkyObject> icList;
	QList<SkyPoint> clineList;
	QList<QChar> clineModeList;
	QList<SkyObject> cnameList;
	QList<SkyPoint> Equator;
	QList<SkyPoint> Ecliptic;
	QList<SkyPoint> Horizon;
	QList<SkyPoint> MilkyWay[11];
	QList < SkyObjectName > *ObjNames;

	QString cnameFile;  	
	KStandardDirs *stdDirs;

	QDateTime now, then, LTime, UTime;
  QTime     LST;
	KLocale *locale;

	KSSun *Sun;
	KSMoon *Moon;
	KSPlanet *Mercury, *Venus, *Earth, *Mars, *Jupiter;
	KSPlanet *Saturn, *Uranus, *Neptune;
	KSPluto *Pluto;

//	SkyPoint focus, oldfocus;
//	dms LSTh;
//	double HourAngle;

	double Obliquity, dObliq, dEcLong;
	long double CurrentEpoch, CurrentDate, LastSkyUpdate, LastPlanetUpdate, LastMoonUpdate;
	int starCount1, starCount2, starCount3, starCount0;
	
	// options
	KStarsOptions* options;
	KStarsOptions* oldOptions;

	void saveOptions();
	void restoreOptions();
};


#endif // KSTARSDATA_H

