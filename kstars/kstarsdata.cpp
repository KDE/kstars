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

#include <qregexp.h>
#include <kmessagebox.h>
#include <kdebug.h>
#include <kstandarddirs.h>
#include <qdir.h>

#include "ksutils.h"
#include "kstarsdata.h"
#include "kstarsmessagebox.h"
#include "skymap.h"
#include "filesource.h"
#include "stardatasink.h"
#include "ksfilereader.h"
#include "indi/lilxml.h"

#include <kapplication.h>

QPtrList<GeoLocation> KStarsData::geoList = QPtrList<GeoLocation>();
QMap<QString, TimeZoneRule> KStarsData::Rulebook = QMap<QString, TimeZoneRule>();
int KStarsData::objects = 0;

KStarsData::KStarsData() {
	saoFileReader = 0;
	Moon = 0;
	jmoons = 0;
	initTimer = 0L;
	startupComplete = false;
	source = 0;
	loader = 0;
	pump = 0;
	objects++;

	//standard directories and locale objects
	stdDirs = new KStandardDirs();
	locale = new KLocale( "kstars" );

	//Instantiate the SimClock object
	Clock = new SimClock();

	//Instantiate KStarsOptions object
	options = new KStarsOptions();
	oldOptions = 0;

	//Sidereal Time and Hour Angle objects
	LST = new dms();
	HourAngle = new dms();

	//Instantiate planet catalog
	PC = new PlanetCatalog(this);

	//set AutoDelete property for QPtrLists.  Most are set TRUE,
	//but some 'meta-lists' need to be FALSE.
	starList.setAutoDelete( TRUE );
	ADVtreeList.setAutoDelete( TRUE );
	geoList.setAutoDelete( TRUE );
	deepSkyList.setAutoDelete( TRUE );        // list of all deep space objects

	//separate lists for each deep-sky catalog.  The objects are duplicates of
	//deepSkyList, so do not delete them twice!
  deepSkyListMessier.setAutoDelete( FALSE );
  deepSkyListNGC.setAutoDelete( FALSE );
  deepSkyListIC.setAutoDelete( FALSE );
  deepSkyListOther.setAutoDelete( FALSE );

	//ObjLabelList does not construct new objects, so no autoDelete needed
	ObjLabelList.setAutoDelete( FALSE );

	cometList.setAutoDelete( TRUE );
	asteroidList.setAutoDelete( TRUE );

	clineList.setAutoDelete( TRUE );
	clineModeList.setAutoDelete( TRUE );
	cnameList.setAutoDelete( TRUE );

	Equator.setAutoDelete( TRUE );
	Ecliptic.setAutoDelete( TRUE );
	Horizon.setAutoDelete( TRUE );
	for ( unsigned int i=0; i<11; ++i ) MilkyWay[i].setAutoDelete( TRUE );

	VariableStarsList.setAutoDelete(TRUE);
	INDIHostsList.setAutoDelete(TRUE);

	//Initialize object type strings
	//(type==-1 is a constellation)
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

	// at startup times run forward
	setTimeDirection( 0.0 );

	temporaryTrail = false;
}

KStarsData::~KStarsData() {
	objects--;
	checkDataPumpAction();

  	delete oldOptions;
        oldOptions = 0;

        delete options;
        options = 0;
	// the list items do not need to be removed by hand.
	// all lists are set to AutoDelete = true

	delete stdDirs;
	delete Clock;
	delete Moon;
	delete locale;
	delete LST;
	delete HourAngle;
	delete PC;
	delete jmoons;
        delete initTimer;
}

