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
//JH 25.08.2001: added i18n() to strings

#include <kmessagebox.h>
#include <qtextstream.h>

#include "kstarsdata.h"
#include <klocale.h>

#include "kstars.h"
#include <kdebug.h>

#if (KDE_VERSION <= 222)
#include <kapp.h>
#else
#include <kapplication.h>
#endif

KStarsData::KStarsData() : reloadingInProgress ( false ) {
	stdDirs = new KStandardDirs();
	options = new KStarsOptions();
/*
	* Store the max set magnitude of current session. Will increased in KStarsData::appendNewData()
	*/
	maxSetMagnitude = options->magLimitDrawStar;

	locale = new KLocale( "kstars" );
	oldOptions = 0;

	objList = new QList < SkyObject >;
	ObjNames = new QList < SkyObjectName >;

	geoList.setAutoDelete( TRUE );
	objList->setAutoDelete( FALSE );//sky objects in this list are deleted elsewhere
	starList.setAutoDelete( TRUE );
	messList.setAutoDelete( TRUE );
	ngcList.setAutoDelete( TRUE );
	icList.setAutoDelete( TRUE );
	clineList.setAutoDelete( TRUE );
	clineModeList.setAutoDelete( TRUE );
	cnameList.setAutoDelete( TRUE );
	Equator.setAutoDelete( TRUE );
	Horizon.setAutoDelete( TRUE );

  //Initialize object type strings
	//(type==-1 is a constellation)
	TypeName[0] = i18n( "star" );
	TypeName[1] = i18n( "star" );
	TypeName[2] = i18n( "planet" );
	TypeName[3] = i18n( "open cluster" );
	TypeName[4] = i18n( "globular cluster" );
	TypeName[5] = i18n( "gaseous nebula" );
	TypeName[6] = i18n( "planetary nebula" );
	TypeName[7] = i18n( "supernova remnant" );
	TypeName[8] = i18n( "galaxy" );
	TypeName[9] = i18n( "Users should never see this", "UNUSED_TYPE" );
}

KStarsData::~KStarsData() {
	if ( 0 != oldOptions ) {
  	delete oldOptions;
		oldOptions = 0;
	}
	if ( 0 != options ) {
		delete options;
		options = 0;
	}
	// the list items do not need to be removed by hand.
	// all lists are set to AutoDelete = true
	delete ObjNames;
	delete stdDirs;
	delete Sun;
	delete Mercury;
	delete Venus;
	delete Mars;
	delete Jupiter;
	delete Saturn;
	delete Uranus;
	delete Neptune;
}

//Functions for reading data files
bool KStarsData::openDataFile( QFile &file, QString s ) {
	bool result;
	KStandardDirs *stdDirs = new KStandardDirs();
	QString FileName= stdDirs->findResource( "data", "kstars/" + s );

	if ( FileName != QString::null ) {
		file.setName( FileName );
		if ( !file.open( IO_ReadOnly ) ) {
			delete stdDirs;
			result = false;
		}
		delete stdDirs;
		result = true;
  } else { //File not found, try the local option
		file.setName( "data/" + s );

		if (!file.open( IO_ReadOnly ) ) { //File not found.  return false.
			delete stdDirs;
			result = false;
		}	else {
			delete stdDirs;
			result = true;
		}
  }

	return result;
}

bool KStarsData::readMWData( void ) {
	QFile file;

	for ( unsigned int i=0; i<11; ++i ) {
		QString snum, fname, szero;
		snum = snum.setNum( i+1 );
		if ( i+1 < 10 ) szero = "0"; else szero = "";
		fname = "mw" + szero + snum + ".dat";

		if ( openDataFile( file, fname ) ) {
		  QTextStream stream( &file );

	  	while ( !stream.eof() ) {
		  	QString line;
			  double ra, dec;

			  line = stream.readLine();

			  ra = line.mid( 0, 8 ).toDouble();
	  		dec = line.mid( 9, 8 ).toDouble();

			  SkyPoint *o = new SkyPoint( ra, dec );
			  MilkyWay[i].append( o );
	  	}
			file.close();
		} else {
			return false;
		}
	}
	return true;
}

