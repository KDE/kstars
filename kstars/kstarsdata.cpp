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

#include "kstarsdata.h"

#include <QApplication>
#include <QRegExp>
#include <QDir>
#include <QFile>
#include <QTextStream>

#include <kcomponentdata.h>
#include <kmessagebox.h>
#include <kdebug.h>
#include <klocale.h>
#include <kstandarddirs.h>

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
#include "indi/libs/lilxml.h"
#include "indistd.h"
#include "detaildialog.h"

//Initialize static members
QList<GeoLocation*> KStarsData::geoList = QList<GeoLocation*>();

QMap<QString, TimeZoneRule> KStarsData::Rulebook = QMap<QString, TimeZoneRule>();

int KStarsData::objects = 0;

KStarsData::KStarsData() : locale(0),
		LST(0), HourAngle(0), initTimer(0)
{
	startupComplete = false;
	objects++;

	TypeName[0] = i18n( "star" );
	TypeName[1] = i18n( "multiple star" );
	TypeName[2] = i18n( "planet" );
	TypeName[3] = i18n( "open cluster" );
	TypeName[4] = i18n( "globular cluster" );
	TypeName[5] = i18n( "gaseous nebula" );
	TypeName[6] = i18n( "planetary nebula" );
	TypeName[7] = i18n( "supernova remnant" );
	TypeName[8] = i18n( "galaxy" );
	TypeName[9] = i18n( "comet" );
	TypeName[10] = i18n( "asteroid" );
	TypeName[11] = i18n( "constellation" );

	//standard directories and locale objects
	locale = new KLocale( "kstars" );

	//Check to see if config file already exists.  If not, set
	//useDefaultOptions = true
	QString fname = KStandardDirs::locateLocal( "config", "kstarsrc" );
	useDefaultOptions = ! ( QFile(fname).exists() );

	m_SkyComposite = new SkyMapComposite( 0, this );

	//Instantiate LST and HourAngle
	LST = new dms();
	HourAngle = new dms();

	//initialize FOV symbol
	fovSymbol = FOV();

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
	delete locale;
	delete initTimer;
	delete LST;
	delete HourAngle;

	while ( ! geoList.isEmpty() )
		delete geoList.takeFirst();

	while ( !VariableStarsList.isEmpty())
		delete VariableStarsList.takeFirst();

	while ( !INDIHostsList.isEmpty())
		delete INDIHostsList.takeFirst();
	
	while ( !ADVtreeList.isEmpty())
		delete ADVtreeList.takeFirst();

}

QString KStarsData::typeName( int i ) {
  QString result = i18n( "no type" );
  if ( i >= 0 && i < 12 ) result = TypeName[i];

  return result;
}

void KStarsData::initialize() {
	if (startupComplete) return;

	initTimer = new QTimer;
	QObject::connect(initTimer, SIGNAL(timeout()), this, SLOT( slotInitialize() ) );
	initCounter = 0;
	initTimer->start(1);
}