bool KStarsData::readMWData( void ) {
	QFile file;

	for ( unsigned int i=0; i<11; ++i ) {
		QString snum, fname, szero;
		snum = snum.setNum( i+1 );
		if ( i+1 < 10 ) szero = "0"; else szero = "";
		fname = "mw" + szero + snum + ".dat";

		if ( KSUtils::openDataFile( file, fname ) ) {
			KSFileReader fileReader( file ); // close file is included
			while ( fileReader.hasMoreLines() ) {
				QString line;
				double ra, dec;

				line = fileReader.readLine();

				ra = line.mid( 0, 8 ).toDouble();
				dec = line.mid( 9, 8 ).toDouble();

				SkyPoint *o = new SkyPoint( ra, dec );
				MilkyWay[i].append( o );
			}
		} else {
			return false;
		}
	}
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
		if ( !file.open( IO_ReadOnly ) )
		{
			// Open default variable stars file
			if ( KSUtils::openDataFile( file, varFile ) )
			{
				// we found urlfile, we need to copy it to locale
				localeFile.setName( locateLocal( "appdata", varFile ) );

				if (localeFile.open(IO_WriteOnly))
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

    Designation = Line.mid(0,8).stripWhiteSpace();
    Name          = Line.mid(10,20).simplifyWhiteSpace();

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

  file.setName( locateLocal( "appdata", indiFile ) );
		if ( !file.open( IO_ReadOnly ) )
		 return false;

while ( (c = (char) file.getch()) != -1)
 {
    root = readXMLEle(xmlParser, c, errmsg);

    if (root)
    {

      // Get host name
      ap = findXMLAtt(root, "hostname");

      if (!ap)
       return false;

    INDIHostsInfo *VInfo = new INDIHostsInfo;
    VInfo->hostname = QString(ap->valu);

    ap = findXMLAtt(root, "port");

     if (!ap)
      return false;

    VInfo->portnumber = QString(ap->valu);

    VInfo->isConnected = false;
    VInfo->mgrID = -1;

    INDIHostsList.append(VInfo);

    delXMLEle(root);
    }

    else if (errmsg[0])
     kdDebug() << errmsg << endl;

  }

  return true;


}

bool KStarsData::readCLineData( void ) {
	QFile file;
	if ( KSUtils::openDataFile( file, "clines.dat" ) ) {
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

	if ( KSUtils::openDataFile( file, cnameFile ) ) {
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
			ObjNames.append( o );
	  }
		file.close();

		return true;
	} else {
		return false;
	}
}

bool KStarsData::openSAOFile(int i) {
	if (saoFileReader != 0) delete saoFileReader;
	QFile file;
	QString snum, fname;
	snum = QString().sprintf("%02d", i);
	fname = "sao" + snum + ".dat";
	if (KSUtils::openDataFile(file, fname)) {
		saoFileReader = new KSFileReader(file); // close file is included
	} else {
		saoFileReader = 0;
		return false;
	}
	return true;
}

//02/2003: NEW: split data files, using Heiko's new KSFileReader.
bool KStarsData::readStarData( void ) {
	bool ready = false;
	for (unsigned int i=1; i<NSAOFILES+1; ++i) {
		if (openSAOFile(i) == true) {
			while (saoFileReader->hasMoreLines()) {
				QString line;
				float mag;

				line = saoFileReader->readLine();

				// check star magnitude
				mag = line.mid( 33, 4 ).toFloat();
				if ( mag > options->magLimitDrawStar ) {
					ready = true;
					break;
				}

				processSAO(&line);
			}  // end of while

		} else { //one of the star files could not be read.
			//maxSetMagnitude = starList.last()->mag();  // faintest star loaded (assumes stars are read in order of decreasing brightness)
			//For now, we return false if any star file had problems.  May want to start up anyway if at least one file is read.
			return false;
		}
		// magnitude level is reached
		if (ready == true) break;
	}

//Store the max set magnitude of current session. Will increase in KStarsData::appendNewData()
	maxSetMagnitude = options->magLimitDrawStar;
	delete saoFileReader;
	saoFileReader = 0;
	return true;
}

void KStarsData::processSAO(QString *line, bool reloadedData) {
	QString name, gname, SpType;
	int rah, ram, ras, dd, dm, ds;
	QChar sgn;
	float mag;

	name = ""; gname = "";

	rah = line->mid( 0, 2 ).toInt();
	ram = line->mid( 2, 2 ).toInt();
	ras = int(line->mid( 4, 6 ).toDouble() + 0.5); //add 0.5 to make int() pick nearest integer

	sgn = line->at(17);
	dd = line->mid(18, 2).toInt();
	dm = line->mid(20, 2).toInt();
	ds = int(line->mid(22, 5).toDouble() + 0.5); //add 0.5 to make int() pick nearest integer
	// star magnitude
	mag = line->mid( 33, 4 ).toFloat();

	SpType = line->mid(37, 2);
	name = line->mid(40 ).stripWhiteSpace(); //the rest of the line
	if (name.contains( ':' )) { //genetive form exists
		gname = name.mid( name.find(':')+1 );
		name = name.mid( 0, name.find(':') ).stripWhiteSpace();
	}

	// HEV: look up star name in internationalization filesource
	name = i18n("star name", name.local8Bit().data());

	bool starIsUnnamed = false;
	if (name.isEmpty()) {
		name = "star";
		starIsUnnamed = true;
	}

	dms r;
	r.setH(rah, ram, ras);
	dms d(dd, dm, ds);

	if (sgn == "-") { d.setD( -1.0*d.Degrees() ); }

	StarObject *o = new StarObject(r, d, mag, name, gname, SpType);
	starList.append(o);

	// add named stars to list
	if (starIsUnnamed == false) {
		ObjNames.append(o);
	}
}

bool KStarsData::readAsteroidData( void ) {
	QFile file;

	if ( KSUtils::openDataFile( file, "asteroids.dat" ) ) {
		KSFileReader fileReader( file );

		while( fileReader.hasMoreLines() ) {
			QString line, name;
			int mJD;
			double a, e, dble_i, dble_w, dble_N, dble_M, H;
			long double JD;
			KSAsteroid *ast = 0;

			line = fileReader.readLine();
			name = line.mid( 6, 17 ).stripWhiteSpace();
			mJD  = line.mid( 24, 5 ).toInt();
			a    = line.mid( 31, 9 ).toDouble();
			e    = line.mid( 41, 10 ).toDouble();
			dble_i = line.mid( 52, 9 ).toDouble();
			dble_w = line.mid( 62, 9 ).toDouble();
			dble_N = line.mid( 72, 9 ).toDouble();
			dble_M = line.mid( 82, 11 ).toDouble();
			H = line.mid( 94, 5 ).toDouble();

			JD = double( mJD ) + 2400000.5;

			ast = new KSAsteroid( this, name, "", JD, a, e, dms(dble_i), dms(dble_w), dms(dble_N), dms(dble_M), H );
			ast->setAngSize( 0.001 );
			asteroidList.append( ast );
			ObjNames.append( ast );
		}

		if ( asteroidList.count() ) return true;
	}

	return false;
}

bool KStarsData::readCometData( void ) {
	QFile file;

	if ( KSUtils::openDataFile( file, "comets.dat" ) ) {
		KSFileReader fileReader( file );

		while( fileReader.hasMoreLines() ) {
			QString line, name;
			int mJD;
			double q, e, dble_i, dble_w, dble_N, Tp;
			long double JD;
			KSComet *com = 0;

			line = fileReader.readLine();
			name = line.mid( 3, 35 ).stripWhiteSpace();
			mJD  = line.mid( 38, 5 ).toInt();
			q    = line.mid( 44, 10 ).toDouble();
			e    = line.mid( 55, 10 ).toDouble();
			dble_i = line.mid( 66, 9 ).toDouble();
			dble_w = line.mid( 76, 9 ).toDouble();
			dble_N = line.mid( 86, 9 ).toDouble();
			Tp = line.mid( 96, 14 ).toDouble();

			JD = double( mJD ) + 2400000.5;

			com = new KSComet( this, name, "", JD, q, e, dms(dble_i), dms(dble_w), dms(dble_N), Tp );
			com->setAngSize( 0.001 );

			cometList.append( com );
			ObjNames.append( com );
		}

		if ( cometList.count() ) return true;
	}

	return false;
}


//02/2003: NEW: split data files, using Heiko's new KSFileReader.
bool KStarsData::readDeepSkyData( void ) {
	QFile file;

	for ( unsigned int i=0; i<NNGCFILES; ++i ) {
		QString snum, fname;
		snum = QString().sprintf( "%02d", i+1 );
		fname = "ngcic" + snum + ".dat";

		if ( KSUtils::openDataFile( file, fname ) ) {
			KSFileReader fileReader( file ); // close file is included
			while ( fileReader.hasMoreLines() ) {
				QString line, con, ss, name, name2, longname;
				QString cat, cat2;
				float mag(1000.0), ras, a, b;
				int type, ingc, imess(-1), rah, ram, dd, dm, ds, pa;
				int pgc, ugc;
				QChar sgn, iflag;

				line = fileReader.readLine();

				iflag = line.at( 0 ); //check for IC catalog flag
				if ( iflag == 'I' ) cat = "IC";
				else if ( iflag == ' ' ) cat = "NGC"; // if blank, set catalog to NGC

				ingc = line.mid( 1, 4 ).toInt();  // NGC/IC catalog number
				if ( ingc==0 ) cat = ""; //object is not in NGC or IC catalogs

				con = line.mid( 6, 3 );     // constellation
				rah = line.mid( 10, 2 ).toInt();
				ram = line.mid( 13, 2 ).toInt();
				ras = line.mid( 16, 4 ).toFloat();
				sgn = line.at( 21 );
				dd = line.mid( 22, 2 ).toInt();
				dm = line.mid( 25, 2 ).toInt();
				ds = line.mid( 28, 2 ).toInt();
				type = line.mid( 31, 1 ).toInt();
				//Skipping detailed type string for now (it's mid(33,4) )
				ss = line.mid( 38, 4 );
			  if (ss == "    " ) { mag = 99.9; } else { mag = ss.toFloat(); }
				ss = line.mid( 43, 6 );
				if (ss == "      " ) { a = 0.0; } else { a = ss.toFloat(); }
				ss = line.mid( 50, 5 );
				if (ss == "     " ) { b = 0.0; } else { b = ss.toFloat(); }
				ss = line.mid( 56, 3 );
				if (ss == "   " ) { pa = 0; } else { pa = ss.toInt(); }
				ss = line.mid( 60, 3 );
				if (ss != "   " ) { //Messier object
					cat2 = cat;
					if ( ingc==0 ) cat2 = "";
					cat = "M";
					imess = ss.toInt();
				}
				ss = line.mid( 64, 6 );
				if (ss == "      " ) { pgc = 0; } else { pgc = ss.toInt(); }
				ss = line.mid( 71, 5 );
				if (ss == "     " ) { ugc = 0; } else { ugc = ss.toInt(); }
				longname = line.mid( 77, line.length() ).stripWhiteSpace();

				dms r;
				r.setH( rah, ram, int(ras) );
				dms d( dd, dm, ds );

				if ( sgn == "-" ) { d.setD( -1.0*d.Degrees() ); }

//				QString snum;
				if ( cat=="IC" || cat=="NGC" ) {
					snum.setNum( ingc );
					name = cat + " " + snum;
				} else if ( cat=="M" ) {
					snum.setNum( imess );
					name = cat + " " + snum;
					if ( cat2=="NGC" ) {
						snum.setNum( ingc );
						name2 = cat2 + " " + snum;
					} else if ( cat2=="IC" ) {
						snum.setNum( ingc );
						name2 = cat2 + " " + snum;
					} else {
						name2 = "";
					}
				} else {
					if ( longname.isEmpty() ) name = i18n( "Unnamed Object" );
					else name = longname;
				}

				// create new starobject or skyobject
				// type 0 and 1 are starobjects
				// all other are skyobjects
				SkyObject *o = 0;
				if (type < 2) {
					if ( type==0 ) type = 1; //Make sure we use CATALOG_STAR, not STAR
					o = new StarObject( type, r, d, mag, name, name2, longname, cat, a, b, pa, pgc, ugc );
				} else {
					o = new SkyObject( type, r, d, mag, name, name2, longname, cat, a, b, pa, pgc, ugc );
				}

				// keep object in deep sky objects' list
				deepSkyList.append( o );
				// plus: keep objects separated for performance reasons. Switching the colors during
				// paint is too expensive.
				if ( o->isCatalogM()) {
					deepSkyListMessier.append( o );
				} else if (o->isCatalogNGC() ) {
					deepSkyListNGC.append( o );
				} else if ( o->isCatalogIC() ) {
					deepSkyListIC.append( o );
				} else {
					deepSkyListOther.append( o );
				}

				// list of object names
				ObjNames.append( o );

				//Add longname to objList, unless longname is the same as name
				if ( !o->longname().isEmpty() && o->name() != o->longname() && o->name() != i18n( "star" ) ) {
					ObjNames.append( o, true );  // append with longname
				}
			} //end while-filereader
		} else { //one of the files could not be opened
			return false;
		}
	} //end for-loop through files

	return true;
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
		if ( file.open( IO_ReadOnly ) )
			fileFound = true;
		else
			if ( KSUtils::openDataFile( file, urlfile ) ) {
				if ( locale->language() != "en_US" ) kdDebug() << i18n( "No localized URL file; using defaul English file." ) << endl;
				// we found urlfile, we need to copy it to locale
				localeFile.setName( locateLocal( "appdata", urlfile ) );
				if (localeFile.open(IO_WriteOnly)) {
				QTextStream readStream(&file);
				QTextStream writeStream(&localeFile);
				writeStream <<  readStream.read();
				localeFile.close();
				file.reset();
			} else
				kdDebug() << i18n( "Failed to copy default URL file to locale directory, modifying default object links is not possible" ) << endl;
			fileFound = true;
		}
	}
	return fileFound;
}

bool KStarsData::readUserLog(void)
{
	QFile file;
	QString buffer;
	QString sub, name, data;

	if (!KSUtils::openDataFile( file, "userlog.dat" )) return false;

	QTextStream stream(&file);

	if (!stream.eof()) buffer = stream.read();

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

		SkyObjectName *sonm = ObjNames.find(name);
		if (sonm == 0) {
			kdWarning() << k_funcinfo << name << " not found" << endl;
		} else {
			sonm->skyObject()->userLog = data;
		}

	} // end while
	file.close();
	return true;
}