bool KStarsData::readCLineData( void ) {
	QFile file;
	if ( openDataFile( file, "clines.dat" ) ) {
	  QTextStream stream( &file );

  	while ( !stream.eof() ) {
	  	QString line, name;
		  int rah, ram, ras, dd, dm, ds;
		  QChar sgn;
			QChar *mode;

		  line = stream.readLine();

		  rah = line.mid( 0, 2 ).toInt();
	  	ram = line.mid( 2, 2 ).toInt();
		  ras = line.mid( 4, 2 ).toInt();

		  sgn = line.at( 6 );
	  	dd = line.mid( 7, 2 ).toInt();
		  dm = line.mid( 9, 2 ).toInt();
		  ds = line.mid( 11, 2 ).toInt();
	    mode = new QChar( line.at( 13 ) );
		  dms r; r.setH( rah, ram, ras );
	  	dms d( dd, dm,  ds );

		  if ( sgn == "-" ) { d.setD( -1.0*d.Degrees() ); }

		  SkyPoint *o = new SkyPoint( r.Hours(), d.Degrees() );
		  clineList.append( o );
			clineModeList.append( mode );
  	}
		file.close();

		return true;
	} else {
		return false;
	}
}

bool KStarsData::readCNameData( void ) {
  QFile file;
	cnameFile = "cnames.dat";
//	if ( locale->language()=="fr" ) cnameFile = "fr_cnames.dat";

	if ( openDataFile( file, cnameFile ) ) {
	  QTextStream stream( &file );

  	while ( !stream.eof() ) {
	  	QString line, name, abbrev;
		  int rah, ram, ras, dd, dm, ds;
		  QChar sgn;

	  	line = stream.readLine();

		  rah = line.mid( 0, 2 ).toInt();
		  ram = line.mid( 2, 2 ).toInt();
	  	ras = line.mid( 4, 2 ).toInt();

		  sgn = line.at( 6 );
		  dd = line.mid( 7, 2 ).toInt();
	  	dm = line.mid( 9, 2 ).toInt();
		  ds = line.mid( 11, 2 ).toInt();

			abbrev = line.mid( 13, 3 );
		  name  = line.mid( 17 ).stripWhiteSpace();

		  dms r; r.setH( rah, ram, ras );
		  dms d( dd, dm,  ds );

	  	if ( sgn == "-" ) { d.setD( -1.0*d.Degrees() ); }

		  SkyObject *o = new SkyObject( -1, r, d, 0.0, name, abbrev, "" );
	  	cnameList.append( o );
			objList->append( o );
	  	ObjNames->append (new SkyObjectName (o->name(), objList->last()));
	  }
		file.close();

		return true;
	} else {
		return false;
	}
}

bool KStarsData::readStarData( void ) {
	QFile file;
	if ( openDataFile( file, "sao.dat" ) ) {
		QTextStream stream( &file );

		while ( !stream.eof() ) {
			QString line, name, gname, SpType;
			float mag;
			int rah, ram, ras, dd, dm, ds;
			QChar sgn;

			line = stream.readLine();

			mag = line.mid( 33, 4 ).toFloat();	// star magnitude
			if ( mag > options->magLimitDrawStar ) break;	// break reading file if magnitude is higher than needed
			
			name = "star"; gname = "";

			rah = line.mid( 0, 2 ).toInt();
			ram = line.mid( 2, 2 ).toInt();
			ras = int( line.mid( 4, 6 ).toDouble() + 0.5 ); //add 0.5 to make int() pick nearest integer

			sgn = line.at( 17 );
			dd = line.mid( 18, 2 ).toInt();
			dm = line.mid( 20, 2 ).toInt();
			ds = int( line.mid( 22, 5 ).toDouble() + 0.5 ); //add 0.5 to make int() pick nearest integer

			SpType = line.mid( 37, 2 );
			name = line.mid( 40 ).stripWhiteSpace(); //the rest of the line
			if ( name.contains( ':' ) ) { //genetive form exists
				gname = name.mid( name.find(':')+1 );
				name = name.mid( 0, name.find(':') ).stripWhiteSpace();
			}
			if ( name.isEmpty() ) name = "star";

			dms r;
			r.setH( rah, ram, ras );
			dms d( dd, dm,  ds );

			if ( sgn == "-" ) { d.setD( -1.0*d.Degrees() ); }

			StarObject *o = new StarObject( r, d, mag, name, gname, SpType );
	  		starList.append( o );

			if ( o->name() != "star" ) {		// just add to name list if a name is given
				objList->append( o );
				ObjNames->append (new SkyObjectName (o->name(), objList->last()));
//				starList.last()->setSkyObjectName( ObjNames->last() );	// set pointer to ObjNames
			}

			if (mag < 4.0) starCount0 = starList.count();
			if (mag < 6.0) starCount1 = starList.count();
			if (mag < 7.0) starCount2 = starList.count();
			if (mag < 7.5) starCount3 = starList.count();
  		}	// end of while
		lastFileIndex = file.at();	// stores current file index - needed by reloading
		file.close();
    	return true;
	} else {
		return false;
	}
}

