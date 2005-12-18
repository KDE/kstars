/***************************************************************************
                          kstarsdata.cpp  -  K Desktop Planetarium
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

#include <QRegExp>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <Q3PtrList>

#include <kapplication.h>
#include <kmessagebox.h>
#include <kdebug.h>
#include <klocale.h>
#include <kstandarddirs.h>

#include "kstarsdata.h"

#include "Options.h"
#include "dms.h"
#include "skymap.h"
#include "ksutils.h"
#include "ksfilereader.h"
#include "ksnumbers.h"
#include "skypoint.h"
#include "skyobject.h"

#include "simclock.h"
#include "timezonerule.h"
#include "indidriver.h"
#include "indi/lilxml.h"
#include "indistd.h"
#include "detaildialog.h"

//Initialize static members
QList<GeoLocation*> KStarsData::geoList = QList<GeoLocation*>();

QMap<QString, TimeZoneRule> KStarsData::Rulebook = QMap<QString, TimeZoneRule>();

int KStarsData::objects = 0;

QString TypeName[] = { 
		i18n( "star" ),
		i18n( "multiple star" ),
		i18n( "planet" ),
		i18n( "open cluster" ),
		i18n( "globular cluster" ),
		i18n( "gaseous nebula" ),
		i18n( "planetary nebula" ),
		i18n( "supernova remnant" ),
		i18n( "galaxy" ),
		i18n( "comet" ),
		i18n( "asteroid" ),
		i18n( "constellation" )
};

KStarsData::KStarsData() : stdDirs(0), locale(0), 
		LST(0), HourAngle(0), initTimer(0)
{
	startupComplete = false;
	objects++;

	//standard directories and locale objects
	stdDirs = new KStandardDirs();
	locale = new KLocale( "kstars" );

	//Check to see if config file already exists.  If not, set 
	//useDefaultOptions = true
	QString fname = locateLocal( "config", "kstarsrc" );
	useDefaultOptions = ! ( QFile(fname).exists() );

	//Instantiate LST and HourAngle
	LST = new dms();
	HourAngle = new dms();
	
	//initialize FOV symbol
	fovSymbol = FOV();

	VariableStarsList.setAutoDelete(TRUE);
	INDIHostsList.setAutoDelete(TRUE);
	INDITelescopeList.setAutoDelete(TRUE);

	// at startup times run forward
	setTimeDirection( 0.0 );

	//The StoredDate is used when saving user settings in a script; initialize to invalid date
	StoredDate.setDJD( (long double)INVALID_DAY );

	temporaryTrail = false;
}

KStarsData::~KStarsData() {
	//FIXME: Do we still need this?
	objects--; //the number of existing KStarsData objects 

	//FIXME: Verify list of deletes
	delete stdDirs;
	delete locale;
	delete initTimer;

	while ( ! geoList.isEmpty() ) 
		delete geoList.takeFirst();
}

void KStarsData::initialize() {
	if (startupComplete) return;

	initTimer = new QTimer;
	QObject::connect(initTimer, SIGNAL(timeout()), this, SLOT( slotInitialize() ) );
	initCounter = 0;
	initTimer->start(1);
}

void KStarsData::initError(QString s, bool required = false) {
	QString message, caption;
	initTimer->stop();
	if (required) {
		message = i18n( "The file %1 could not be found. "
				"KStars cannot run properly without this file. "
				"To continue loading, place the file in one of the "
				"following locations, then press Retry:\n\n" ).arg( s )
				+ QString( "\t$(KDEDIR)/share/apps/kstars/%1\n" ).arg( s )
				+ QString( "\t~/.kde/share/apps/kstars/%1\n\n" ).arg( s ) 
				+ i18n( "Otherwise, press Cancel to shutdown." );
		caption = i18n( "Critical File Not Found: %1" ).arg( s );
	} else {
		message = i18n( "The file %1 could not be found. "
				"KStars can still run without this file. "
				"However, to avoid seeing this message in the future, you can "
				"place the file in one of the following locations, then press Retry:\n\n" ).arg( s )
				+ QString( "\t$(KDEDIR)/share/apps/kstars/%1\n" ).arg( s )
				+ QString( "\t~/.kde/share/apps/kstars/%1\n\n" ).arg( s )
				+ i18n( "Otherwise, press Cancel to continue loading without this file." ).arg( s );
		caption = i18n( "Non-Critical File Not Found: %1" ).arg( s );
	}

	if ( KMessageBox::warningContinueCancel( 0, message, caption, i18n( "Retry" ) ) == KMessageBox::Continue ) {
		initCounter--;
		initTimer->start(1);
	} else {
		if (required) {
			delete initTimer;
			initTimer = 0L;
			emit initFinished(false);
		} else {
			initTimer->start(1);
		}
	}
}

void KStarsData::slotInitialize() {
	QFile imFile;
	QString ImageName;

	kapp->flush(); // flush all paint events before loading data

	switch ( initCounter )
	{
		case 0: //Load Time Zone Rules//
			emit progressText( i18n("Reading time zone rules") );

			if (objects==1) {
				// timezone rules
				if ( !readTimeZoneRulebook( ) )
					initError( "TZrules.dat", true );
			}

			// read INDI hosts file, not required
			readINDIHosts();
			break;

		case 1: //Load Cities//
		{
			if (objects>1) break;

			emit progressText( i18n("Loading city data") );

			if ( !readCityData( ) )
				initError( "Cities.dat", true );
			
			break;
		}

		case 2: //Initialize SkyMapComposite//

//			emit progressText(i18n("Loading sky objects" ) );
			skyComposite()->init( this );
			break;

		case 3: //Load Image URLs//

			emit progressText( i18n("Loading Image URLs" ) );
			if ( !readURLData( "image_url.dat", 0 ) ) {
				initError( "image_url.dat", false );
			}
			//if ( !readURLData( "myimage_url.dat", 0 ) ) {
			//Don't do anything if the local file is not found.
			//}
          // doesn't belong here, only temp
             readUserLog();

			break;

		case 4: //Load Information URLs//

			emit progressText( i18n("Loading Information URLs" ) );
			if ( !readURLData( "info_url.dat", 1 ) ) {
				initError( "info_url.dat", false );
			}
			//if ( !readURLData( "myinfo_url.dat", 1 ) ) {
			//Don't do anything if the local file is not found.
			//}
			break;

		default:
			initTimer->stop();
			delete initTimer;
			initTimer = 0L;
			startupComplete = true;
			emit initFinished(true);
			break;
	} // switch ( initCounter )

	initCounter++;
}

void KStarsData::updateTime( GeoLocation *geo, SkyMap *skymap, const bool automaticDSTchange ) {
	// sync LTime with the simulation clock
	LTime = geo->UTtoLT( ut() );
	syncLST();
	
	//Only check DST if (1) TZrule is not the empty rule, and (2) if we have crossed
	//the DST change date/time.
	if ( !geo->tzrule()->isEmptyRule() ) {
		if ( TimeRunsForward ) {
			// timedirection is forward
			// DST change happens if current date is bigger than next calculated dst change
			if ( ut() > NextDSTChange ) resetToNewDST(geo, automaticDSTchange);
		} else {
			// timedirection is backward
			// DST change happens if current date is smaller than next calculated dst change
			if ( ut() < NextDSTChange ) resetToNewDST(geo, automaticDSTchange);
		}
	}

	KSNumbers num( ut().djd() );

	if ( fabs( ut().djd() - LastNumUpdate.djd() ) > 1.0 ) {
		LastNumUpdate = ut().djd();
		skyComposite()->update( this, &num );
	}

	if ( fabs( ut().djd() - LastPlanetUpdate.djd() ) > 0.01 ) {
		LastPlanetUpdate = ut().djd();
		skyComposite()->updatePlanets( this, &num );
	}

	// Moon moves ~30 arcmin/hr, so update its position every minute.
	if ( fabs( ut().djd() - LastMoonUpdate.djd() ) > 0.00069444 ) {
		LastMoonUpdate = ut();
		skyComposite()->updateMoons( this, &num );
	}

	//Update Alt/Az coordinates.  Timescale varies with zoom level
	//If Clock is in Manual Mode, always update. (?)
	if ( fabs( ut().djd() - LastSkyUpdate.djd() ) > 0.25/Options::zoomFactor() || clock()->isManualMode() ) {
		LastSkyUpdate = ut();
		skyComposite()->update( this ); //omit KSNumbers arg == just update Alt/Az coords

		//FIXME: Do we need an INDIComponent ?
//		for (SkyObject *o = INDITelescopeList.first(); o; o = INDITelescopeList.next())
//			o->EquatorialToHorizontal(LST, geo->lat());

		//Update focus
		skymap->updateFocus();

		if ( clock()->isManualMode() )
			QTimer::singleShot( 0, skymap, SLOT( forceUpdateNow() ) );
		else skymap->forceUpdate();
	}
}

void KStarsData::setFullTimeUpdate() {
	LastSkyUpdate.setDJD(    (long double)INVALID_DAY );
	LastPlanetUpdate.setDJD( (long double)INVALID_DAY );
	LastMoonUpdate.setDJD(   (long double)INVALID_DAY );
	LastNumUpdate.setDJD(    (long double)INVALID_DAY );
}

void KStarsData::syncLST() {
	LST->set( geo()->GSTtoLST( ut().gst() ) );
}

void KStarsData::changeDateTime( const KStarsDateTime &newDate ) {
	//Turn off animated slews for the next time step.
	setSnapNextFocus();
	
	clock()->setUTC( newDate );
	LTime = geo()->UTtoLT( ut() );
	//set local sideral time
	syncLST();
	
	//Make sure Numbers, Moon, planets, and sky objects are updated immediately
	setFullTimeUpdate();

	// reset tzrules data with new local time and time direction (forward or backward)
	geo()->tzrule()->reset_with_ltime(LTime, geo()->TZ0(), isTimeRunningForward() );

	// reset next dst change time
	setNextDSTChange( geo()->tzrule()->nextDSTChange() );
}

void KStarsData::resetToNewDST(const GeoLocation *geo, const bool automaticDSTchange) {
	// reset tzrules data with local time, timezone offset and time direction (forward or backward)
	// force a DST change with option true for 3. parameter
	geo->tzrule()->reset_with_ltime( LTime, geo->TZ0(), TimeRunsForward, automaticDSTchange );
	// reset next DST change time
	setNextDSTChange( geo->tzrule()->nextDSTChange() );
	//reset LTime, because TZoffset has changed
	LTime = geo->UTtoLT( ut() );
}

void KStarsData::setTimeDirection( float scale ) {
	TimeRunsForward = ( scale < 0 ? false : true );
}

void KStarsData::setLocationFromOptions() {
	QMap<QString, TimeZoneRule>::Iterator it = Rulebook.find( Options::dST() );
	setLocation( GeoLocation ( Options::longitude(), Options::latitude(), 
			Options::cityName(), Options::provinceName(), Options::countryName(), 
			Options::timeZone(), &(it.data()), 4, Options::elevation() ) );
}

void KStarsData::setLocation( const GeoLocation &l ) {
 	GeoLocation g( l );
	if ( g.lat()->Degrees() >= 90.0 ) g.setLat( 89.99 );
	if ( g.lat()->Degrees() <= -90.0 ) g.setLat( -89.99 );

	Geo = g;

	//store data in the Options objects
	Options::setCityName( g.name() );
	Options::setProvinceName( g.province() );
	Options::setCountryName( g.country() );
	Options::setTimeZone( g.TZ0() );
	Options::setElevation( g.height() );
	Options::setLongitude( g.lng()->Degrees() );
	Options::setLatitude( g.lat()->Degrees() );
}

SkyObject* KStarsData::objectNamed( const QString &name ) {
	if ( (name== "star") || (name== "nothing") || name.isEmpty() ) return NULL;

	return skyComposite()->findByName( name );
}

bool KStarsData::readCityData( void ) {
	QFile file;
	bool citiesFound = false;

// begin new code
	if ( KSUtils::openDataFile( file, "Cities.dat" ) ) {
    KSFileReader fileReader( file ); // close file is included
    while ( fileReader.hasMoreLines() ) {
			citiesFound |= processCity( fileReader.readLine() );
    }
  }
// end new code

	//check for local cities database, but don't require it.
	file.setName( locate( "appdata", "mycities.dat" ) ); //determine filename in local user KDE directory tree.
	if ( file.exists() && file.open( QIODevice::ReadOnly ) ) {
		QTextStream stream( &file );

  	while ( !stream.atEnd() ) {
			QString line = stream.readLine();
			citiesFound |= processCity( line );
		}	// while ( !stream.atEnd() )
		file.close();
	}	// if ( fileopen() )

	return citiesFound;
}

bool KStarsData::processCity( QString& line ) {
	QString totalLine;
	QString name, province, country;
	QStringList fields;
	TimeZoneRule *TZrule;
	bool intCheck = true;
	QChar latsgn, lngsgn;
	int lngD, lngM, lngS;
	int latD, latM, latS;
	double TZ;
	float lng, lat;

	totalLine = line;

	// separate fields
	fields = QStringList::split( ":", line );

	for ( int i=0; i< fields.size(); ++i )
		fields[i] = fields[i].trimmed();

	if ( fields.size() < 11 ) {
		kdDebug()<< i18n( "Cities.dat: Ran out of fields.  Line was:" ) <<endl;
		kdDebug()<< totalLine.local8Bit() <<endl;
		return false;
	} else if ( fields.size() < 12 ) {
		// allow old format (without TZ) for mycities.dat
		fields.append("");
		fields.append("--");
	} else if ( fields.size() < 13 ) {
		// Set TZrule to "--"
		fields.append("--");
	}

	name = fields[0];
	province = fields[1];
	country = fields[2];

	latD = fields[3].toInt( &intCheck );
	if ( !intCheck ) {
		kdDebug() << fields[3] << i18n( "\nCities.dat: Bad integer.  Line was:\n" ) << totalLine << endl;
		return false;
	}

	latM = fields[4].toInt( &intCheck );
	if ( !intCheck ) {
		kdDebug() << fields[4] << i18n( "\nCities.dat: Bad integer.  Line was:\n" ) << totalLine << endl;
		return false;
	}

	latS = fields[5].toInt( &intCheck );
	if ( !intCheck ) {
		kdDebug() << fields[5] << i18n( "\nCities.dat: Bad integer.  Line was:\n" ) << totalLine << endl;
		return false;
	}

	QChar ctemp = fields[6].at(0);
	latsgn = ctemp;
	if (latsgn != 'N' && latsgn != 'S') {
		kdDebug() << latsgn << i18n( "\nCities.dat: Invalid latitude sign.  Line was:\n" ) << totalLine << endl;
		return false;
	}

	lngD = fields[7].toInt( &intCheck );
	if ( !intCheck ) {
		kdDebug() << fields[7] << i18n( "\nCities.dat: Bad integer.  Line was:\n" ) << totalLine << endl;
		return false;
	}

	lngM = fields[8].toInt( &intCheck );
	if ( !intCheck ) {
		kdDebug() << fields[8] << i18n( "\nCities.dat: Bad integer.  Line was:\n" ) << totalLine << endl;
		return false;
	}

	lngS = fields[9].toInt( &intCheck );
	if ( !intCheck ) {
		kdDebug() << fields[9] << i18n( "\nCities.dat: Bad integer.  Line was:\n" ) << totalLine << endl;
		return false;
	}

	ctemp = fields[10].at(0);
	lngsgn = ctemp;
	if (lngsgn != 'E' && lngsgn != 'W') {
		kdDebug() << latsgn << i18n( "\nCities.dat: Invalid longitude sign.  Line was:\n" ) << totalLine << endl;
		return false;
	}

	lat = (float)latD + ((float)latM + (float)latS/60.0)/60.0;
	lng = (float)lngD + ((float)lngM + (float)lngS/60.0)/60.0;

	if ( latsgn == 'S' ) lat *= -1.0;
	if ( lngsgn == 'W' ) lng *= -1.0;

	// find time zone. Use value from Cities.dat if available.
	// otherwise use the old approximation: int(lng/15.0);
	if ( fields[11].isEmpty() || ('x' == fields[11].at(0)) ) {
		TZ = int(lng/15.0);
	} else {
		bool doubleCheck = true;
		TZ = fields[11].toDouble( &doubleCheck);
		if ( !doubleCheck ) {
			kdDebug() << fields[11] << i18n( "\nCities.dat: Bad time zone.  Line was:\n" ) << totalLine << endl;
			return false;
		}
	}

	//last field is the TimeZone Rule ID.
	TZrule = &( Rulebook[ fields[12] ] );

//	if ( fields[12]=="--" )
//		kdDebug() << "Empty rule start month: " << TZrule->StartMonth << endl;
	geoList.append ( new GeoLocation( lng, lat, name, province, country, TZ, TZrule ));  // appends city names to list
	return true;
}

bool KStarsData::readTimeZoneRulebook( void ) {
	QFile file;
	QString id;

	if ( KSUtils::openDataFile( file, "TZrules.dat" ) ) {
		QTextStream stream( &file );

		while ( !stream.atEnd() ) {
			QString line = stream.readLine().trimmed();
			if ( line.left(1) != "#" && line.length() ) { //ignore commented and blank lines
				QStringList fields = QStringList::split( " ", line );
				id = fields[0];
				QTime stime = QTime( fields[3].left( fields[3].find(':')).toInt() ,
								fields[3].mid( fields[3].find(':')+1, fields[3].length()).toInt() );
				QTime rtime = QTime( fields[6].left( fields[6].find(':')).toInt(),
								fields[6].mid( fields[6].find(':')+1, fields[6].length()).toInt() );

				Rulebook[ id ] = TimeZoneRule( fields[1], fields[2], stime, fields[4], fields[5], rtime );
			}
		}
		return true;
	} else {
		return false;
	}
}

bool KStarsData::openURLFile(QString urlfile, QFile & file) {
	//QFile file;
	QString localFile;
	bool fileFound = false;
	QFile localeFile;

	if ( locale->language() != "en_US" )
		localFile = locale->language() + "/" + urlfile;

	if ( ! localFile.isEmpty() && KSUtils::openDataFile( file, localFile ) ) {
		fileFound = true;
	} else {
   // Try to load locale file, if not successful, load regular urlfile and then copy it to locale.
		file.setName( locateLocal( "appdata", urlfile ) );
		if ( file.open( QIODevice::ReadOnly ) ) {
			//local file found.  Now, if global file has newer timestamp, then merge the two files.
			//First load local file into QStringList
			bool newDataFound( false );
			QStringList urlData;
			QTextStream lStream( &file );
			while ( ! lStream.atEnd() ) urlData.append( lStream.readLine() );

			//Find global file(s) in findAllResources() list.
			QFileInfo fi_local( file.name() );
			QStringList flist = KGlobal::instance()->dirs()->findAllResources( "appdata", urlfile );
			for ( int i=0; i< flist.size(); i++ ) {
				if ( flist[i] != file.name() ) {
					QFileInfo fi_global( flist[i] );

					//Is this global file newer than the local file?
					if ( fi_global.lastModified() > fi_local.lastModified() ) {
						//Global file has newer timestamp than local.  Add lines in global file that don't already exist in local file.
						//be smart about this; in some cases the URL is updated but the image is probably the same if its
						//label string is the same.  So only check strings up to last ":"
						QFile globalFile( flist[i] );
						if ( globalFile.open( QIODevice::ReadOnly ) ) {
							QTextStream gStream( &globalFile );
							while ( ! gStream.atEnd() ) {
								QString line = gStream.readLine();

								//If global-file line begins with "XXX:" then this line should be removed from the local file.
								if ( line.left( 4 ) == "XXX:"  && urlData.contains( line.mid( 4 ) ) ) {
									urlData.remove( urlData.find( line.mid( 4 ) ) );
								} else {
									//does local file contain the current global file line, up to second ':' ?

									bool linefound( false );
									for ( int j=0; j< urlData.size(); ++j ) {
										if ( urlData[j].contains( line.left( line.find( ':', line.find( ':' ) + 1 ) ) ) ) {
											//replace line in urlData with its equivalent in the newer global file.
											urlData.replace( j, line );
											if ( !newDataFound ) newDataFound = true;
											linefound = true;
											break;
										}
									}
									if ( ! linefound ) {
										urlData.append( line );
										if ( !newDataFound ) newDataFound = true;
									}
								}
							}
						}
					}
				}
			}

			file.close();

			//(possibly) write appended local file
			if ( newDataFound ) {
				if ( file.open( QIODevice::WriteOnly ) ) {
					QTextStream outStream( &file );
					for ( int i=0; i<urlData.size(); i++ ) {
						outStream << urlData[i] << endl;
					}
					file.close();
				}
			}

			if ( file.open( QIODevice::ReadOnly ) ) fileFound = true;

		} else {
			if ( KSUtils::openDataFile( file, urlfile ) ) {
				if ( locale->language() != "en_US" ) kdDebug() << i18n( "No localized URL file; using default English file." ) << endl;
				// we found urlfile, we need to copy it to locale
				localeFile.setName( locateLocal( "appdata", urlfile ) );
				if (localeFile.open(QIODevice::WriteOnly)) {
					QTextStream readStream(&file);
					QTextStream writeStream(&localeFile);
					while ( ! readStream.atEnd() ) {
						QString line = readStream.readLine();
						if ( line.left( 4 ) != "XXX:" ) //do not write "deleted" lines
							writeStream << line << endl;
					}

					localeFile.close();
					file.reset();
				} else {
					kdDebug() << i18n( "Failed to copy default URL file to locale folder, modifying default object links is not possible" ) << endl;
				}
				fileFound = true;
			}
		}
	}
	return fileFound;
}

bool KStarsData::readURLData( QString urlfile, int type, bool deepOnly ) {
	QFile file;
	if (!openURLFile(urlfile, file)) return false;

	QTextStream stream(&file);

	while ( !stream.atEnd() ) {
		QString line = stream.readLine();

		//ignore comment lines
		if ( line.left(1) != "#" ) {
			QString name = line.mid( 0, line.find(':') );
			QString sub = line.mid( line.find(':')+1 );
			QString title = sub.mid( 0, sub.find(':') );
			QString url = sub.mid( sub.find(':')+1 );
	
			SkyObject *o = skyComposite()->findByName(name);
			
			if ( !o ) {
				kdWarning() << k_funcinfo << name << " not found" << endl;
			} else {
				if ( ! deepOnly || ( o->type() > 2 && o->type() < 9 ) ) {
					if ( type==0 ) { //image URL
						o->ImageList.append( url );
						o->ImageTitle.append( title );
					} else if ( type==1 ) { //info URL
						o->InfoList.append( url );
						o->InfoTitle.append( title );
					}
				}
			}
		}
	}
	file.close();
	return true;
}

bool KStarsData::readUserLog(void)
{
	QFile file;
	QString buffer;
	QString sub, name, data;

	if (!KSUtils::openDataFile( file, "userlog.dat" )) return false;

	QTextStream stream(&file);

	if (!stream.atEnd()) buffer = stream.read();

	while (!buffer.isEmpty()) {
		int startIndex, endIndex;

		startIndex = buffer.find("[KSLABEL:");
		sub = buffer.mid(startIndex);
		endIndex = sub.find("[KSLogEnd]");

		// Read name after KSLABEL identifer
		name = sub.mid(startIndex + 9, sub.find("]") - (startIndex + 9));
		// Read data and skip new line
		data   = sub.mid(sub.find("]") + 2, endIndex - (sub.find("]") + 2));
		buffer = buffer.mid(endIndex + 11);

		//Find the sky object named 'name'.
		//Note that ObjectNameList::find() looks for the ascii representation 
		//of star genetive names, so stars are identified that way in the user log.
		SkyObject *o = skyComposite()->findByName(name);
		if ( !o ) {
			kdWarning() << k_funcinfo << name << " not found" << endl;
		} else {
			o->userLog = data;
		}

	} // end while
	file.close();
	return true;
}

bool KStarsData::readADVTreeData(void)
{
	QFile file;
	QString Interface;
	
	if (!KSUtils::openDataFile(file, "advinterface.dat"))
		return false;
	
	QTextStream stream(&file);
	QString Line;
	
	while  (!stream.atEnd())
	{
		QString Name, Link, subName;
		int Type, interfaceIndex;
	
		Line = stream.readLine();
	
		if (Line.startsWith("[KSLABEL]"))
		{
				Name = Line.mid(9);
				Type = 0;
		}
		else if (Line.startsWith("[END]"))
				Type = 1;
		else if (Line.startsWith("[KSINTERFACE]"))
		{
			Interface = Line.mid(13);
			continue;
		}
	
		else
		{
			Name = Line.mid(0, Line.find(":"));
			Link = Line.mid(Line.find(":") + 1);
	
			// Link is empty, using Interface instead
			if (Link.isEmpty())
			{
					Link = Interface;
					subName = Name;
					interfaceIndex = Link.find("KSINTERFACE");
					Link.remove(interfaceIndex, 11);
					Link = Link.insert(interfaceIndex, subName.replace( QRegExp(" "), "+"));
	
				}
	
			Type = 2;
		}
	
		ADVTreeData *ADVData = new ADVTreeData;
	
		ADVData->Name = Name;
		ADVData->Link = Link;
		ADVData->Type = Type;
	
		ADVtreeList.append(ADVData);
	}

	return true;
}

bool KStarsData::readVARData(void)
{
	QString varFile("valaav.txt");
	QFile localeFile;
	QFile file;
	
	file.setName( locateLocal( "appdata", varFile ) );
	if ( !file.open( QIODevice::ReadOnly ) )
	{
		// Open default variable stars file
		if ( KSUtils::openDataFile( file, varFile ) )
		{
			// we found urlfile, we need to copy it to locale
			localeFile.setName( locateLocal( "appdata", varFile ) );

			if (localeFile.open(QIODevice::WriteOnly))
			{
					QTextStream readStream(&file);
					QTextStream writeStream(&localeFile);
					writeStream <<  readStream.read();
					localeFile.close();
					file.reset();
			}
		}
		else
			return false;
	}


	QTextStream stream(&file);
	stream.readLine();
	
	while  (!stream.atEnd())
	{
		QString Name;
		QString Designation;
		QString Line;
	
		Line = stream.readLine();
	
		if (Line[0] == QChar('*'))
			break;
	
		Designation = Line.mid(0,8).trimmed();
		Name          = Line.mid(10,20).simplified();
	
		VariableStarInfo *VInfo = new VariableStarInfo;
	
		VInfo->Designation = Designation;
		VInfo->Name          = Name;
		VariableStarsList.append(VInfo);
	}

	return true;
}


bool KStarsData::readINDIHosts(void)
{
	QString indiFile("indihosts.xml");
	QFile localeFile;
	QFile file;
	char errmsg[1024];
	signed char c;
	LilXML *xmlParser = newLilXML();
	XMLEle *root = NULL;
	XMLAtt *ap;
	
	file.setName( locate( "appdata", indiFile ) );
	if ( file.name().isEmpty() || !file.open( QIODevice::ReadOnly ) )
		 return false;

	while ( (c = (signed char) file.getch()) != -1)
	{
		root = readXMLEle(xmlParser, c, errmsg);
	
		if (root)
		{
			// Get host name
			ap = findXMLAtt(root, "name");
			if (!ap) {
				delLilXML(xmlParser);
				return false;
			}

			INDIHostsInfo *VInfo = new INDIHostsInfo;
			VInfo->name = QString(valuXMLAtt(ap));
	
			// Get host name
			ap = findXMLAtt(root, "hostname");
	
			if (!ap) {
				delLilXML(xmlParser);
				return false;
			}
	
			VInfo->hostname = QString(valuXMLAtt(ap));
			ap = findXMLAtt(root, "port");
	
			if (!ap) {
				delLilXML(xmlParser);
				return false;
			}
	
			VInfo->portnumber = QString(valuXMLAtt(ap));
			VInfo->isConnected = false;
			VInfo->mgrID = -1;
			INDIHostsList.append(VInfo);
	
			delXMLEle(root);
		}
		else if (errmsg[0])
			kdDebug() << errmsg << endl;
	}

	delLilXML(xmlParser);

	return true;
}

//FIXME: There's way too much code duplication here!
//We should just execute the script through KProcess.  
//One complication is that we want to ignore some script commands 
//that don't make sense for dump mode.  However, it would be better 
//for the DCOP fcns themselves to detect whether a SkyMap object exists, 
//and act accordingly.
//
//"pseudo-execute" a shell script, ignoring all interactive aspects.  Just use
//the portions of the script that change the state of the program.  This is only
//used for image-dump mode, where the GUI is not running.  So, some things (such as
//waitForKey()) don't make sense and should be ignored.
//also, even functions that do make sense in this context have aspects that should
//be modified or ignored.  For example, we don't need to call slotCenter() on recentering
//commands, just setDestination().  (sltoCenter() does additional things that we dont need).
bool KStarsData::executeScript( const QString &scriptname, SkyMap *map ) {
	int cmdCount(0);

	QFile f( scriptname );
	if ( !f.open( QIODevice::ReadOnly) ) {
		kdDebug() << i18n( "Could not open file %1" ).arg( f.name() ) << endl;
		return false;
	}

	QTextStream istream(&f);
	while ( ! istream.atEnd() ) {
		QString line = istream.readLine();

		//found a dcop line
		if ( line.left(4) == "dcop" ) {
			line = line.mid( 20 );  //strip away leading characters
			QStringList fn = QStringList::split( " ", line );

			if ( fn[0] == "lookTowards" && fn.size() >= 2 ) {
				double az(-1.0);
				QString arg = fn[1].lower();
				if ( arg == "n"  || arg == "north" )     az =   0.0;
				if ( arg == "ne" || arg == "northeast" ) az =  45.0;
				if ( arg == "e"  || arg == "east" )      az =  90.0;
				if ( arg == "se" || arg == "southeast" ) az = 135.0;
				if ( arg == "s"  || arg == "south" )     az = 180.0;
				if ( arg == "sw" || arg == "southwest" ) az = 225.0;
				if ( arg == "w"  || arg == "west" )      az = 270.0;
				if ( arg == "nw" || arg == "northwest" ) az = 335.0;
				if ( az >= 0.0 ) { 
					map->setFocusAltAz( 90.0, map->focus()->az()->Degrees() );
					cmdCount++;
					map->setDestination( map->focus() );
				}

				if ( arg == "z" || arg == "zenith" ) {
					map->setFocusAltAz( 90.0, map->focus()->az()->Degrees() );
					cmdCount++;
					map->setDestination( map->focus() );
				}

				//try a named object.  name is everything after the first word (which is 'lookTowards')
				fn.remove( fn.first() );
				SkyObject *target = objectNamed( fn.join( " " ) );
				if ( target ) { map->setFocus( target ); cmdCount++; }

			} else if ( fn[0] == "setRaDec" && fn.size() == 3 ) {
				bool ok( false );
				dms r(0.0), d(0.0);

				ok = r.setFromString( fn[1], false ); //assume angle in hours
				if ( ok ) ok = d.setFromString( fn[2], true );  //assume angle in degrees
				if ( ok ) {
					map->setFocus( r, d );
					cmdCount++;
				}

			} else if ( fn[0] == "setAltAz" && fn.size() == 3 ) {
				bool ok( false );
				dms az(0.0), alt(0.0);

				ok = alt.setFromString( fn[1] );
				if ( ok ) ok = az.setFromString( fn[2] );
				if ( ok ) {
					map->setFocusAltAz( alt, az );
					cmdCount++;
				}

			} else if ( fn[0] == "zoom" && fn.size() == 2 ) {
				bool ok(false);
				double z = fn[1].toDouble(&ok);
				if ( ok ) {
					if ( z > MAXZOOM ) z = MAXZOOM;
					if ( z < MINZOOM ) z = MINZOOM;
					Options::setZoomFactor( z );
					cmdCount++;
				}

			} else if ( fn[0] == "zoomIn" ) {
				if ( Options::zoomFactor() < MAXZOOM ) {
					Options::setZoomFactor( Options::zoomFactor() * DZOOM );
					cmdCount++;
				}
			} else if ( fn[0] == "zoomOut" ) {
				if ( Options::zoomFactor() > MINZOOM ) {
					Options::setZoomFactor( Options::zoomFactor() / DZOOM );
					cmdCount++;
				}
			} else if ( fn[0] == "defaultZoom" ) {
				Options::setZoomFactor( DEFAULTZOOM );
				cmdCount++;
			} else if ( fn[0] == "setLocalTime" && fn.size() == 7 ) {
				bool ok(false);
				int yr(0), mth(0), day(0) ,hr(0), min(0), sec(0);
				yr = fn[1].toInt(&ok);
				if ( ok ) mth = fn[2].toInt(&ok);
				if ( ok ) day = fn[3].toInt(&ok);
				if ( ok ) hr  = fn[4].toInt(&ok);
				if ( ok ) min = fn[5].toInt(&ok);
				if ( ok ) sec = fn[6].toInt(&ok);
				if ( ok ) {
					changeDateTime( geo()->LTtoUT( KStarsDateTime( ExtDate(yr, mth, day), QTime(hr,min,sec) ) ) );
					cmdCount++;
				} else {
					kdWarning() << i18n( "Could not set time: %1 / %2 / %3 ; %4:%5:%6" )
						.arg(day).arg(mth).arg(yr).arg(hr).arg(min).arg(sec) << endl;
				}
			} else if ( fn[0] == "changeViewOption" && fn.size() == 3 ) {
				bool bOk(false), nOk(false), dOk(false);

				//parse bool value
				bool bVal(false);
				if ( fn[2].lower() == "true" ) { bOk = true; bVal = true; }
				if ( fn[2].lower() == "false" ) { bOk = true; bVal = false; }
				if ( fn[2] == "1" ) { bOk = true; bVal = true; }
				if ( fn[2] == "0" ) { bOk = true; bVal = false; }

				//parse int value
				int nVal = fn[2].toInt( &nOk );

				//parse double value
				double dVal = fn[2].toDouble( &dOk );

				if ( fn[1] == "FOVName"                ) { Options::setFOVName(       fn[2] ); cmdCount++; }
				if ( fn[1] == "FOVSize"         && dOk ) { Options::setFOVSize( (float)dVal ); cmdCount++; }
				if ( fn[1] == "FOVShape"        && nOk ) { Options::setFOVShape(       nVal ); cmdCount++; }
				if ( fn[1] == "FOVColor"               ) { Options::setFOVColor(      fn[2] ); cmdCount++; }
				if ( fn[1] == "ShowStars"         && bOk ) { Options::setShowStars(    bVal ); cmdCount++; }
				if ( fn[1] == "ShowMessier"        && bOk ) { Options::setShowMessier( bVal ); cmdCount++; }
				if ( fn[1] == "ShowMessierImages"  && bOk ) { Options::setShowMessierImages( bVal ); cmdCount++; }
				if ( fn[1] == "ShowCLines"      && bOk ) { Options::setShowCLines(   bVal ); cmdCount++; }
				if ( fn[1] == "ShowCNames"      && bOk ) { Options::setShowCNames(   bVal ); cmdCount++; }
				if ( fn[1] == "ShowNGC"         && bOk ) { Options::setShowNGC(      bVal ); cmdCount++; }
				if ( fn[1] == "ShowIC"          && bOk ) { Options::setShowIC(       bVal ); cmdCount++; }
				if ( fn[1] == "ShowMilkyWay"    && bOk ) { Options::setShowMilkyWay( bVal ); cmdCount++; }
				if ( fn[1] == "ShowGrid"        && bOk ) { Options::setShowGrid(     bVal ); cmdCount++; }
				if ( fn[1] == "ShowEquator"     && bOk ) { Options::setShowEquator(  bVal ); cmdCount++; }
				if ( fn[1] == "ShowEcliptic"    && bOk ) { Options::setShowEcliptic( bVal ); cmdCount++; }
				if ( fn[1] == "ShowHorizon"     && bOk ) { Options::setShowHorizon(  bVal ); cmdCount++; }
				if ( fn[1] == "ShowGround"      && bOk ) { Options::setShowGround(   bVal ); cmdCount++; }
				if ( fn[1] == "ShowSun"         && bOk ) { Options::setShowSun(      bVal ); cmdCount++; }
				if ( fn[1] == "ShowMoon"        && bOk ) { Options::setShowMoon(     bVal ); cmdCount++; }
				if ( fn[1] == "ShowMercury"     && bOk ) { Options::setShowMercury(  bVal ); cmdCount++; }
				if ( fn[1] == "ShowVenus"       && bOk ) { Options::setShowVenus(    bVal ); cmdCount++; }
				if ( fn[1] == "ShowMars"        && bOk ) { Options::setShowMars(     bVal ); cmdCount++; }
				if ( fn[1] == "ShowJupiter"     && bOk ) { Options::setShowJupiter(  bVal ); cmdCount++; }
				if ( fn[1] == "ShowSaturn"      && bOk ) { Options::setShowSaturn(   bVal ); cmdCount++; }
				if ( fn[1] == "ShowUranus"      && bOk ) { Options::setShowUranus(   bVal ); cmdCount++; }
				if ( fn[1] == "ShowNeptune"     && bOk ) { Options::setShowNeptune(  bVal ); cmdCount++; }
				if ( fn[1] == "ShowPluto"       && bOk ) { Options::setShowPluto(    bVal ); cmdCount++; }
				if ( fn[1] == "ShowAsteroids"   && bOk ) { Options::setShowAsteroids( bVal ); cmdCount++; }
				if ( fn[1] == "ShowComets"      && bOk ) { Options::setShowComets(   bVal ); cmdCount++; }
				if ( fn[1] == "ShowPlanets"     && bOk ) { Options::setShowPlanets(  bVal ); cmdCount++; }
				if ( fn[1] == "ShowDeepSky"     && bOk ) { Options::setShowDeepSky(  bVal ); cmdCount++; }
				if ( fn[1] == "ShowStarNames"      && bOk ) { Options::setShowStarNames(      bVal ); cmdCount++; }
				if ( fn[1] == "ShowStarMagnitudes" && bOk ) { Options::setShowStarMagnitudes( bVal ); cmdCount++; }
				if ( fn[1] == "ShowAsteroidNames"  && bOk ) { Options::setShowAsteroidNames(  bVal ); cmdCount++; }
				if ( fn[1] == "ShowCometNames"     && bOk ) { Options::setShowCometNames(     bVal ); cmdCount++; }
				if ( fn[1] == "ShowPlanetNames"    && bOk ) { Options::setShowPlanetNames(    bVal ); cmdCount++; }
				if ( fn[1] == "ShowPlanetImages"   && bOk ) { Options::setShowPlanetImages(   bVal ); cmdCount++; }

				if ( fn[1] == "UseAltAz"         && bOk ) { Options::setUseAltAz(      bVal ); cmdCount++; }
				if ( fn[1] == "UseRefraction"    && bOk ) { Options::setUseRefraction( bVal ); cmdCount++; }
				if ( fn[1] == "UseAutoLabel"     && bOk ) { Options::setUseAutoLabel(  bVal ); cmdCount++; }
				if ( fn[1] == "UseAutoTrail"     && bOk ) { Options::setUseAutoTrail(  bVal ); cmdCount++; }
				if ( fn[1] == "UseAnimatedSlewing"   && bOk ) { Options::setUseAnimatedSlewing( bVal ); cmdCount++; }
				if ( fn[1] == "FadePlanetTrails" && bOk ) { Options::setFadePlanetTrails( bVal ); cmdCount++; }
				if ( fn[1] == "SlewTimeScale"    && dOk ) { Options::setSlewTimeScale(    dVal ); cmdCount++; }
				if ( fn[1] == "ZoomFactor"       && dOk ) { Options::setZoomFactor(       dVal ); cmdCount++; }
				if ( fn[1] == "MagLimitDrawStar"     && dOk ) { Options::setMagLimitDrawStar( dVal ); cmdCount++; }
				if ( fn[1] == "MagLimitDrawStarZoomOut" && dOk ) { Options::setMagLimitDrawStarZoomOut( dVal ); cmdCount++; }
				if ( fn[1] == "MagLimitDrawDeepSky"     && dOk ) { Options::setMagLimitDrawDeepSky( dVal ); cmdCount++; }
				if ( fn[1] == "MagLimitDrawDeepSkyZoomOut" && dOk ) { Options::setMagLimitDrawDeepSkyZoomOut( dVal ); cmdCount++; }
				if ( fn[1] == "MagLimitDrawStarInfo" && dOk ) { Options::setMagLimitDrawStarInfo( dVal ); cmdCount++; }
				if ( fn[1] == "MagLimitHideStar"     && dOk ) { Options::setMagLimitHideStar(     dVal ); cmdCount++; }
				if ( fn[1] == "MagLimitAsteroid"     && dOk ) { Options::setMagLimitAsteroid(     dVal ); cmdCount++; }
				if ( fn[1] == "MagLimitAsteroidName" && dOk ) { Options::setMagLimitAsteroidName( dVal ); cmdCount++; }
				if ( fn[1] == "MaxRadCometName"      && dOk ) { Options::setMaxRadCometName(      dVal ); cmdCount++; }

				//these three are a "radio group"
				if ( fn[1] == "UseLatinConstellationNames" && bOk ) {
					Options::setUseLatinConstellNames( true );
					Options::setUseLocalConstellNames( false );
					Options::setUseAbbrevConstellNames( false );
					cmdCount++;
				}
				if ( fn[1] == "UseLocalConstellationNames" && bOk ) {
					Options::setUseLatinConstellNames( false );
					Options::setUseLocalConstellNames( true );
					Options::setUseAbbrevConstellNames( false );
					cmdCount++;
				}
				if ( fn[1] == "UseAbbrevConstellationNames" && bOk ) {
					Options::setUseLatinConstellNames( false );
					Options::setUseLocalConstellNames( false );
					Options::setUseAbbrevConstellNames( true );
					cmdCount++;
				}
			} else if ( fn[0] == "setGeoLocation" && ( fn.size() == 3 || fn.size() == 4 ) ) {
				QString city( fn[1] ), province( "" ), country( fn[2] );
				if ( fn.size() == 4 ) {
					province = fn[2];
					country = fn[3];
				}

				bool cityFound( false );
				foreach ( GeoLocation *loc, geoList ) {
					if ( loc->translatedName() == city &&
								( province.isEmpty() || loc->translatedProvince() == province ) &&
								loc->translatedCountry() == country ) {

						cityFound = true;
						setLocation( *loc );
						cmdCount++;
						break;
					}
				}

				if ( !cityFound )
					kdWarning() << i18n( "Could not set location named %1, %2, %3" ).arg(city).arg(province).arg(country) << endl;
			}
		}
	}  //end while

	if ( cmdCount ) return true;
	return false;
}

void KStarsData::appendTelescopeObject(SkyObject * object)
{
  INDITelescopeList.append(object);
}

void KStarsData::saveTimeBoxShaded( bool b ) { Options::setShadeTimeBox( b ); }
void KStarsData::saveGeoBoxShaded( bool b ) { Options::setShadeGeoBox( b ); }
void KStarsData::saveFocusBoxShaded( bool b ) { Options::setShadeFocusBox( b ); }
void KStarsData::saveTimeBoxPos( QPoint p ) { Options::setPositionTimeBox( p ); }
void KStarsData::saveGeoBoxPos( QPoint p ) { Options::setPositionGeoBox( p ); }
void KStarsData::saveFocusBoxPos( QPoint p ) { Options::setPositionFocusBox( p ); }

#include "kstarsdata.moc"