bool KStarsData::readURLData( QString urlfile, int type ) {
	QFile file;
	if (!openURLFile(urlfile, file)) return false;

	QTextStream stream(&file);

	while ( !stream.eof() ) {
		QString line = stream.readLine();

		QString name = line.mid( 0, line.find(':') );
		QString sub = line.mid( line.find(':')+1 );
		QString title = sub.mid( 0, sub.find(':') );
		QString url = sub.mid( sub.find(':')+1 );

		SkyObjectName *sonm = ObjNames.find(name);
		if (sonm == 0) {
			kdWarning() << k_funcinfo << name << " not found" << endl;
		} else {
			if ( type==0 ) { //image URL
				sonm->skyObject()->ImageList.append( url );
				sonm->skyObject()->ImageTitle.append( title );
			} else if ( type==1 ) { //info URL
				  sonm->skyObject()->InfoList.append( url );
					sonm->skyObject()->InfoTitle.append( title );
			}
		}

	}
	file.close();
	return true;
}

bool KStarsData::readCustomData( QString filename, QPtrList<SkyObject> &objList, bool showerrs ) {
	bool checkValue;
	bool badLine(false);
	int countValidLines(0);
	int countLine(0);

	int iType(0);
	double RA(0.0), Dec(0.0), mag(0.0);
	QString name(""); QString lname(""); QString spType("");

//If the filename begins with "~", replace the "~" with the user's home directory
//(otherwise, the file will not successfully open)
	if ( filename.at(0)=='~' ) filename = QDir::homeDirPath() + filename.mid( 1, filename.length() );

	QFile test( filename );
	QStringList errs;

	if ( test.open( IO_ReadOnly ) ) {
		QTextStream stream( &test );
		while ( !stream.eof() ) {
			QString sub, msg;
			QString line = stream.readLine();
			++countLine;
			iType = -1; //initialize to invalid values
			RA = 0.0; Dec = 0.0; mag = 0.0;
			name = ""; lname = ""; spType = "";

			if ( line.left(1) != "#" ) { //ignore commented lines
				QStringList fields = QStringList::split( " ", line ); //parse the line
				if ( fields.count() < 5 ) { //every valid line has at least 5 fields
					badLine = true;
					if (showerrs) errs.append( i18n( "Line " ) + QString( "%1" ).arg( countLine ) +
						i18n( " does not have enough fields." ) );
				} else {
					iType = (*fields.at(0)).toInt( &checkValue );
					if ( !checkValue ) {
						badLine = true;
						iType = -1;
						if (showerrs) errs.append( i18n( "Line " ) + QString( "%1" ).arg( countLine ) +
							i18n( ": field 1 is not an integer." ) );
					}
					RA = (*fields.at(1)).toDouble( &checkValue );
					if ( !checkValue ) {
						badLine = true;
						if (showerrs) errs.append( i18n( "Line " ) + QString( "%1" ).arg( countLine ) +
							i18n( ": field 2 is not a float (right ascension)." ) );
					}
					Dec = (*fields.at(2)).toDouble( &checkValue );
					if ( !checkValue ) {
						badLine = true;
						if (showerrs) errs.append( i18n( "Line " ) + QString( "%1" ).arg( countLine ) +
							i18n( ": field 3 is not a float (declination)." ) );
					}
					mag = (*fields.at(3)).toDouble( &checkValue );
					if ( !checkValue ) {
						badLine = true;
						if (showerrs) errs.append( i18n( "Line " ) + QString( "%1" ).arg( countLine ) +
							i18n( ": field 4 is not a float (magnitude)." ) );
					}

					if ( iType==0 || iType==1 ) {  //Star
						spType = (*fields.at(4));
						if ( !( spType.left(1)=="O" || spType.left(1)=="B" ||
								spType.left(1)=="A" || spType.left(1)=="F" ||
								spType.left(1)=="G" || spType.left(1)=="K" ||
								spType.left(1)=="M" ) ) { //invalid spType
							badLine = true;
							if (showerrs) errs.append( i18n( "Line " ) + QString( "%1" ).arg( countLine ) +
								i18n( ", field 5: invalid spectral type (must start with O,B,A,F,G,K or M)" ) );
						}
					} else if ( iType < 3 || iType > 8 ) { //invalid object type
						badLine = true;
						if (showerrs) errs.append( i18n( "Line " ) + QString( "%1" ).arg( countLine ) +
							i18n( ", field 1: invalid object type (must be 0,1,3,4,5,6,7 or 8)" ) );
					}
				}

				if ( !badLine ) { //valid data found!  add to objList
					++countValidLines;
					unsigned int Mark(4); //field marker; 5 for stars, 4 for deep-sky

					//Stars:
					if ( iType==0 || iType==1 ) Mark = 5;

					//First, check for name:
					if ( fields.count() > Mark && (*fields.at(Mark)).left(1)=="\"" ) {
						//The name is (probably) more than one word...
						name = (*fields.at(Mark)).mid(1, (*fields.at(Mark)).length()); //remove leading quote mark
						if (name.right(1)=="\"") { //name was one word in quotes...
							name = name.left( name.length() -1 ); //remove trailing quote mark
						} else {
							for ( unsigned int i=Mark+1; i<fields.count(); ++i ) {
								name = name + " " + (*fields.at(i));
								if (name.right(1)=="\"") {
									name = name.left( name.length() -1 ); //remove trailing quote mark
									break;
								}
							}
						}
					} else if ( fields.count() > Mark ) { //one-word name
						name = (*fields.at(Mark));
					}

					if ( Mark==5 ) { //Stars...
						if ( name == "" ) name = i18n( "star" );

						StarObject *o = new StarObject( RA, Dec, mag, name, "", spType );
						objList.append( o );
					} else if ( iType > 2 && iType <= 8 ) { //Deep-sky objects...
						if ( name == "" ) name = i18n( "unnamed object" );

						SkyObject *o = new SkyObject( iType, RA, Dec, mag, name, "", "" );
						objList.append( o );
					}
				}
			}
		}
	} else {
		if (showerrs) errs.append( i18n( "Could not open custom data file: " ) + filename );
		kdWarning() << i18n( "Could not open custom data file: " ) << filename;
	}

	if ( countValidLines==countLine && countValidLines ) { //all lines were valid!
		return true;
	} else if ( countValidLines ) { //found some valid lines, some invalid lines.
		if ( showerrs ) {
			QString message( i18n( "Some lines could not be parsed in the specified file, see error messages below." ) + "\n" +
											i18n( "To reject the file, press Cancel. " ) +
											i18n( "To accept the file (ignoring unparsed lines), press Accept." ) );
			if ( KMessageBox::warningContinueCancelList( 0, message, errs,
					i18n( "Some lines in file were invalid" ), i18n( "Accept" ) )==KMessageBox::Continue ) {
				return true;
			} else {
				return false;
			}
			return true; //not showing errs; return true for parsed lines
		}
	} else {
		if ( showerrs ) {
			QString message( i18n( "No lines could be parsed from the specified file, see error messages below." ) );
			KStarsMessageBox::badCatalog( 0, message, errs,
					i18n( "No Valid Data Found in File" ) );
//			KMessageBox::warningContinueCancelList( 0, message, errs,
//							 i18n( "No valid data found in file" ), i18n( "Close" ) );
		}
		return false;
	}
	// all test passed
	return false;
}