bool KStarsData::readNGCData( void ) {
	QFile file;
	if ( openDataFile( file, "ngc2000.dat" ) ) {
	  QTextStream stream( &file );

  	while ( !stream.eof() ) {
	  	QString line, smag, name, name2, longname;
		  float mag, ramf;
		  int type, ingc, imess, rah, ram, ras, dd, dm;
	  	QChar sgn, cat_id, cat2_id, mflag;

			//set the compiler at ease; initialize these variables
			imess = 0;
			mag = 0.0;

		  line = stream.readLine();

		  cat_id = line.at( 0 ); //check for IC catalog flag
	  	if ( cat_id == " " ) cat_id = 'N'; // if blank, set catalog to NGC

		  ingc = line.mid( 1, 4 ).toInt();  // NGC/IC catalog number
			type = line.mid( 6, 1 ).toInt();     // object type identifier
	  	rah = line.mid( 8, 2 ).toInt();
		  ramf = line.mid( 11, 4 ).toFloat();
		  sgn = line.at( 16 );
	  	dd = line.mid( 17, 2 ).toInt();
		  dm = line.mid( 20, 2 ).toInt();
		  smag = line.mid( 23, 4 );
		  if (smag == "    " ) { mag = 99.9; } else { mag = smag.toFloat(); }
			mflag = line.at( 28 );
			if ( mflag=='M' ) {
				cat2_id = cat_id;
				if ( ingc==0 ) cat2_id = ' ';
				cat_id = mflag;
				imess = line.mid( 30, 3 ).toInt();
			}
			longname = line.mid( 34, 30 ).stripWhiteSpace();

		  ram = int( ramf );
		  ras = int( 60.0*( ramf - (float)ram ) );

	  	dms r; r.setH( rah, ram, ras );
		  dms d( dd, dm, 0 );

		  if ( sgn == "-" ) { d.setD( -1.0*d.Degrees() ); }

			QString snum;
			if (cat_id=="I") {
				snum.setNum( ingc );
				name = "IC " + snum;
			} else if (cat_id=="N") {
				snum.setNum( ingc );
				name = "NGC " + snum;
			} else if (cat_id=="M") {
				snum.setNum( imess );
				name = "M " + snum;
				if (cat2_id=="N") {
					snum.setNum( ingc );
					name2 = "NGC " + snum;
				} else if (cat2_id=="I") {
					snum.setNum( ingc );
					name2 = "IC " + snum;
				} else {
					name2 = "";
				}
			}

	  	SkyObject *o = new SkyObject( type, r, d, mag, name, name2, longname );

	  	if ( !o->longname().isEmpty() ) {
	  		objList->append( o );
  			ObjNames->append (new SkyObjectName (o->longname(), objList->last()));
	  	}

		  if (cat_id=="M") {
	  		messList.append( o );
				objList->append( o );
	  		ObjNames->append (new SkyObjectName (o->name(), objList->last()));
		  } else if ( cat_id=="N" ) {
			  ngcList.append( o );
				objList->append( o );
	  		ObjNames->append (new SkyObjectName (o->name(), objList->last()));
			} else if ( cat_id=="I" ) {
				icList.append( o );
				objList->append( o );
	  		ObjNames->append (new SkyObjectName (o->name(), objList->last()));
			}
	  }
		file.close();

		return true;
	} else {
		return false;
	}
}