void KStarsData::initError(const QString &s, bool required = false) {
	QString message, caption;
	initTimer->stop();
	if (required) {
		message = i18n( "The file %1 could not be found. "
				"KStars cannot run properly without this file. "
				"To continue loading, place the file in one of the "
				"following locations, then press Retry:\n\n", s )
				+ QString( "\t$(KDEDIR)/share/apps/kstars/%1\n" ).arg( s )
				+ QString( "\t~/.kde/share/apps/kstars/%1\n\n" ).arg( s )
				+ i18n( "Otherwise, press Cancel to shutdown." );
		caption = i18n( "Critical File Not Found: %1", s );
	} else {
		message = i18n( "The file %1 could not be found. "
				"KStars can still run without this file. "
				"However, to avoid seeing this message in the future, you can "
				"place the file in one of the following locations, then press Retry:\n\n", s )
				+ QString( "\t$(KDEDIR)/share/apps/kstars/%1\n" ).arg( s )
				+ QString( "\t~/.kde/share/apps/kstars/%1\n\n" ).arg( s )
				+ i18n( "Otherwise, press Cancel to continue loading without this file.");
		caption = i18n( "Non-Critical File Not Found: %1", s );
	}

	if ( KMessageBox::warningContinueCancel( 0, message, caption, KGuiItem( i18n( "Retry" ) ) ) == KMessageBox::Continue ) {
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

	qApp->flush(); // flush all paint events before loading data

	switch ( initCounter )
	{
		case 0: //Load Time Zone Rules//
			emit progressText( i18n("Reading time zone rules") );

			if (objects==1) {
				// timezone rules
				if ( !readTimeZoneRulebook( ) )
					initError( "TZrules.dat", true );
			}


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

			emit progressText(i18n("Loading sky objects" ) );
			skyComposite()->init( this );
			break;

		case 3: //Load Image URLs//

			emit progressText( i18n("Loading Image URLs" ) );
			if ( !readURLData( "image_url.dat", 0 ) ) {
				initError( "image_url.dat", false );
			}



			break;

		case 4: //Load Information URLs//

			emit progressText( i18n("Loading Information URLs" ) );
			if ( !readURLData( "info_url.dat", 1 ) ) {
				initError( "info_url.dat", false );
			}

			break;

		case 5:
			emit progressText( i18n("Loading Variable Stars" ) );
			readINDIHosts();
			readUserLog();
			readVARData();
			readADVTreeData();
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

	//TIMING
	QTime t;

	if ( fabs( ut().djd() - LastNumUpdate.djd() ) > 1.0 ) {
		LastNumUpdate = ut().djd();
		//TIMING
		//		t.start();

		skyComposite()->update( this, &num );

		//TIMING
		//		kDebug() << QString("SkyMapComposite::update() took %1 ms").arg(t.elapsed());
	}

	if ( fabs( ut().djd() - LastPlanetUpdate.djd() ) > 0.01 ) {
		LastPlanetUpdate = ut().djd();
		//TIMING
		//		t.start();

		skyComposite()->updatePlanets( this, &num );

		//TIMING
		//		kDebug() << QString("SkyMapComposite::updatePlanets() took %1 ms").arg(t.elapsed());
	}

	// Moon moves ~30 arcmin/hr, so update its position every minute.
	if ( fabs( ut().djd() - LastMoonUpdate.djd() ) > 0.00069444 ) {
		LastMoonUpdate = ut();
		//TIMING
		//		t.start();

		skyComposite()->updateMoons( this, &num );

		//TIMING
		//		kDebug() << QString("SkyMapComposite::updateMoons() took %1 ms").arg(t.elapsed());
	}

	//Update Alt/Az coordinates.  Timescale varies with zoom level
	//If Clock is in Manual Mode, always update. (?)
	if ( fabs( ut().djd() - LastSkyUpdate.djd() ) > 0.25/Options::zoomFactor() || clock()->isManualMode() ) {
		LastSkyUpdate = ut();
		//TIMING
		//		t.start();

		skyComposite()->update( this ); //omit KSNumbers arg == just update Alt/Az coords

		//Update focus
		skymap->updateFocus();

		//TIMING
		//		kDebug() << QString("SkyMapComposite::update() for Alt/Az took %1 ms").arg(t.elapsed());

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

	//DEBUG
	kDebug() << "setting DateTime :" << newDate.toString() << ":";

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
	setLocation( GeoLocation ( Options::longitude(), Options::latitude(),
			Options::cityName(), Options::provinceName(), Options::countryName(),
			Options::timeZone(), &(Rulebook[ Options::dST() ]), 4, Options::elevation() ) );
}

void KStarsData::setLocation( const GeoLocation &l ) {
 	Geo = GeoLocation(l);
	if ( Geo.lat()->Degrees() >= 90.0 ) Geo.setLat( 89.99 );
	if ( Geo.lat()->Degrees() <= -90.0 ) Geo.setLat( -89.99 );

	//store data in the Options objects
	Options::setCityName( Geo.name() );
	Options::setProvinceName( Geo.province() );
	Options::setCountryName( Geo.country() );
	Options::setTimeZone( Geo.TZ0() );
	Options::setElevation( Geo.height() );
	Options::setLongitude( Geo.lng()->Degrees() );
	Options::setLatitude( Geo.lat()->Degrees() );
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
	file.setFileName( KStandardDirs::locate( "appdata", "mycities.dat" ) ); //determine filename in local user KDE directory tree.
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

bool KStarsData::processCity( const QString& line ) {
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
	fields = line.split( ":" );

	for ( int i=0; i< fields.size(); ++i )
		fields[i] = fields[i].trimmed();

	if ( fields.size() < 11 ) {
		kDebug()<< i18n( "Cities.dat: Ran out of fields.  Line was:" );
		kDebug()<< totalLine.toLocal8Bit();
		return false;
	} else if ( fields.size() < 12 ) {
		// allow old format (without TZ) for mycities.dat
		fields.append(QString());
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
		kDebug() << fields[3] << i18n( "\nCities.dat: Bad integer.  Line was:\n" ) << totalLine;
		return false;
	}

	latM = fields[4].toInt( &intCheck );
	if ( !intCheck ) {
		kDebug() << fields[4] << i18n( "\nCities.dat: Bad integer.  Line was:\n" ) << totalLine;
		return false;
	}

	latS = fields[5].toInt( &intCheck );
	if ( !intCheck ) {
		kDebug() << fields[5] << i18n( "\nCities.dat: Bad integer.  Line was:\n" ) << totalLine;
		return false;
	}

	QChar ctemp = fields[6].at(0);
	latsgn = ctemp;
	if (latsgn != 'N' && latsgn != 'S') {
		kDebug() << latsgn << i18n( "\nCities.dat: Invalid latitude sign.  Line was:\n" ) << totalLine;
		return false;
	}

	lngD = fields[7].toInt( &intCheck );
	if ( !intCheck ) {
		kDebug() << fields[7] << i18n( "\nCities.dat: Bad integer.  Line was:\n" ) << totalLine;
		return false;
	}

	lngM = fields[8].toInt( &intCheck );
	if ( !intCheck ) {
		kDebug() << fields[8] << i18n( "\nCities.dat: Bad integer.  Line was:\n" ) << totalLine;
		return false;
	}

	lngS = fields[9].toInt( &intCheck );
	if ( !intCheck ) {
		kDebug() << fields[9] << i18n( "\nCities.dat: Bad integer.  Line was:\n" ) << totalLine;
		return false;
	}

	ctemp = fields[10].at(0);
	lngsgn = ctemp;
	if (lngsgn != 'E' && lngsgn != 'W') {
		kDebug() << latsgn << i18n( "\nCities.dat: Invalid longitude sign.  Line was:\n" ) << totalLine;
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
			kDebug() << fields[11] << i18n( "\nCities.dat: Bad time zone.  Line was:\n" ) << totalLine;
			return false;
		}
	}

	//last field is the TimeZone Rule ID.
	TZrule = &( Rulebook[ fields[12] ] );

//	if ( fields[12]=="--" )
//		kDebug() << "Empty rule start month: " << TZrule->StartMonth;
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
				QStringList fields = line.split( " ", QString::SkipEmptyParts );
				id = fields[0];
				QTime stime = QTime( fields[3].left( fields[3].indexOf(':')).toInt() ,
								fields[3].mid( fields[3].indexOf(':')+1, fields[3].length()).toInt() );
				QTime rtime = QTime( fields[6].left( fields[6].indexOf(':')).toInt(),
								fields[6].mid( fields[6].indexOf(':')+1, fields[6].length()).toInt() );

				Rulebook[ id ] = TimeZoneRule( fields[1], fields[2], stime, fields[4], fields[5], rtime );
			}
		}
		return true;
	} else {
		return false;
	}
}