bool KStarsData::processCity( QString& line ) {
	QString totalLine;
	QString name, province, country;
	QStringList fields;
	TimeZoneRule *TZrule;
	bool intCheck = true;
	char latsgn, lngsgn;
	int lngD, lngM, lngS;
	int latD, latM, latS;
	double TZ;
	float lng, lat;

	totalLine = line;

	// separate fields
	fields = QStringList::split( ":", line );

	for ( unsigned int i=0; i< fields.count(); ++i )
		fields[i] = fields[i].stripWhiteSpace();

	if ( fields.count() < 11 ) {
		kdDebug()<< i18n( "Cities.dat: Ran out of fields.  Line was:" ) <<endl;
		kdDebug()<< totalLine.local8Bit() <<endl;
		return false;
	} else if ( fields.count() < 12 ) {
		// allow old format (without TZ) for mycities.dat
		fields.append("");
		fields.append("--");
	} else if ( fields.count() < 13 ) {
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

		while ( !stream.eof() ) {
			QString line = stream.readLine().stripWhiteSpace();
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
void KStarsData::backupOptions()
{
	if ( 0 == oldOptions) {
		oldOptions = new KStarsOptions(false);
	}
	*oldOptions = *options;
}

void KStarsData::restoreOptions()
{
	if ( 0 == oldOptions ) {
		// this should not happen
		return;
	}
	*options = *oldOptions;
	delete oldOptions;
	oldOptions = 0;
}

void KStarsData::setMagnitude( float newMagnitude, bool forceReload ) {
// only reload data if not loaded yet
// if checkDataPumpAction() detects that new magnitude is higher than the
// loaded, it can force a reload
	if ( newMagnitude > maxSetMagnitude || forceReload ) {
		maxSetMagnitude = newMagnitude;  // store new highest magnitude level

		if (reloadingData() == false) {  // if not already reloading data
			source = new FileSource(this, newMagnitude);
			loader = new StarDataSink(this);
			connect(loader, SIGNAL(done()), this, SLOT(checkDataPumpAction()));
			connect(loader, SIGNAL(updateSkymap()), this, SLOT(updateSkymap()));
			connect(loader, SIGNAL(clearCache()), this, SLOT(sendClearCache()));
			// start reloading
			pump = new QDataPump (source, (QDataSink*) loader);
		}
	}
	// change current magnitude level in KStarsOptions
	options->setMagLimitDrawStar(newMagnitude);
}

void KStarsData::checkDataPumpAction() {
	// it will set to true if new data should be reloaded
	bool reloadMoreData = false;
	if (source != 0) {
		// check if a new reload must be started
		if (source->magnitude() < maxSetMagnitude) reloadMoreData = true;
		delete source;
		source = 0;
	}
	if (pump != 0) {  // if pump exists
		delete pump;
		pump = 0;
	}
	if (loader != 0) {  // if loader exists
		delete loader;
		loader = 0;
	}
	// If magnitude was changed while reloading data start a new reload of data.
	if (reloadMoreData == true) {
		setMagnitude(maxSetMagnitude, true);
	}
}

bool KStarsData::reloadingData() {
	return ( pump || loader || source );  // true if variables != 0
}

void KStarsData::updateSkymap() {
	emit update();
}

void KStarsData::sendClearCache() {
	emit clearCache();
}

void KStarsData::addCatalog( QString name, QPtrList<SkyObject> oList ) {
	CustomCatalogs[ name ] = oList;
	CustomCatalogs[ name ].setAutoDelete( TRUE );
}

void KStarsData::initialize() {
	if (startupComplete) return;

	initTimer = new QTimer;
	QObject::connect(initTimer, SIGNAL(timeout()), this, SLOT( slotInitialize() ) );
	initTimer->start(1);
	initCounter = 0;
}

void KStarsData::initError(QString s, bool required = false) {
	QString message, caption;
	initTimer->stop();
	if (required) {
		message = i18n( "The file %1 could not be found.\n"
				"Shutting down, because KStars cannot run properly without this file.\n"
				"Place it in one of the following locations, then try again:\n\n"
				"\t$(KDEDIR)/share/apps/kstars/%1\n"
				"\t~/.kde/share/apps/kstars/%1" ).arg(s);
		caption = i18n( "Critical File Not Found: %1" ).arg(s);
	} else {
		message = i18n( "The file %1 could not be found.\n"
				"KStars can still run without this file, so will continue.\n"
				"However, to avoid seeing this message in the future, you may want to\n"
				"place the file in one of the following locations:\n\n"
				"\t$(KDEDIR)/share/apps/kstars/%1"
				"\n\t~/.kde/share/apps/kstars/%1" ).arg(s);
		caption = i18n( "Non-Critical File Not Found: %1" ).arg(s);
	}

	KMessageBox::information( 0, message, caption );

	if (required) {
		delete initTimer;
                initTimer = 0L;
		emit initFinished(false);
	} else {
		initTimer->start(1);
	}

}

void KStarsData::slotInitialize() {
	QFile imFile;
	QString ImageName;

	kapp->flush(); // flush all paint events before loading data

	switch ( initCounter )
	{
		case 0: //Load Options//
			emit progressText( i18n("Loading Options") );

			if (objects==1) {
				// timezone rules
				if ( !readTimeZoneRulebook( ) )
					initError( "TZrules.dat", true );
			}

			loadOptions();

			// read INDI hosts file, not required
			readINDIHosts();
			break;

		case 1: //Load Cities//
			if (objects>1) break;

			emit progressText( i18n("Loading City Data") );

			if ( !readCityData( ) )
				initError( "Cities.dat", true );
			break;

		case 2: //Load SAO stellar database//

			emit progressText(i18n("Loading Star Data" ) );
			if ( !readStarData( ) )
				initError( "saoN.dat", true );
			if (!readVARData())
				initError( "valaav.dat", false);
			if (!readADVTreeData())
				initError( "advinterface.dat", false);
			break;

		case 3: //Load NGC 2000 database//

			emit progressText( i18n("Loading NGC/IC Data" ) );
			if ( !readDeepSkyData( ) )
				initError( "ngcicN.dat", true );
			break;

		case 4: //Load Constellation lines//

			emit progressText( i18n("Loading Constellations" ) );
			if ( !readCLineData( ) )
				initError( "clines.dat", true );
			break;

		case 5: //Load Constellation names//

			emit progressText( i18n("Loading Constellation Names" ) );
			if ( !readCNameData( ) )
				initError( cnameFile, true );
			break;

		case 6: //Load Milky Way//

			emit progressText( i18n("Loading Milky Way" ) );
			if ( !readMWData( ) )
				initError( "mw*.dat", true );
			break;

		case 7: //Initialize the Planets//

			emit progressText( i18n("Creating Planets" ) );
			if (PC->initialize())
				PC->addObject( ObjNames );

			jmoons = new JupiterMoons();
			break;

		case 8: //Initialize Asteroids & Comets//

			emit progressText( i18n( "Creating Asteroids and Comets" ) );
			if ( !readAsteroidData() )
				initError( "asteroids.dat", false );
			if ( !readCometData() )
				initError( "comets.dat", false );

			break;

		case 9: //Initialize the Moon//

			emit progressText( i18n("Creating Moon" ) );
			Moon = new KSMoon(this);
			Moon->setAngSize( 30.0 );
			ObjNames.append( Moon );
			Moon->loadData();
			break;

		case 10: //Load Image URLs//

			emit progressText( i18n("Loading Image URLs" ) );
			if ( !readURLData( "image_url.dat", 0 ) ) {
				initError( "image_url.dat", false );
			}
			if ( !readURLData( "myimage_url.dat", 0 ) ) {
			//Don't do anything if the local file is not found.
			}
          // doesn't belong here, only temp
             readUserLog();

			break;

		case 11: //Load Information URLs//

			emit progressText( i18n("Loading Information URLs" ) );
			if ( !readURLData( "info_url.dat", 1 ) ) {
				initError( "info_url.dat", false );
			}
			if ( !readURLData( "myinfo_url.dat", 1 ) ) {
			//Don't do anything if the local file is not found.
			}
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

void KStarsData::initGuides(KSNumbers *num)
{
	// Define the Celestial Equator
	for ( unsigned int i=0; i<NCIRCLE; ++i ) {
		SkyPoint *o = new SkyPoint( i*24./NCIRCLE, 0.0 );
		o->EquatorialToHorizontal( LST, geo()->lat() );
		Equator.append( o );
	}

  // Define the horizon.
  // Use the celestial Equator as a convenient starting point, but instead of RA and Dec,
  // interpret the coordinates as azimuth and altitude, and then convert to RA, dec.
  // The horizon will be redefined whenever the positions of sky objects are updated.
	dms temp( 0.0 );
	for (SkyPoint *point = Equator.first(); point; point = Equator.next()) {
		double sinlat, coslat, sindec, cosdec, sinAz, cosAz;
		double HARad;
		dms dec, HA, RA, Az;
		Az = dms(*(point->ra()));

		Az.SinCos( sinAz, cosAz );
		geo()->lat()->SinCos( sinlat, coslat );

		dec.setRadians( asin( coslat*cosAz ) );
		dec.SinCos( sindec, cosdec );

		HARad = acos( -1.0*(sinlat*sindec)/(coslat*cosdec) );
		if ( sinAz > 0.0 ) { HARad = 2.0*dms::PI - HARad; }
		HA.setRadians( HARad );
		RA = LST->Degrees() - HA.Degrees();

		SkyPoint *o = new SkyPoint( RA, dec );
		o->setAlt( 0.0 );
		o->setAz( Az );

		Horizon.append( o );

		//Define the Ecliptic (use the same ListIteration; interpret coordinates as Ecliptic long/lat)
		o = new SkyPoint( 0.0, 0.0 );
		o->setFromEcliptic( num->obliquity(), point->ra(), &temp );
		o->EquatorialToHorizontal( LST, geo()->lat() );
		Ecliptic.append( o );
	}
}

void KStarsData::resetToNewDST(const GeoLocation *geo, const bool automaticDSTchange) {
	// reset tzrules data with local time, timezone offset and time direction (forward or backward)
	// force a DST change with option true for 3. parameter
	geo->tzrule()->reset_with_ltime( LTime, geo->TZ0(), TimeRunsForward, automaticDSTchange );
	// reset next DST change time
	setNextDSTChange( KSUtils::UTtoJD( geo->tzrule()->nextDSTChange() ) );
	//reset LTime, because TZoffset has changed
	LTime = UTime.addSecs( int( 3600*geo->TZ() ) );
}

void KStarsData::updateTime( GeoLocation *geo, SkyMap *skymap, const bool automaticDSTchange ) {
	// sync times with clock
	UTime = Clock->UTC();
	LTime = UTime.addSecs( int( 3600*geo->TZ() ) );
	CurrentDate = Clock->JD();
	setLST();

	//Only check DST if (1) TZrule is not the empty rule, and (2) if we have crossed
	//the DST change date/time.
	if ( !geo->tzrule()->isEmptyRule() ) {
		if ( TimeRunsForward ) {
			// timedirection is forward
			// DST change happens if current date is bigger than next calculated dst change
			if ( CurrentDate > NextDSTChange ) resetToNewDST(geo, automaticDSTchange);
		} else {
			// timedirection is backward
			// DST change happens if current date is smaller than next calculated dst change
			if ( CurrentDate < NextDSTChange ) resetToNewDST(geo, automaticDSTchange);
		}
	}

	KSNumbers num(CurrentDate);

	bool needNewCoords = false;
	if ( fabs( CurrentDate - LastNumUpdate ) > 1.0 ) {
		//update time-dependent variables once per day
		needNewCoords = true;
		LastNumUpdate = CurrentDate;
	}

	// Update positions of objects, if necessary
	if ( fabs( CurrentDate - LastPlanetUpdate ) > 0.01 ) {
		LastPlanetUpdate = CurrentDate;

		if ( options->drawPlanets ) PC->findPosition(&num);

		//Asteroids
		if ( options->drawPlanets && options->drawAsteroids )
			for ( KSAsteroid *ast = asteroidList.first(); ast; ast = asteroidList.next() )
				ast->findPosition( &num, earth() );

		//Comets
		if ( options->drawPlanets && options->drawComets )
			for ( KSComet *com = cometList.first(); com; com = cometList.next() )
				com->findPosition( &num, earth() );

//		//Add a point to the planet trail if the centered object is a solar system body.
//		if ( isSolarSystem( skymap->focusObject() ) ) {
//			PlanetTrail.append( new SkyPoint(skymap->focusObject()->ra(), skymap->focusObject()->dec()) );
//
//			//Allow no more than 200 points in the trail
//			while ( PlanetTrail.count() > 400 )
//				PlanetTrail.removeFirst();
//		}

		//Recompute the Ecliptic
		if ( options->drawEcliptic ) {
			Ecliptic.clear();

			dms temp(0.0);
			for ( unsigned int i=0; i<Equator.count(); ++i ) {
				SkyPoint *o = new SkyPoint( 0.0, 0.0 );
				o->setFromEcliptic( num.obliquity(), Equator.at(i)->ra(), &temp );
				Ecliptic.append( o );
			}
		}
	}

	// Moon moves ~30 arcmin/hr, so update its position every minute.
	if ( fabs( CurrentDate - LastMoonUpdate ) > 0.00069444 ) {
		LastMoonUpdate = CurrentDate;
		if ( options->drawMoon ) {
			Moon->findPosition( &num, geo->lat(), LST );
			Moon->findPhase( PC->planetSun() );
		}

		//for now, update positions of Jupiter's moons here also
		if ( options->drawPlanets && options->drawJupiter )
			jmoons->findPosition( &num, (const KSPlanet*)PC->findByName("Jupiter"), PC->planetSun() );
	}

	//Update Alt/Az coordinates.  Timescale varies with zoom level
	//If Clock is in Manual Mode, always update. (?)
	if ( fabs( CurrentDate - LastSkyUpdate) > 0.25/options->ZoomFactor || Clock->isManualMode() ) {
		LastSkyUpdate = CurrentDate;

		//Recompute Alt, Az coords for all objects
		//Planets
		//This updates trails as well
		PC->EquatorialToHorizontal( LST, geo->lat() );

		jmoons->EquatorialToHorizontal( LST, geo->lat() );
		if ( options->drawMoon ) {
			Moon->EquatorialToHorizontal( LST, geo->lat() );
			if ( Moon->hasTrail() ) Moon->updateTrail( LST, geo->lat() );
		}

//		//Planet Trails
//		for( SkyPoint *p = PlanetTrail.first(); p; p = PlanetTrail.next() )
//			p->EquatorialToHorizontal( LST, geo->lat() );

		//Asteroids
		if ( options->drawAsteroids ) {
			for ( KSAsteroid *ast = asteroidList.first(); ast; ast = asteroidList.next() ) {
				ast->EquatorialToHorizontal( LST, geo->lat() );
				if ( ast->hasTrail() ) ast->updateTrail( LST, geo->lat() );
			}
		}

		//Comets
		if ( options->drawComets ) {
			for ( KSComet *com = cometList.first(); com; com = cometList.next() ) {
				com->EquatorialToHorizontal( LST, geo->lat() );
				if ( com->hasTrail() ) com->updateTrail( LST, geo->lat() );
			}
		}

		//Stars
		if ( options->drawSAO ) {
			for ( StarObject *star = starList.first(); star; star = starList.next() ) {
				if ( star->mag() > options->magLimitDrawStar ) break;
				if (needNewCoords) star->updateCoords( &num );
				star->EquatorialToHorizontal( LST, geo->lat() );
			}
		}

		//Deep-sky objects. Keep lists separated for performance reasons
    if ( options->drawMessier || options->drawMessImages ) {
  		for ( SkyObject *o = deepSkyListMessier.first(); o; o = deepSkyListMessier.next() ) {
				if (needNewCoords) o->updateCoords( &num );
				o->EquatorialToHorizontal( LST, geo->lat() );
      }
    }
    if ( options->drawNGC ) {
  		for ( SkyObject *o = deepSkyListNGC.first(); o; o = deepSkyListNGC.next() ) {
				if (needNewCoords) o->updateCoords( &num );
				o->EquatorialToHorizontal( LST, geo->lat() );
      }
    }
    if ( options->drawIC ) {
  		for ( SkyObject *o = deepSkyListIC.first(); o; o = deepSkyListIC.next() ) {
				if (needNewCoords) o->updateCoords( &num );
				o->EquatorialToHorizontal( LST, geo->lat() );
      }
    }
    if ( options->drawOther ) {
  		for ( SkyObject *o = deepSkyListOther.first(); o; o = deepSkyListOther.next() ) {
				if (needNewCoords) o->updateCoords( &num );
				o->EquatorialToHorizontal( LST, geo->lat() );
      }
    }
    /* old code was
		for ( SkyObject *o = deepSkyList.first(); o; o = deepSkyList.next() ) {
			bool update = false;
			if ( o->catalog()=="M" &&
					( options->drawMessier || options->drawMessImages ) ) update = true;
			else if ( o->catalog()== "NGC" && options->drawNGC ) update = true;
			else if ( o->catalog()== "IC" && options->drawIC ) update = true;
			else if ( options->drawOther ) update = true;

			if ( update ) {
				if (needNewCoords) o->updateCoords( &num );
				o->EquatorialToHorizontal( LST, geo->lat() );
			}
		}
    */

		//Custom Catalogs
		for ( unsigned int j=0; j<options->CatalogCount; ++j ) {
			QPtrList<SkyObject> cat = CustomCatalogs[ options->CatalogName[j] ];
			if ( options->drawCatalog[j] ) {
				for ( SkyObject *o = cat.first(); o; o = cat.next() ) {
					if (needNewCoords) o->updateCoords( &num );
					o->EquatorialToHorizontal( LST, geo->lat() );
				}
			}
		}

		//Milky Way
		if ( options->drawMilkyWay ) {
			for ( unsigned int j=0; j<11; ++j ) {
				for ( SkyPoint *p = MilkyWay[j].first(); p; p = MilkyWay[j].next() ) {
					if (needNewCoords) p->updateCoords( &num );
					p->EquatorialToHorizontal( LST, geo->lat() );
				}
			}
		}

		//CLines
		if ( options->drawConstellLines ) {
			for ( SkyPoint *p = clineList.first(); p; p = clineList.next() ) {
				if (needNewCoords) p->updateCoords( &num );
				p->EquatorialToHorizontal( LST, geo->lat() );
			}
		}

		//CNames
		if ( options->drawConstellNames ) {
			for ( SkyPoint *p = cnameList.first(); p; p = cnameList.next() ) {
				if (needNewCoords) p->updateCoords( &num );
				p->EquatorialToHorizontal( LST, geo->lat() );
			}
		}

		//Celestial Equator
		if ( options->drawEquator ) {
			for ( SkyPoint *p = Equator.first(); p; p = Equator.next() ) {
				p->EquatorialToHorizontal( LST, geo->lat() );
			}
		}

		//Ecliptic
		if ( options->drawEcliptic ) {
			for ( SkyPoint *p = Ecliptic.first(); p; p = Ecliptic.next() ) {
				p->EquatorialToHorizontal( LST, geo->lat() );
			}
		}

		//Horizon: different than the others; Alt & Az remain constant, RA, Dec must keep up
		if ( options->drawHorizon || options->drawGround ) {
			for ( SkyPoint *p = Horizon.first(); p; p = Horizon.next() ) {
				p->HorizontalToEquatorial( LST, geo->lat() );
			}
		}

		//Update focus
		skymap->updateFocus();

		if ( Clock->isManualMode() )
			QTimer::singleShot( 0, skymap, SLOT( forceUpdateNow() ) );
		else skymap->forceUpdate();
	}
}

void KStarsData::setTimeDirection( float scale ) {
	TimeRunsForward = ( scale < 0 ? false : true );
}

void KStarsData::setFullTimeUpdate() {
			LastSkyUpdate = -1000000.0;
			LastPlanetUpdate = -1000000.0;
			LastMoonUpdate   = -1000000.0;
			LastNumUpdate = -1000000.0;
}

void KStarsData::setLST() {
	setLST( Clock->UTC() );
}

void KStarsData::setLST( QDateTime UTC ) {
	LST->set( KSUtils::UTtoLST( UTC, geo()->lng() ) );
}

void KStarsData::changeTime( QDate newDate, QTime newTime ) {
	QDateTime new_time(newDate, newTime);

	//Make sure Numbers, Moon, planets, and sky objects are updated immediately
	setFullTimeUpdate();

	// reset tzrules data with newlocal time and time direction (forward or backward)
	geo()->tzrule()->reset_with_ltime(new_time, geo()->TZ0(), isTimeRunningForward() );

	// reset next dst change time
	setNextDSTChange( KSUtils::UTtoJD( geo()->tzrule()->nextDSTChange() ) );

	//Turn off animated slews for the next time step.
	options->setSnapNextFocus();

	//Set the clock
	clock()->setUTC( new_time.addSecs( int(-3600 * geo()->TZ()) ) );

	//set local sideral time
	setLST();
}

bool KStarsData::isSolarSystem( SkyObject *o ) {
	if ( !o ) return false;
	return ( o->type() == 2 || o->type() == 9 || o->type() == 10 );
}

SkyObject* KStarsData::objectNamed( const QString &name ) {
	if ( (name== "star") || (name== "nothing") || name.isEmpty() ) return NULL;
	if ( name== Moon->name() ) return Moon;

	SkyObject *so = PC->findByName(name);

	if (so != 0)
		return so;

	//Stars
	for ( unsigned int i=0; i<starList.count(); ++i ) {
		if ( name==starList.at(i)->name() ) return starList.at(i);
	}

	//Deep sky objects
	for ( unsigned int i=0; i<deepSkyListMessier.count(); ++i ) {
		if ( name==deepSkyListMessier.at(i)->name() ) return deepSkyListMessier.at(i);
	}
	for ( unsigned int i=0; i<deepSkyListNGC.count(); ++i ) {
		if ( name==deepSkyListNGC.at(i)->name() ) return deepSkyListNGC.at(i);
	}
	for ( unsigned int i=0; i<deepSkyListIC.count(); ++i ) {
		if ( name==deepSkyListIC.at(i)->name() ) return deepSkyListIC.at(i);
	}
	for ( unsigned int i=0; i<deepSkyListOther.count(); ++i ) {
		if ( name==deepSkyListOther.at(i)->name() ) return deepSkyListOther.at(i);
	}

	//Constellations
	for ( unsigned int i=0; i<cnameList.count(); ++i ) {
		if ( name==cnameList.at(i)->name() ) return cnameList.at(i);
	}

	//reach here only if argument is not matched
	return NULL;
}

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
	if ( !f.open( IO_ReadOnly) ) {
		kdDebug() << i18n( "Could not open file %1" ).arg( f.name() ) << endl;
		return false;
	}

	QTextStream istream(&f);
	while ( ! istream.eof() ) {
		QString line = istream.readLine();

		//found a dcop line
		if ( line.left(4) == "dcop" ) {
			line = line.mid( 20 );  //strip away leading characters
			QStringList fn = QStringList::split( " ", line );

			if ( fn[0] == "lookTowards" && fn.count() >= 2 ) {
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
				if ( az >= 0.0 ) { map->setFocusAltAz( 15.0, az ); cmdCount++; }

				if ( arg == "z" || arg == "zenith" ) {
					map->setFocusAltAz( 90.0, map->focus()->az()->Degrees() );
					cmdCount++;
				}

				//try a named object.  name is everything after the first word (which is 'lookTowards')
				fn.remove( fn.first() );
				SkyObject *target = objectNamed( fn.join( " " ) );
				if ( target ) { map->setFocus( target ); cmdCount++; }

			} else if ( fn[0] == "setRaDec" && fn.count() == 3 ) {
				bool ok( false );
				dms r(0.0), d(0.0);

				ok = r.setFromString( fn[1], false ); //assume angle in hours
				if ( ok ) ok = d.setFromString( fn[2], true );  //assume angle in degrees
				if ( ok ) {
					map->setFocus( r, d );
					cmdCount++;
				}

			} else if ( fn[0] == "setAltAz" && fn.count() == 3 ) {
				bool ok( false );
				dms az(0.0), alt(0.0);

				ok = alt.setFromString( fn[1] );
				if ( ok ) ok = az.setFromString( fn[2] );
				if ( ok ) {
					map->setFocusAltAz( alt, az );
					cmdCount++;
				}

			} else if ( fn[0] == "zoom" && fn.count() == 2 ) {
				bool ok(false);
				double z = fn[1].toDouble(&ok);
				if ( ok ) {
					if ( z > MAXZOOM ) z = MAXZOOM;
					if ( z < MINZOOM ) z = MINZOOM;
					options->ZoomFactor = z;
					cmdCount++;
				}

			} else if ( fn[0] == "zoomIn" ) {
				if ( options->ZoomFactor < MAXZOOM ) {
					options->ZoomFactor *= DZOOM;
					cmdCount++;
				}
			} else if ( fn[0] == "zoomOut" ) {
				if ( options->ZoomFactor > MINZOOM ) {
					options->ZoomFactor /= DZOOM;
					cmdCount++;
				}
			} else if ( fn[0] == "defaultZoom" ) {
				options->ZoomFactor = DEFAULTZOOM;
				cmdCount++;
			} else if ( fn[0] == "setLocalTime" && fn.count() == 7 ) {
				bool ok(false);
				int yr(0), mth(0), day(0) ,hr(0), min(0), sec(0);
				yr = fn[1].toInt(&ok);
				if ( ok ) mth = fn[2].toInt(&ok);
				if ( ok ) day = fn[3].toInt(&ok);
				if ( ok ) hr  = fn[4].toInt(&ok);
				if ( ok ) min = fn[5].toInt(&ok);
				if ( ok ) sec = fn[6].toInt(&ok);
				if ( ok ) {
					changeTime( QDate(yr, mth, day), QTime(hr,min,sec));
					cmdCount++;
				} else {
					kdWarning() << i18n( "Could not set time: %1 / %2 / %3 ; %4:%5:%6" )
						.arg(day).arg(mth).arg(yr).arg(hr).arg(min).arg(sec) << endl;
				}
			} else if ( fn[0] == "changeViewOption" && fn.count() == 3 ) {
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

				if ( fn[1] == "TargetSymbol"    && nOk ) { options->targetSymbol    = nVal; cmdCount++; }
				if ( fn[1] == "ShowSAO"         && bOk ) { options->drawSAO         = bVal; cmdCount++; }
				if ( fn[1] == "ShowMess"        && bOk ) { options->drawMessier     = bVal; cmdCount++; }
				if ( fn[1] == "ShowMessImages"  && bOk ) { options->drawMessImages  = bVal; cmdCount++; }
				if ( fn[1] == "ShowNGC"         && bOk ) { options->drawNGC         = bVal; cmdCount++; }
				if ( fn[1] == "ShowIC"          && bOk ) { options->drawIC          = bVal; cmdCount++; }
				if ( fn[1] == "ShowCLines"      && bOk ) { options->drawConstellLines = bVal; cmdCount++; }
				if ( fn[1] == "ShowCNames"      && bOk ) { options->drawConstellNames = bVal; cmdCount++; }
				if ( fn[1] == "ShowMilkyWay"    && bOk ) { options->drawMilkyWay    = bVal; cmdCount++; }
				if ( fn[1] == "ShowGrid"        && bOk ) { options->drawGrid        = bVal; cmdCount++; }
				if ( fn[1] == "ShowEquator"     && bOk ) { options->drawEquator     = bVal; cmdCount++; }
				if ( fn[1] == "ShowEcliptic"    && bOk ) { options->drawEcliptic    = bVal; cmdCount++; }
				if ( fn[1] == "ShowHorizon"     && bOk ) { options->drawHorizon     = bVal; cmdCount++; }
				if ( fn[1] == "ShowGround"      && bOk ) { options->drawGround      = bVal; cmdCount++; }
				if ( fn[1] == "ShowSun"         && bOk ) { options->drawSun         = bVal; cmdCount++; }
				if ( fn[1] == "ShowMoon"        && bOk ) { options->drawMoon        = bVal; cmdCount++; }
				if ( fn[1] == "ShowMercury"     && bOk ) { options->drawMercury     = bVal; cmdCount++; }
				if ( fn[1] == "ShowVenus"       && bOk ) { options->drawVenus       = bVal; cmdCount++; }
				if ( fn[1] == "ShowMars"        && bOk ) { options->drawMars        = bVal; cmdCount++; }
				if ( fn[1] == "ShowJupiter"     && bOk ) { options->drawJupiter     = bVal; cmdCount++; }
				if ( fn[1] == "ShowSaturn"      && bOk ) { options->drawSaturn      = bVal; cmdCount++; }
				if ( fn[1] == "ShowUranus"      && bOk ) { options->drawUranus      = bVal; cmdCount++; }
				if ( fn[1] == "ShowNeptune"     && bOk ) { options->drawNeptune     = bVal; cmdCount++; }
				if ( fn[1] == "ShowPluto"       && bOk ) { options->drawPluto       = bVal; cmdCount++; }
				if ( fn[1] == "ShowAsteroids"   && bOk ) { options->drawAsteroids   = bVal; cmdCount++; }
				if ( fn[1] == "ShowComets"      && bOk ) { options->drawComets      = bVal; cmdCount++; }
				if ( fn[1] == "ShowPlanets"     && bOk ) { options->drawPlanets     = bVal; cmdCount++; }
				if ( fn[1] == "ShowDeepSky"     && bOk ) { options->drawDeepSky     = bVal; cmdCount++; }
				if ( fn[1] == "drawStarName"      && bOk ) { options->drawStarName      = bVal; cmdCount++; }
				if ( fn[1] == "drawStarMagnitude" && bOk ) { options->drawStarMagnitude = bVal; cmdCount++; }
				if ( fn[1] == "drawAsteroidName"  && bOk ) { options->drawAsteroidName  = bVal; cmdCount++; }
				if ( fn[1] == "drawCometName"     && bOk ) { options->drawCometName     = bVal; cmdCount++; }
				if ( fn[1] == "drawPlanetName"    && bOk ) { options->drawPlanetName    = bVal; cmdCount++; }
				if ( fn[1] == "drawPlanetImage"   && bOk ) { options->drawPlanetImage   = bVal; cmdCount++; }

				if ( fn[1] == "UseAltAz"         && bOk ) { options->useAltAz           = bVal; cmdCount++; }
				if ( fn[1] == "UseRefraction"    && bOk ) { options->useRefraction      = bVal; cmdCount++; }
				if ( fn[1] == "UseAutoLabel"     && bOk ) { options->useAutoLabel       = bVal; cmdCount++; }
				if ( fn[1] == "UseAutoTrail"     && bOk ) { options->useAutoTrail       = bVal; cmdCount++; }
				if ( fn[1] == "AnimateSlewing"   && bOk ) { options->useAnimatedSlewing = bVal; cmdCount++; }
				if ( fn[1] == "FadePlanetTrails" && bOk ) { options->fadePlanetTrails   = bVal; cmdCount++; }
				if ( fn[1] == "SlewTimeScale"    && dOk ) { options->slewTimeScale      = dVal; cmdCount++; }
				if ( fn[1] == "ZoomFactor"       && dOk ) { options->ZoomFactor         = dVal; cmdCount++; }
				if ( fn[1] == "magLimitDrawStar"     && dOk ) { options->magLimitDrawStar     = dVal; cmdCount++; }
				if ( fn[1] == "magLimitDrawStarInfo" && dOk ) { options->magLimitDrawStarInfo = dVal; cmdCount++; }
				if ( fn[1] == "magLimitHideStar"     && dOk ) { options->magLimitHideStar     = dVal; cmdCount++; }
				if ( fn[1] == "magLimitAsteroid"     && dOk ) { options->magLimitAsteroid     = dVal; cmdCount++; }
				if ( fn[1] == "magLimitAsteroidName" && dOk ) { options->magLimitAsteroidName = dVal; cmdCount++; }
				if ( fn[1] == "maxRadCometName"      && dOk ) { options->maxRadCometName      = dVal; cmdCount++; }

				//these three are a "radio group"
				if ( fn[1] == "UseLatinConstellationNames" && bOk ) {
					options->useLatinConstellNames = true;
					options->useLocalConstellNames = false;
					options->useAbbrevConstellNames = false;
					cmdCount++;
				}
				if ( fn[1] == "UseLocalConstellationNames" && bOk ) {
					options->useLatinConstellNames = false;
					options->useLocalConstellNames = true;
					options->useAbbrevConstellNames = false;
					cmdCount++;
				}
				if ( fn[1] == "UseAbbrevConstellationNames" && bOk ) {
					options->useLatinConstellNames = false;
					options->useLocalConstellNames = false;
					options->useAbbrevConstellNames = true;
					cmdCount++;
				}
			} else if ( fn[0] == "setGeoLocation" && ( fn.count() == 3 || fn.count() == 4 ) ) {
				QString city( fn[1] ), province( "" ), country( fn[2] );
				if ( fn.count() == 4 ) {
					province = fn[2];
					country = fn[3];
				}

				bool cityFound( false );
				for (GeoLocation *loc = geoList.first(); loc; loc = geoList.next()) {
					if ( loc->translatedName() == city &&
								( province.isEmpty() || loc->translatedProvince() == province ) &&
								loc->translatedCountry() == country ) {

						cityFound = true;
						options->setLocation( *loc );
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

#include "kstarsdata.moc"