bool KStarsData::readURLData( QString urlfile, int type ) {
	QFile file;
	bool fileFound = false;

	if ( openDataFile( file, urlfile ) ) {
		fileFound = true;
	} else {
		file.setName( locateLocal( "appdata", urlfile ) );
		if ( file.open( IO_ReadOnly ) ) fileFound = true;
	}

	if ( fileFound ) {
	  QTextStream stream( &file );

  	while ( !stream.eof() ) {
	  	QString line = stream.readLine();
			QString name = line.mid( 0, line.find(':') );
			QString sub = line.mid( line.find(':')+1 );
			QString title = sub.mid( 0, sub.find(':') );
			QString url = sub.mid( sub.find(':')+1 );

			for ( unsigned int i=0; i<objList->count(); ++i ) {
				SkyObject *o = objList->at(i);
				if ( o->name() == name ) {
					if ( type==0 ) { //image URL
        		o->ImageList.append( url );
						o->ImageTitle.append( title );
					} else if ( type==1 ) { //info URL
        		o->InfoList.append( url );
						o->InfoTitle.append( title );
					}
	        break;
				}
			}
		}
		file.close();

		return true;
	} else {
		return false;
	}
}

bool KStarsData::processCity( QString line ) {
			QString totalLine, field[12];
			QString name, province, country;
			bool intCheck = true, lastField = false;
			char latsgn, lngsgn;
			int lngD, lngM, lngS;
	  	int latD, latM, latS;
			double TZ;
			float lng, lat;

			totalLine = line;
	
			// separate fields
			unsigned int i;
			for ( i=0; i<12; ++i ) {
				field[i] = line.mid( 0, line.find(':') ).stripWhiteSpace();
				line = line.mid( line.find(':')+1 );
				if (lastField) break; //ran out of fields
				if (line.find(':')<0) lastField = true; //no more field delimiters
			}

			if ( i<10 ) {
				kdDebug()<< i18n( "Cities.dat: Ran out of fields. Line was:" ) <<endl;
				kdDebug()<< totalLine.local8Bit() <<endl;
				return false;   
			}
			if ( i < 11  ) {	
				// allow old format (without TZ) for mycities.dat
				field[11] = QString("");
			}

			name  		= field[0];
		  province 	= field[1];
			country 	= field[2];

			latD = field[3].toInt( &intCheck );
			if ( !intCheck ) {
				kdDebug() << field[3] << i18n( "\nCities.dat: Bad integer. Line was:\n" ) << totalLine << endl;
				return false;
			}

			latM = field[4].toInt( &intCheck );
			if ( !intCheck ) {
				kdDebug() << field[4] << i18n( "\nCities.dat: Bad integer. Line was:\n" ) << totalLine << endl;
				return false;
			}

			latS = field[5].toInt( &intCheck );
			if ( !intCheck ) {
				kdDebug() << field[5] << i18n( "\nCities.dat: Bad integer. Line was:\n" ) << totalLine << endl;
				return false;
			}

			QChar ctemp = field[6].at(0);
			latsgn = ctemp;
			if (latsgn != 'N' && latsgn != 'S') {
				kdDebug() << latsgn << i18n( "\nCities.dat: Invalid latitude sign. Line was:\n" ) << totalLine << endl;
				return false;
			}

			lngD = field[7].toInt( &intCheck );
			if ( !intCheck ) {
				kdDebug() << field[7] << i18n( "\nCities.dat: Bad integer. Line was:\n" ) << totalLine << endl;
				return false;
			}

			lngM = field[8].toInt( &intCheck );
			if ( !intCheck ) {
				kdDebug() << field[8] << i18n( "\nCities.dat: Bad integer. Line was:\n" ) << totalLine << endl;
				return false;
			}

			lngS = field[9].toInt( &intCheck );
			if ( !intCheck ) {
				kdDebug() << field[9] << i18n( "\nCities.dat: Bad integer. Line was:\n" ) << totalLine << endl;
				return false;
			}

			ctemp = field[10].at(0);
			lngsgn = ctemp;
			if (lngsgn != 'E' && lngsgn != 'W') {
				kdDebug() << latsgn << i18n( "\nCities.dat: Invalid longitude sign. Line was:\n" ) << totalLine << endl;
				return false;
			}

  		lat = (float)latD + ((float)latM + (float)latS/60.0)/60.0;
			lng = (float)lngD + ((float)lngM + (float)lngS/60.0)/60.0;

			if ( latsgn == 'S' ) lat *= -1.0;
			if ( lngsgn == 'W' ) lng *= -1.0;

			// find time zone. Use value from Cities.dat if available.
			// otherwise use the old approximation: int(lng/15.0);
			if ( field[11].isEmpty() || ('x' == field[11].at(0)) ) {
				TZ = int(lng/15.0);
			} else {
				bool doubleCheck = true;
				TZ = field[11].toDouble( &doubleCheck);
				if ( !doubleCheck ) {
					kdDebug() << field[11] << i18n( "\nCities.dat: Bad time zone. Line was:\n" ) << totalLine << endl;
					return false;
				}
			}

			geoList.append ( new GeoLocation( lng, lat, name, province, country, TZ ));		// appends city names to list
			return true;
}