bool KStarsData::openUrlFile(const QString &urlfile, QFile & file) {
	//QFile file;
	QString localFile;
	bool fileFound = false;
	QFile localeFile;

	if ( locale->language() != "en_US" )
		localFile = locale->language() + '/' + urlfile;

	if ( ! localFile.isEmpty() && KSUtils::openDataFile( file, localFile ) ) {
		fileFound = true;
	} else {
   // Try to load locale file, if not successful, load regular urlfile and then copy it to locale.
		file.setFileName( KStandardDirs::locateLocal( "appdata", urlfile ) );
		if ( file.open( QIODevice::ReadOnly ) ) {
			//local file found.  Now, if global file has newer timestamp, then merge the two files.
			//First load local file into QStringList
			bool newDataFound( false );
			QStringList urlData;
			QTextStream lStream( &file );
			while ( ! lStream.atEnd() ) urlData.append( lStream.readLine() );

			//Find global file(s) in findAllResources() list.
			QFileInfo fi_local( file.fileName() );
			QStringList flist = KGlobal::mainComponent().dirs()->findAllResources( "appdata", urlfile );
			for ( int i=0; i< flist.size(); i++ ) {
				if ( flist[i] != file.fileName() ) {
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
									urlData.removeAt( urlData.indexOf( line.mid( 4 ) ) );
								} else {
									//does local file contain the current global file line, up to second ':' ?

									bool linefound( false );
									for ( int j=0; j< urlData.size(); ++j ) {
										if ( urlData[j].contains( line.left( line.indexOf( ':', line.indexOf( ':' ) + 1 ) ) ) ) {
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
				if ( locale->language() != "en_US" ) kDebug() << i18n( "No localized URL file; using default English file." );
				// we found urlfile, we need to copy it to locale
				localeFile.setFileName( KStandardDirs::locateLocal( "appdata", urlfile ) );
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
					kDebug() << i18n( "Failed to copy default URL file to locale folder, modifying default object links is not possible" );
				}
				fileFound = true;
			}
		}
	}
	return fileFound;
}

bool KStarsData::readURLData( const QString &urlfile, int type, bool deepOnly ) {
	QFile file;
	if (!openUrlFile(urlfile, file)) return false;

	QTextStream stream(&file);

	while ( !stream.atEnd() ) {
		QString line = stream.readLine();

		//ignore comment lines
		if ( line.left(1) != "#" ) {
			QString name = line.mid( 0, line.indexOf(':') );
			QString sub = line.mid( line.indexOf(':')+1 );
			QString title = sub.mid( 0, sub.indexOf(':') );
			QString url = sub.mid( sub.indexOf(':')+1 );
			SkyObject *o = skyComposite()->findByName(name);

			if ( !o ) {
				kWarning() << k_funcinfo << i18n( "Object named %1 not found", name ) ;
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

	if (!stream.atEnd()) buffer = stream.readAll();

	while (!buffer.isEmpty()) {
		int startIndex, endIndex;

		startIndex = buffer.indexOf("[KSLABEL:");
		sub = buffer.mid(startIndex);
		endIndex = sub.indexOf("[KSLogEnd]");

		// Read name after KSLABEL identifer
		name = sub.mid(startIndex + 9, sub.indexOf("]") - (startIndex + 9));
		// Read data and skip new line
		data   = sub.mid(sub.indexOf("]") + 2, endIndex - (sub.indexOf("]") + 2));
		buffer = buffer.mid(endIndex + 11);

		//Find the sky object named 'name'.
		//Note that ObjectNameList::find() looks for the ascii representation
		//of star genetive names, so stars are identified that way in the user log.
		SkyObject *o = skyComposite()->findByName(name);
		if ( !o ) {
			kWarning() << k_funcinfo << name << " not found" ;
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
			Name = Line.mid(0, Line.indexOf(":"));
			Link = Line.mid(Line.indexOf(":") + 1);

			// Link is empty, using Interface instead
			if (Link.isEmpty())
			{
					Link = Interface;
					subName = Name;
					interfaceIndex = Link.indexOf("KSINTERFACE");
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

	file.setFileName( KStandardDirs::locateLocal( "appdata", varFile ) );
	if ( !file.open( QIODevice::ReadOnly ) )
	{
		// Open default variable stars file
		if ( KSUtils::openDataFile( file, varFile ) )
		{
			// we found urlfile, we need to copy it to locale
			localeFile.setFileName( KStandardDirs::locateLocal( "appdata", varFile ) );

			if (localeFile.open(QIODevice::WriteOnly))
			{
					QTextStream readStream(&file);
					QTextStream writeStream(&localeFile);
					writeStream <<  readStream.readAll();
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
	char c;
	LilXML *xmlParser = newLilXML();
	XMLEle *root = NULL;
	XMLAtt *ap;

	file.setFileName( KStandardDirs::locate( "appdata", indiFile ) );
	if ( file.fileName().isEmpty() || !file.open( QIODevice::ReadOnly ) )
		 return false;

	while ( file.getChar( &c ) )
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
				delete VInfo;
				delLilXML(xmlParser);
				return false;
			}

			VInfo->hostname = QString(valuXMLAtt(ap));
			ap = findXMLAtt(root, "port");

			if (!ap) {
				delete VInfo;
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
			kDebug() << errmsg;
	}

	delLilXML(xmlParser);

	return true;
}

//FIXME: There's way too much code duplication here!
//We should just execute the script through K3Process.
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
//commands, just setDestination().  (sltoCenter() does additional things that we do not need).
bool KStarsData::executeScript( const QString &scriptname, SkyMap *map ) {
	int cmdCount(0);

	QFile f( scriptname );
	if ( !f.open( QIODevice::ReadOnly) ) {
		kDebug() << i18n( "Could not open file %1", f.fileName() );
		return false;
	}

	QTextStream istream(&f);
	while ( ! istream.atEnd() ) {
		QString line = istream.readLine();

		//found a dcop line
		if ( line.left(4) == "dcop" ) {
			line = line.mid( 20 );  //strip away leading characters
			QStringList fn = line.split( " " );

			if ( fn[0] == "lookTowards" && fn.size() >= 2 ) {
				double az(-1.0);
				QString arg = fn[1].toLower();
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
				fn.removeAll( fn.first() );
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
				// min is a macro - use mnt
				int yr(0), mth(0), day(0) ,hr(0), mnt(0), sec(0);
				yr = fn[1].toInt(&ok);
				if ( ok ) mth = fn[2].toInt(&ok);
				if ( ok ) day = fn[3].toInt(&ok);
				if ( ok ) hr  = fn[4].toInt(&ok);
				if ( ok ) mnt = fn[5].toInt(&ok);
				if ( ok ) sec = fn[6].toInt(&ok);
				if ( ok ) {
					changeDateTime( geo()->LTtoUT( KStarsDateTime( ExtDate(yr, mth, day), QTime(hr,mnt,sec) ) ) );
					cmdCount++;
				} else {
					kWarning() << ki18n( "Could not set time: %1 / %2 / %3 ; %4:%5:%6" )
						.subs( day ).subs( mth ).subs( yr )
						.subs( hr ).subs( mnt ).subs( sec ).toString() << endl;
				}
			} else if ( fn[0] == "changeViewOption" && fn.size() == 3 ) {
				bool bOk(false), nOk(false), dOk(false);

				//parse bool value
				bool bVal(false);
				if ( fn[2].toLower() == "true" ) { bOk = true; bVal = true; }
				if ( fn[2].toLower() == "false" ) { bOk = true; bVal = false; }
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
				QString city( fn[1] ), province, country( fn[2] );
				province.clear();
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
					kWarning() << i18n( "Could not set location named %1, %2, %3" , city, province, country) ;
			}
		}
	}  //end while

	if ( cmdCount ) return true;
	return false;
}

/*void KStarsData::appendTelescopeObject(SkyObject * object)
{
  INDITelescopeList.append(object);
}*/

void KStarsData::saveTimeBoxShaded( bool b ) { Options::setShadeTimeBox( b ); }
void KStarsData::saveGeoBoxShaded( bool b ) { Options::setShadeGeoBox( b ); }
void KStarsData::saveFocusBoxShaded( bool b ) { Options::setShadeFocusBox( b ); }
void KStarsData::saveTimeBoxPos( QPoint p ) { Options::setPositionTimeBox( p ); }
void KStarsData::saveGeoBoxPos( QPoint p ) { Options::setPositionGeoBox( p ); }
void KStarsData::saveFocusBoxPos( QPoint p ) { Options::setPositionFocusBox( p ); }

#include "kstarsdata.moc"
