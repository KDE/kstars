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

#include <kapp.h>
#include <kmessagebox.h>
#include <qtextstream.h>

#include "dms.h"
#include "skypoint.h"
#include "kstarsdata.h"
#include "klocale.h"

KStarsData::KStarsData() {
	stdDirs = new KStandardDirs();
	options = new KStarsOptions();
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

		  if ( sgn == "-" ) { d.setD( -1.0*d.getD() ); }

		  SkyPoint *o = new SkyPoint( r.getH(), d.getD() );
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
	if ( locale->language()=="fr" ) cnameFile = "fr_cnames.dat";

	if ( openDataFile( file, cnameFile ) ) {
	  QTextStream stream( &file );

  	while ( !stream.eof() ) {
	  	QString line, name;
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

		  name  = line.mid( 13 ).stripWhiteSpace();
	
		  dms r; r.setH( rah, ram, ras );
		  dms d( dd, dm,  ds );

	  	if ( sgn == "-" ) { d.setD( -1.0*d.getD() ); }

		  SkyObject *o = new SkyObject( -1, r, d, 0.0, name );
	  	cnameList.append( o );
			objList->append( o );
	  		ObjNames->append (new SkyObjectName (o->name, objList->last()));
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
			QString line, name, SpType;
			float mag;
			int rah, ram, ras, dd, dm, ds;
			QChar sgn;

	  	line = stream.readLine();

			name = "star";
			rah = line.mid( 0, 2 ).toInt();
		  ram = line.mid( 2, 2 ).toInt();
			ras = int( line.mid( 4, 6 ).toDouble() + 0.5 ); //add 0.5 to make int() pick nearest integer

			sgn = line.at( 17 );
		  dd = line.mid( 18, 2 ).toInt();
			dm = line.mid( 20, 2 ).toInt();
			ds = int( line.mid( 22, 5 ).toDouble() + 0.5 ); //add 0.5 to make int() pick nearest integer

		  mag = line.mid( 33, 4 ).toFloat();
		  SpType = line.mid( 37, 2 );
			name = line.mid( 40, 20 ).stripWhiteSpace();
			if ( name.isEmpty() ) name = "star";

			dms r;
			r.setH( rah, ram, ras );
			dms d( dd, dm,  ds );
	
			if ( sgn == "-" ) { d.setD( -1.0*d.getD() ); }

			StarObject *o = new StarObject( 0, r, d, mag, name, "", "", SpType );
		  	starList.append( o );
			
			if ( o->name != "star" ) {
				objList->append( o );
  				ObjNames->append (new SkyObjectName (o->name, objList->last()));
			}
						
			if (mag < 4.0) starCount0 = starList.count();
			if (mag < 6.0) starCount1 = starList.count();
			if (mag < 7.0) starCount2 = starList.count();
			if (mag < 7.5) starCount3 = starList.count();
  		}	// enf of while
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

		  if ( sgn == "-" ) { d.setD( -1.0*d.getD() ); }
	
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

	  	if ( !o->longname.isEmpty() ) {
	  		objList->append( o );
  			ObjNames->append (new SkyObjectName (o->longname, objList->last()));
	  	}
	  			
		  if (cat_id=="M") {
	  		messList.append( o );
				objList->append( o );
	  		ObjNames->append (new SkyObjectName (o->name, objList->last()));
		  } else if ( cat_id=="N" ) {
			  ngcList.append( o );
				objList->append( o );
	  		ObjNames->append (new SkyObjectName (o->name, objList->last()));
			} else if ( cat_id=="I" ) {
				icList.append( o );
				objList->append( o );
	  		ObjNames->append (new SkyObjectName (o->name, objList->last()));
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
				if ( o->name == name ) {
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

bool KStarsData::readCityData( void ) {
	QFile file;
	bool citiesFound = false;

	if ( openDataFile( file, "Cities.dat" ) ) {
		QTextStream stream( &file );

  	while ( !stream.eof() ) {
	  	QString line, name, state;
			char latsgn, lngsgn;
			int lngD, lngM, lngS;
	  	int latD, latM, latS;
			int TZ;
		  float lng, lat;

		 	line = stream.readLine();

		  name  = line.left( 32 );
	  	state = line.mid( 31, 17 );
		 	latD = line.mid( 49, 2 ).toInt();
		  latM = line.mid( 52, 2 ).toInt();
	  	latS = line.mid( 55, 2 ).toInt();
		 	latsgn = line.at( 58 ).latin1();
		  lngD = line.mid( 60, 3 ).toInt();
	  	lngM = line.mid( 64, 2 ).toInt();
		 	lngS = line.mid( 67, 2 ).toInt();
		  lngsgn = line.at( 70 ).latin1();

	  	lat = (float)latD + ((float)latM + (float)latS/60.0)/60.0;
			lng = (float)lngD + ((float)lngM + (float)lngS/60.0)/60.0;
  	
		 	if ( latsgn == 'S' ) lat *= -1.0;
			if ( lngsgn == 'W' ) lng *= -1.0;
  	
		  TZ = int(lng/-15.0);

//Strip off white space, and capitalize words properly
			name = name.stripWhiteSpace();
			name = name.left(1).upper() + name.mid(1).lower();
			if ( name.contains( ' ' ) ) {
				for ( unsigned int i=1; i<name.length(); ++i ) {
					if ( name.at(i) == ' ' ) name.replace( i+1, 1, name.at(i+1).upper() );
				}
			}

			state = state.stripWhiteSpace();
			state = state.left(1).upper() + state.mid(1).lower();
			if ( state.contains( ' ' ) ) {
				for ( unsigned int i=1; i<state.length(); ++i ) {
					if ( state.at(i) == ' ' ) state.replace( i+1, 1, state.at(i+1).upper() );
				}
			}

			geoList.append (new GeoLocation( lng, lat, name, state, TZ ));
			if ( !citiesFound ) citiesFound = true; //found at least one city
   	}
		file.close();
	}

	//check for local cities database, but don't require it.
	file.setName( locateLocal( "appdata", "mycities.dat" ) ); //determine filename in local user KDE directory tree.
	if ( file.open( IO_ReadOnly ) ) {
		QTextStream stream( &file );

  	while ( !stream.eof() ) {
	  	QString line, name, state;
			char latsgn, lngsgn;
			int lngD, lngM, lngS;
	  	int latD, latM, latS;
			int TZ;
		  float lng, lat;

		 	line = stream.readLine();

		  name  = line.left( 32 );
	  	state = line.mid( 31, 17 );
		 	latD = line.mid( 49, 2 ).toInt();
		  latM = line.mid( 52, 2 ).toInt();
	  	latS = line.mid( 55, 2 ).toInt();
		 	latsgn = line.at( 58 ).latin1();
		  lngD = line.mid( 60, 3 ).toInt();
	  	lngM = line.mid( 64, 2 ).toInt();
		 	lngS = line.mid( 67, 2 ).toInt();
		  lngsgn = line.at( 70 ).latin1();

	  	lat = (float)latD + ((float)latM + (float)latS/60.0)/60.0;
			lng = (float)lngD + ((float)lngM + (float)lngS/60.0)/60.0;
  	
		 	if ( latsgn == 'S' ) lat *= -1.0;
			if ( lngsgn == 'W' ) lng *= -1.0;
  	
		  TZ = int(lng/-15.0);

//Strip off white space, and capitalize words properly
			name = name.stripWhiteSpace();
			name = name.left(1).upper() + name.mid(1).lower();
			if ( name.contains( ' ' ) ) {
				for ( unsigned int i=1; i<name.length(); ++i ) {
					if ( name.at(i) == ' ' ) name.replace( i+1, 1, name.at(i+1).upper() );
				}
			}

			state = state.stripWhiteSpace();
			state = state.left(1).upper() + state.mid(1).lower();
			if ( state.contains( ' ' ) ) {
				for ( unsigned int i=1; i<state.length(); ++i ) {
					if ( state.at(i) == ' ' ) state.replace( i+1, 1, state.at(i+1).upper() );
				}
			}

			//insert the custom city alphabetically into geoList
			for ( unsigned int i=0; i < geoList.count(); ++i ) {
				if ( geoList.at(i)->name().lower() > name.lower() ) {
					int TZ = int(lng/-15.0); //estimate time zone
					geoList.insert( i, new GeoLocation( lng, lat, name, state, TZ ) );
					break;
				}
      }

			geoList.append (new GeoLocation( lng, lat, name, state, TZ ));
			if ( !citiesFound ) citiesFound = true; //found at least one city
   	}
		file.close();
  }

	if ( citiesFound ) {
		return true;
	} else {
		return false;
	}
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