bool KStarsData::readCityData( void ) {
	QFile file;
	bool citiesFound = false;

	if ( openDataFile( file, "Cities.dat" ) ) {
		QTextStream stream( &file );

  	while ( !stream.eof() ) {
			QString line = stream.readLine();
			citiesFound |= processCity( line );
		}	// while ( !stream.eof() )
		file.close();
	}	// if ( openDataFile() )

	//check for local cities database, but don't require it.
	file.setName( locateLocal( "appdata", "mycities.dat" ) ); //determine filename in local user KDE directory tree.
	if ( file.open( IO_ReadOnly ) ) {
		QTextStream stream( &file );

  	while ( !stream.eof() ) {
			QString line = stream.readLine();
			citiesFound |= processCity( line );
		}	// while ( !stream.eof() )
		file.close();
	}	// if ( fileopen() )

	return citiesFound;
}

// Save and restore options
void KStarsData::saveOptions()
{
	if ( 0 == oldOptions) {
		oldOptions = new KStarsOptions();
	}
	oldOptions->copy( options );
}

void KStarsData::restoreOptions()
{
	if ( 0 == oldOptions ) {
		// this should not happen
		return;
	}
	options->copy( oldOptions );
}

long double KStarsData::getJD( QDateTime t ) {
  int year = t.date().year();
  int month = t.date().month();
  int day = t.date().day();
  int hour = t.time().hour();
  int minute = t.time().minute();
  double second = t.time().second() + 0.001*t.time().msec();
  int m, y, A, B, C, D;

  if (month > 2) {
	  m = month;
	  y = year;
  } else {
	  y = year - 1;
	  m = month + 12;
  }

 /*  If the date is after 10/15/1582, then take Pope Gregory's modification
	 to the Julian calendar into account */

	 if (( year  >1582 ) ||
		 ( year ==1582 && month  >9 ) ||
		 ( year ==1582 && month ==9 && day >15 ))
	 {
		 A = int(y/100);
		 B = 2 - A + int(A/4);
	 } else {
		 B = 0;
	 }

  if (y < 0) {
	  C = int((365.25*y) - 0.75);
  } else {
	  C = int(365.25*y);
  }

  D = int(30.6001*(m+1));

  long double d = double(day) + (double(hour) + (double(minute) + second/60.0)/60.0)/24.0;
  long double jd = B + C + D + d + 1720994.5;

  return jd;
}

//void KStarsData::getDateTime( long double jd, QDate &date, QTime &time ) {
QDateTime KStarsData::getDateTime( long double jd ) {
	int A, B, C, D, E, G, day, month, year, hour, minute, second, msec;
	long double H, M, S;

	//Add 0.5 to place day boundary at midnight instead of noon:
	jd = jd + 0.5;
	int jday = int( jd );
	long double dayfrac = jd - jday;

	if ( jday > 2299160 ) { //JD of Gregorian Calendar start
		A = int( ( jday - 1867216.25 ) / 36524.25 );
		B = jday + 1 + A - int( A/4 );
	} else {
		B = jday;
	}

	C = B + 1524;
	D = int( ( C - 122.1 )/365.25 );
	E = int( 365.25*D );
	G = int( ( C - E ) / 30.6001 );

	day = C - E - int( 30.6001*G );
	
	if ( G < 14 ) month = G - 1;
	else month = G - 13;

	if ( month > 2 ) year = D - 4716;
	else year = D - 4715;

	H = dayfrac*24.0;
	hour = int( H );
	M = ( H - hour )*60.0;
	minute = int( M );
	S = ( M - minute )*60.0;
	second = int( S );
	msec = int( ( S - second )*1000.0 );

	return QDateTime( QDate( year, month, day ), QTime( hour, minute, second, msec ) );
//	date = QDate( year, month, day );
//	time = QTime( hour, minute, second, msec );
}

void KStarsData::setMagnitude( float newMagnitude ) {
/*
	* Only reload data if not loaded yet.
	*/
	if ( newMagnitude > maxSetMagnitude )
		reloadStarData( newMagnitude );

// change current magnitude level in KStarsOptions
	options->setMagLimitDrawStar( newMagnitude );
}

bool KStarsData::reloadStarData( float newMag ) {
/*
	* If reloading allready works break at this point. If not it cause
	* a loading of stars more than 5 times. It's needed due to reloading
	* in background.
	*/
	if ( reloadingInProgress ) return false;
/*
	* Set true to avoid more than one access of this function. Every enquiry
	* will rejected if this variable is true.
	*/
	reloadingInProgress = true;

	QFile file;
	if ( openDataFile( file, "sao.dat" ) ) {
		if ( !file.at( lastFileIndex ) )
                    kdDebug()<<"Warning: Setting of file index failed\n";
	// set index to last index in file

		QTextStream stream( &file );

		float mag;	// magnitude of current star read in file
		int i = 0;

		while ( !stream.eof() ) {
			QString line, name, gname, SpType;
			int rah, ram, ras, dd, dm, ds;
			QChar sgn;

			line = stream.readLine();

			mag = line.mid( 33, 4 ).toFloat();	// star magnitude

			if ( mag > newMag ) break;	// break reading file if magnitude is higher than needed

			name = "star";
			rah = line.mid( 0, 2 ).toInt();
			ram = line.mid( 2, 2 ).toInt();
			ras = int( line.mid( 4, 6 ).toDouble() + 0.5 ); //add 0.5 to make int() pick nearest integer

			sgn = line.at( 17 );
			dd = line.mid( 18, 2 ).toInt();
			dm = line.mid( 20, 2 ).toInt();
			ds = int( line.mid( 22, 5 ).toDouble() + 0.5 ); //add 0.5 to make int() pick nearest integer

			SpType = line.mid( 37, 2 );
			name = line.mid( 40 ).stripWhiteSpace(); //the rest of the line
			if ( name.contains( ':' ) ) { //genetive form exists
				gname = name.mid( name.find(':')+1 );
				name = name.mid( 0, name.find(':') ).stripWhiteSpace();
			}
			if ( name.isEmpty() ) name = "star";

			dms r;
			r.setH( rah, ram, ras );
			dms d( dd, dm,  ds );

			if ( sgn == "-" ) d.setD( -1.0*d.Degrees() );
			
			StarObject *o = new StarObject( r, d, mag, name, gname, SpType );
			starList.append( o );

			if ( o->name() != "star" ) {		// just add to name list if a name is given
				objList->append( o );
				ObjNames->append (new SkyObjectName (o->name(), objList->last()));
//					starList.last()->setSkyObjectName( ObjNames->last() );
			}
			// recompute coordinates if AltAz is used
			o->pos()->EquatorialToHorizontal( LSTh, ( ( KStars *) kapp )->geo()->lat() );

			if (mag < 4.0) starCount0 = starList.count();
			if (mag < 6.0) starCount1 = starList.count();
			if (mag < 7.0) starCount2 = starList.count();
			if (mag < 7.5) starCount3 = starList.count();

/*
	* If new magnitude is bigger than 6.0 check every 5000 stars the process loop.
	* So it loads data in background.
	*/
			if ( ++i >= 5000 ) {
				i = 0;
				if ( newMag > 6.0 ) kapp->processEvents();
			}

  		}	// enf of while
		maxSetMagnitude = mag;	// store new set magnitude to compare in KStarsData::setMagnitude()
		lastFileIndex = file.at();	// stores last file index
		file.close();
// Set false to unlock the function.
    	reloadingInProgress = false;
    	return true;
	} else {
// Set false to unlock the function.
    	reloadingInProgress = false;
		return false;
	}

}
