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
#include <qdir.h>
#include <qfile.h>

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
#include "ksnumbers.h"
#include "skypoint.h"
#include "skyobject.h"
#include "starobject.h"
#include "deepskyobject.h"
#include "skyobjectname.h"
#include "ksplanet.h"
#include "ksasteroid.h"
#include "kscomet.h"
#include "ksmoon.h"
#include "jupitermoons.h"

#include "simclock.h"
#include "timezonerule.h"
#include "filesource.h"
#include "stardatasink.h"
#include "ksfilereader.h"
#include "indidriver.h"
#include "indi/lilxml.h"
#include "indistd.h"
#include "detaildialog.h"
#include "csegment.h"
#include "customcatalog.h"

QPtrList<GeoLocation> KStarsData::geoList = QPtrList<GeoLocation>();
QMap<QString, TimeZoneRule> KStarsData::Rulebook = QMap<QString, TimeZoneRule>();
QStringList KStarsData::CustomColumns = QStringList::split( " ", "ID RA Dc Tp Nm Mg Mj Mn PA Ig" );
int KStarsData::objects = 0;

KStarsData::KStarsData() : stdDirs(0), locale(0), 
		LST(0), HourAngle(0),
		PCat(0), Moon(0), jmoons(0),
		starFileReader(0), initTimer(0),
		source(0), loader(0), pump(0)
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
	
	//Instantiate planet catalog
	PCat = new PlanetCatalog(this);

	//initialize FOV symbol
	fovSymbol = FOV();

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

	//Constellation lines are now pointers to existing StarObjects;
	//these are already auto-deleted by starList.
	clineList.setAutoDelete( FALSE );
	clineModeList.setAutoDelete( TRUE );
	cnameList.setAutoDelete( TRUE );
	csegmentList.setAutoDelete( TRUE );

	Equator.setAutoDelete( TRUE );
	Ecliptic.setAutoDelete( TRUE );
	Horizon.setAutoDelete( TRUE );
	for ( unsigned int i=0; i<11; ++i ) MilkyWay[i].setAutoDelete( TRUE );

	VariableStarsList.setAutoDelete(TRUE);
	INDIHostsList.setAutoDelete(TRUE);
	INDITelescopeList.setAutoDelete(TRUE);

	//Initialize object type strings
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

	// at startup times run forward
	setTimeDirection( 0.0 );

	//The StoredDate is used when saving user settings in a script; initialize to invalid date
	StoredDate.setDJD( (long double)INVALID_DAY );

	temporaryTrail = false;
}

KStarsData::~KStarsData() {
	objects--;
	checkDataPumpAction();

	// the list items do not need to be removed by hand.
	// all lists are set to AutoDelete = true

	delete stdDirs;
	delete Moon;
	delete locale;
	delete PCat;
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
  signed char c;
  LilXML *xmlParser = newLilXML();
  XMLEle *root = NULL;
  XMLAtt *ap;

  file.setName( locate( "appdata", indiFile ) );
	if ( file.name().isEmpty() || !file.open( IO_ReadOnly ) )
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

bool KStarsData::readCLineData( void ) {
	//The constellation lines data file (clines.dat) contains lists
	//of abbreviated genetive star names in the same format as they 
	//appear in the star data files (hipNNN.dat).  
	//
	//Each constellation consists of a QPtrList of SkyPoints, 
	//corresponding to the stars at each "node" of the constellation.
	//These are pointers to the starobjects themselves, so the nodes 
	//will automatically be fixed to the stars even as the star 
	//positions change due to proper motions.  In addition, each node 
	//has a corresponding flag that determines whether a line should 
	//connect this node and the previous one.
	
	QFile file;
	if ( KSUtils::openDataFile( file, "clines.dat" ) ) {
	  QTextStream stream( &file );

		while ( !stream.eof() ) {
			QString line, name;
			QChar *mode;

			line = stream.readLine();

			//ignore lines beginning with "#":
			if ( line.at( 0 ) != '#' ) {
				name = line.mid( 2 ).stripWhiteSpace();
				
				//Find the star with the same abbreviated genitive name ( name2() )
				//increase efficiency by searching the list of named objects, rather than the 
				//full list of all stars.  
				bool starFound( false );
				for ( SkyObjectName *oname = ObjNames.first(); oname; oname = ObjNames.next() ) {
					if ( oname->skyObject()->type() == SkyObject::STAR && 
							 oname->skyObject()->name2() == name ) {
						starFound = true;
						clineList.append( (SkyPoint *)( oname->skyObject() ) );
						
						mode = new QChar( line.at( 0 ) );
						clineModeList.append( mode );
						break;
					}
				}
				
				if ( ! starFound ) 
					kdWarning() << i18n( "No star named %1 found." ).arg(name) << endl;
			}
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

			SkyObject *o = new SkyObject( SkyObject::CONSTELLATION, r, d, 0.0, name, abbrev );
			cnameList.append( o );
			ObjNames.append( o );
		}
		file.close();

		return true;
	} else {
		return false;
	}
}

bool KStarsData::readCBoundData( void ) {
	QFile file;

	if ( KSUtils::openDataFile( file, "cbound.dat" ) ) {
		QTextStream stream( &file );

		unsigned int nn(0);
		double ra(0.0), dec(0.0);
		QString d1, d2;
		bool ok(false), comment(false);
		
		//read the stream one field at a time.  Individual segments can span
		//multiple lines, so our normal readLine() is less convenient here.
		//Read fields into strings and then attempt to recast them as ints 
		//or doubles..this lets us do error checking on the stream.
		while ( !stream.eof() ) {
			stream >> d1;
			if ( d1.at(0) == '#' ) { 
				comment = true; 
				ok = true; 
			} else {
				comment = false;
				nn = d1.toInt( &ok );
			}
			
			if ( !ok || comment ) {
				d1 = stream.readLine();
				
				if ( !ok ) 
					kdWarning() << i18n( "Unable to parse boundary segment." ) << endl;
				
			} else { 
				CSegment *seg = new CSegment();
				
				for ( unsigned int i=0; i<nn; ++i ) {
					stream >> d1 >> d2;
					ra = d1.toDouble( &ok );
					if ( ok ) dec = d2.toDouble( &ok );
					if ( !ok ) break;
					
					seg->addPoint( ra, dec );
				}
				
				if ( !ok ) {
					//uh oh, this entry was not parsed.  Skip to the next line.
					kdWarning() << i18n( "Unable to parse boundary segment." ) << endl;
					delete seg;
					d1 = stream.readLine();
					
				} else {
					stream >> d1; //this should always equal 2
					
					nn = d1.toInt( &ok );
					//error check
					if ( !ok || nn != 2 ) {
						kdWarning() << i18n( "Bad Constellation Boundary data." ) << endl;
						delete seg;
						d1 = stream.readLine();
					}
				}

				if ( ok ) {
					stream >> d1 >> d2;
					ok = seg->setNames( d1, d2 );
					if ( ok ) csegmentList.append( seg );
				}
			}
		}
		
		return true;
	} else {
		return false;
	}
}

bool KStarsData::openStarFile( int i ) {
	if (starFileReader != 0) delete starFileReader;
	QFile file;
	QString snum, fname;
	snum = QString().sprintf("%03d", i);
	fname = "hip" + snum + ".dat";
	if (KSUtils::openDataFile(file, fname)) {
		starFileReader = new KSFileReader(file); // close file is included
	} else {
		starFileReader = 0;
		return false;
	}
	return true;
}

bool KStarsData::readStarData( void ) {
	bool ready = false;

	float loadUntilMag = MINDRAWSTARMAG;
	if (Options::magLimitDrawStar() > loadUntilMag) loadUntilMag = Options::magLimitDrawStar();
	
	for (unsigned int i=1; i<NHIPFILES+1; ++i) {
		emit progressText( i18n( "Loading Star Data (%1%)" ).arg( int(100.*float(i)/float(NHIPFILES)) ) );
		
		if (openStarFile(i) == true) {
			while (starFileReader->hasMoreLines()) {
				QString line;
				float mag;

				line = starFileReader->readLine();
				if ( line.left(1) != "#" ) {  //ignore comments
					// check star magnitude
					mag = line.mid( 46,5 ).toFloat();
					if ( mag > loadUntilMag ) {
						ready = true;
						break;
					}

					processStar(&line);
				}
			}  // end of while

		} else { //one of the star files could not be read.
			if ( starList.count() ) return true;
			else return false;
		}
		// magnitude level is reached
		if (ready == true) break;
	}

//Store the max set magnitude of current session. Will increase in KStarsData::appendNewData()
	maxSetMagnitude = Options::magLimitDrawStar();
	delete starFileReader;
	starFileReader = 0;
	return true;
}

void KStarsData::processStar( QString *line, bool reloadMode ) {
	QString name, gname, SpType;
	int rah, ram, ras, ras2, dd, dm, ds, ds2;
	bool mult, var;
	QChar sgn;
	double mag, bv, dmag, vper;
	double pmra, pmdec, plx;

	name = ""; gname = "";

	//parse coordinates
	rah = line->mid( 0, 2 ).toInt();
	ram = line->mid( 2, 2 ).toInt();
	ras = int(line->mid( 4, 5 ).toDouble());
	ras2 = int(60.0*(line->mid( 4, 5 ).toDouble()-ras) + 0.5); //add 0.5 to get nearest integer with int()

	sgn = line->at(10);
	dd = line->mid(11, 2).toInt();
	dm = line->mid(13, 2).toInt();
	ds = int(line->mid(15, 4).toDouble());
	ds2 = int(60.0*(line->mid( 15, 5 ).toDouble()-ds) + 0.5); //add 0.5 to get nearest integer with int()

	//parse proper motion and parallax
	pmra = line->mid( 20, 9 ).toDouble();
	pmdec = line->mid( 29, 9 ).toDouble();
	plx = line->mid( 38, 7 ).toDouble();

	//parse magnitude, B-V color, and spectral type
	mag = line->mid( 46, 5 ).toDouble();
	bv  = line->mid( 51, 5 ).toDouble();
	SpType = line->mid(56, 2);

	//parse multiplicity
	mult = line->mid( 59, 1 ).toInt();

	//parse variablility...currently not using dmag or var
	var = false; dmag = 0.0; vper = 0.0;
	if ( line->at( 62 ) == '.' ) {
		var = true;
		dmag = line->mid( 61, 4 ).toDouble();
		vper = line->mid( 66, 6 ).toDouble();
	}

	//parse name(s)
	name = line->mid( 72 ).stripWhiteSpace(); //the rest of the line
	if (name.contains( ':' )) { //genetive form exists
		gname = name.mid( name.find(':')+1 ).stripWhiteSpace();
		name = name.mid( 0, name.find(':') ).stripWhiteSpace();
	}

	// HEV: look up star name in internationalization filesource
	name = i18n("star name", name.local8Bit().data());

	bool starIsUnnamed( false );
	if (name.isEmpty() && gname.isEmpty()) { //both names are empty
		starIsUnnamed = true;
	}
	
	dms r;
	r.setH(rah, ram, ras, ras2);
	dms d(dd, dm, ds, ds2);

	if (sgn == "-") { d.setD( -1.0*d.Degrees() ); }

	StarObject *o = new StarObject(r, d, mag, name, gname, SpType, pmra, pmdec, plx, mult, var );
	starList.append(o);
	
	// get horizontal coordinates when object will loaded while running the application
	// first run doesn't need this because updateTime() will called after loading all data
	if (reloadMode) {
		o->EquatorialToHorizontal( LST, geo()->lat() );
	}
	
	//STAR_SIZE
//	StarObject *p = new StarObject(r, d, mag, name, gname, SpType, pmra, pmdec, plx, mult, var );
//	starList.append(p);

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
			a    = line.mid( 30, 9 ).toDouble();
			e    = line.mid( 41, 10 ).toDouble();
			dble_i = line.mid( 52, 9 ).toDouble();
			dble_w = line.mid( 62, 9 ).toDouble();
			dble_N = line.mid( 72, 9 ).toDouble();
			dble_M = line.mid( 82, 11 ).toDouble();
			H = line.mid( 94, 5 ).toDouble();

			JD = double( mJD ) + 2400000.5;

			ast = new KSAsteroid( this, name, "", JD, a, e, dms(dble_i), dms(dble_w), dms(dble_N), dms(dble_M), H );
			ast->setAngularSize( 0.005 );
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
			com->setAngularSize( 0.005 );

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

		emit progressText( i18n( "Loading NGC/IC Data (%1%)" ).arg( int(100.*float(i)/float(NNGCFILES)) ) );

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
				//Ignore comment lines
				while ( line.at(0) == '#' && fileReader.hasMoreLines() ) line = fileReader.readLine();
				//Ignore lines with no coordinate values
				while ( line.mid(6,8).stripWhiteSpace().isEmpty() ) line = fileReader.readLine();
				
				iflag = line.at( 0 ); //check for NGC/IC catalog flag
				if ( iflag == 'I' ) cat = "IC";
				else if ( iflag == 'N' ) cat = "NGC";

				ingc = line.mid( 1, 4 ).toInt();  // NGC/IC catalog number
				if ( ingc==0 ) cat = ""; //object is not in NGC or IC catalogs

				//coordinates
				rah = line.mid( 6, 2 ).toInt();
				ram = line.mid( 8, 2 ).toInt();
				ras = line.mid( 10, 4 ).toFloat();
				sgn = line.at( 15 );
				dd = line.mid( 16, 2 ).toInt();
				dm = line.mid( 18, 2 ).toInt();
				ds = line.mid( 20, 2 ).toInt();

				//B magnitude
				ss = line.mid( 23, 4 );
			  if (ss == "    " ) { mag = 99.9; } else { mag = ss.toFloat(); }

				//object type
				type = line.mid( 29, 1 ).toInt();

				//major and minor axes
				ss = line.mid( 31, 5 );
				if (ss == "      " ) { a = 0.0; } else { a = ss.toFloat(); }
				ss = line.mid( 37, 5 );
				if (ss == "     " ) { b = 0.0; } else { b = ss.toFloat(); }
				//position angle.  The catalog PA is zero when the Major axis 
				//is horizontal.  But we want the angle measured from North, so 
				//we set PA = 90 - pa.
				ss = line.mid( 43, 3 );
				if (ss == "   " ) { pa = 90; } else { pa = 90 - ss.toInt(); }

				//PGC number
				ss = line.mid( 47, 6 );
				if (ss == "      " ) { pgc = 0; } else { pgc = ss.toInt(); }

				//UGC number
				if ( line.mid( 54, 3 ) == "UGC" ) {
					ugc = line.mid( 58, 5 ).toInt();
				} else {
					ugc = 0;
				}

				//Messier number
				if ( line.at( 70 ) == 'M' ) {
					cat2 = cat;
					if ( ingc==0 ) cat2 = "";
					cat = "M";
					imess = line.mid( 72, 3 ).toInt();
				}

				longname = line.mid( 76, line.length() ).stripWhiteSpace();

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

				// create new deepskyobject
				DeepSkyObject *o = 0;
				if ( type==0 ) type = 1; //Make sure we use CATALOG_STAR, not STAR
				o = new DeepSkyObject( type, r, d, mag, name, name2, longname, cat, a, b, pa, pgc, ugc );

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
				ObjNames.append( (SkyObject*)o );

				//Add longname to objList, unless longname is the same as name
				if ( !o->longname().isEmpty() && o->name() != o->longname() && o->hasName() ) {
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
		if ( file.open( IO_ReadOnly ) ) {
			//local file found.  Now, if global file has newer timestamp, then merge the two files.
			//First load local file into QStringList
			bool newDataFound( false );
			QStringList urlData;
			QTextStream lStream( &file );
			while ( ! lStream.eof() ) urlData.append( lStream.readLine() );

			//Find global file(s) in findAllResources() list.
			QFileInfo fi_local( file.name() );
			QStringList flist = KGlobal::instance()->dirs()->findAllResources( "appdata", urlfile );
			for ( unsigned int i=0; i< flist.count(); i++ ) {
				if ( flist[i] != file.name() ) {
					QFileInfo fi_global( flist[i] );

					//Is this global file newer than the local file?
					if ( fi_global.lastModified() > fi_local.lastModified() ) {
						//Global file has newer timestamp than local.  Add lines in global file that don't already exist in local file.
						//be smart about this; in some cases the URL is updated but the image is probably the same if its
						//label string is the same.  So only check strings up to last ":"
						QFile globalFile( flist[i] );
						if ( globalFile.open( IO_ReadOnly ) ) {
							QTextStream gStream( &globalFile );
							while ( ! gStream.eof() ) {
								QString line = gStream.readLine();

								//If global-file line begins with "XXX:" then this line should be removed from the local file.
								if ( line.left( 4 ) == "XXX:"  && urlData.contains( line.mid( 4 ) ) ) {
									urlData.remove( urlData.find( line.mid( 4 ) ) );
								} else {
									//does local file contain the current global file line, up to second ':' ?

									bool linefound( false );
									for ( unsigned int j=0; j< urlData.count(); ++j ) {
										if ( urlData[j].contains( line.left( line.find( ':', line.find( ':' ) + 1 ) ) ) ) {
											//replace line in urlData with its equivalent in the newer global file.
											urlData.remove( urlData.at(j) );
											urlData.insert( urlData.at(j), line );
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
				if ( file.open( IO_WriteOnly ) ) {
					QTextStream outStream( &file );
					for ( unsigned int i=0; i<urlData.count(); i++ ) {
						outStream << urlData[i] << endl;
					}
					file.close();
				}
			}

			if ( file.open( IO_ReadOnly ) ) fileFound = true;

		} else {
			if ( KSUtils::openDataFile( file, urlfile ) ) {
				if ( locale->language() != "en_US" ) kdDebug() << i18n( "No localized URL file; using default English file." ) << endl;
				// we found urlfile, we need to copy it to locale
				localeFile.setName( locateLocal( "appdata", urlfile ) );
				if (localeFile.open(IO_WriteOnly)) {
					QTextStream readStream(&file);
					QTextStream writeStream(&localeFile);
					while ( ! readStream.eof() ) {
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
		uint uiFirstNewline = sub.find("\n", startIndex + 9);
		name = sub.mid(startIndex + 9, sub.findRev( "]", uiFirstNewline ) - (startIndex + 9) );
		// Read data and skip new line
		data   = sub.mid( uiFirstNewline + 1, endIndex - (uiFirstNewline + 1));
		buffer = buffer.mid(endIndex + 11);

		//Find the sky object named 'name'.
		//Note that ObjectNameList::find() looks for the ascii representation 
		//of star genetive names, so stars are identified that way in the user log.
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

bool KStarsData::readURLData( QString urlfile, int type, bool deepOnly ) {
	QFile file;
	if (!openURLFile(urlfile, file)) return false;

	QTextStream stream(&file);

	while ( !stream.eof() ) {
		QString line = stream.readLine();

		//ignore comment lines
		if ( line.left(1) != "#" ) {
			QString name = line.mid( 0, line.find(':') );
			QString sub = line.mid( line.find(':')+1 );
			QString title = sub.mid( 0, sub.find(':') );
			QString url = sub.mid( sub.find(':')+1 );
	
			SkyObjectName *sonm = ObjNames.find(name);
			
			if (sonm == 0) {
				kdWarning() << k_funcinfo << name << " not found" << endl;
			} else {
				if ( ! deepOnly || ( sonm->skyObject()->type() > 2 && sonm->skyObject()->type() < 9 ) ) {
					if ( type==0 ) { //image URL
						sonm->skyObject()->ImageList.append( url );
						sonm->skyObject()->ImageTitle.append( title );
					} else if ( type==1 ) { //info URL
						sonm->skyObject()->InfoList.append( url );
						sonm->skyObject()->InfoTitle.append( title );
					}
				}
			}
		}
	}
	file.close();
	return true;
}

bool KStarsData::readCustomCatalogs() {
	bool result = false;

	for ( uint i=0; i < Options::catalogFile().count(); i++ ) {
		bool thisresult = addCatalog( Options::catalogFile()[i] );
		//Make result = true if at least one catalog is added
		result = ( result || thisresult );
	}
	return result;
}

bool KStarsData::addCatalog( QString filename ) {
	CustomCatalog *newCat = createCustomCatalog( filename, false );
	if ( newCat ) {
		CustomCatalogs.append( newCat );

		// add object names to ObjNames list
		for ( uint i=0; i < newCat->objList().count(); i++ ) {
//		for ( SkyObject *o=newCat->objList().first(); o; o = newCat->objList().next() ) {
			SkyObject *o = newCat->objList().at(i);
			ObjNames.append( o );
			if ( o->hasLongName() && o->longname() != o->name() ) 
				ObjNames.append( o, true ); //Add long name
			// PdV
			//if (reloadMode) {
		        //        o->EquatorialToHorizontal( LST, geo()->lat() );
			//}

		}

		return true;
	} else
		kdWarning() << k_funcinfo << i18n("Error adding catalog: %1").arg( filename ) << endl;

	return false;
}

bool KStarsData::removeCatalog( int i ) {
	if ( ! CustomCatalogs.at(i) ) return false;

	QPtrList<SkyObject> cat = CustomCatalogs.at(i)->objList();

	for ( SkyObject *o=cat.first(); o; o=cat.next() ) {
		ObjNames.remove( o->name() );
		if ( o->hasLongName() && o->longname() != o->name() ) 
			ObjNames.remove( o->longname() );
	}

	CustomCatalogs.remove(i);

	return true;
}

CustomCatalog* KStarsData::createCustomCatalog( QString filename, bool showerrs ) {
	QDir::setCurrent( QDir::homeDirPath() );  //for files with relative path
	QPtrList<SkyObject> objList;
	QString CatalogName, CatalogPrefix, CatalogColor;
	float CatalogEpoch;

	//If the filename begins with "~", replace the "~" with the user's home directory
	//(otherwise, the file will not successfully open)
	if ( filename.at(0)=='~' )
		filename = QDir::homeDirPath() + filename.mid( 1, filename.length() );
	QFile ccFile( filename );

	if ( ccFile.open( IO_ReadOnly ) ) {
		int iStart(0); //the line number of the first non-header line
		QStringList errs; //list of error messages 
		QStringList Columns; //list of data column descriptors in the header

		QTextStream stream( &ccFile );
		QStringList lines = QStringList::split( "\n", stream.read() );

		if ( parseCustomDataHeader( lines, Columns, CatalogName, CatalogPrefix, 
				CatalogColor, CatalogEpoch, iStart, showerrs, errs ) ) {
	
			QStringList::Iterator it = lines.begin();
			QStringList::Iterator itEnd  = lines.end();
			it += iStart; //jump ahead past header
		
			for ( uint i=iStart; i < lines.count(); i++ ) {
				QStringList d = QStringList::split( " ", lines[i] );
	
				//Now, if one of the columns is the "Name" field, the name may contain spaces.
				//In this case, the name field will need to be surrounded by quotes.  
				//Check for this, and adjust the d list accordingly
				int iname = Columns.findIndex( "Nm" );
				if ( iname >= 0 && d[iname].left(1) == "\"" ) { //multi-word name in quotes
					d[iname] = d[iname].mid(1); //remove leading quote
					//It's possible that the name is one word, but still in quotes
					if ( d[iname].right(1) == "\"" ) {
						d[iname] = d[iname].left( d[iname].length() - 1 );
					} else {
						int iend = iname + 1;
						while ( d[iend].right(1) != "\"" ) {
							d[iname] += " " + d[iend];
							++iend;
						}
						d[iname] += " " + d[iend].left( d[iend].length() - 1 );
	
						//remove the entries from d list that were the multiple words in the name
						for ( int j=iname+1; j<=iend; j++ ) {
							d.remove( d.at(iname + 1) ); //index is *not* j, because next item becomes "iname+1" after remove
						}
					}
				}
	
				if ( d.count() == Columns.count() ) {
					processCustomDataLine( i, d, Columns, CatalogPrefix, objList, showerrs, errs );
				} else {
					if ( showerrs ) errs.append( i18n( "Line %1 does not contain %2 fields.  Skipping it." ).arg( i ).arg( Columns.count() ) );
				}
			}
		}

		if ( objList.count() ) {
			if ( errs.count() > 0 ) { //some data parsed, but there are errs to report
				QString message( i18n( "Some lines in the custom catalog could not be parsed; see error messages below." ) + "\n" +
												i18n( "To reject the file, press Cancel. " ) +
												i18n( "To accept the file (ignoring unparsed lines), press Accept." ) );
				if ( KMessageBox::warningContinueCancelList( 0, message, errs,
						i18n( "Some Lines in File Were Invalid" ), i18n( "Accept" ) ) != KMessageBox::Continue ) {
					return 0; //User pressed Cancel, return NULL pointer
				}
			}
		} else { //objList.count() == 0
			if ( showerrs ) {
				QString message( i18n( "No lines could be parsed from the specified file, see error messages below." ) );
				KMessageBox::informationList( 0, message, errs,
						i18n( "No Valid Data Found in File" ) );
			}
			return 0; //no valid catalog data parsed, return NULL pointer
		}

	} else { //Error opening catalog file
		if ( showerrs )
			KMessageBox::sorry( 0, i18n( "Could not open custom data file: %1" ).arg( filename ), 
					i18n( "Error opening file" ) );
		else 
			kdDebug() << i18n( "Could not open custom data file: %1" ).arg( filename ) << endl;
	}

	//Return the catalog
	if ( objList.count() )
		return new CustomCatalog( CatalogName, CatalogPrefix, CatalogColor, CatalogEpoch, objList );
	else
		return 0;
}


bool KStarsData::processCustomDataLine( int lnum, QStringList d, QStringList Columns, 
		QString Prefix, QPtrList<SkyObject> &objList, bool showerrs, QStringList &errs ) {

	//object data
	unsigned char iType(0);
	dms RA, Dec;
	float mag(0.0), a(0.0), b(0.0), PA(0.0);
	QString name(""); QString lname("");

	for ( uint i=0; i<Columns.count(); i++ ) {
		if ( Columns[i] == "ID" ) 
			name = Prefix + " " + d[i];

		if ( Columns[i] == "Nm" )
			lname = d[i];

		if ( Columns[i] == "RA" ) {
			if ( ! RA.setFromString( d[i], false ) ) {
				if ( showerrs ) 
					errs.append( i18n( "Line %1, field %2: Unable to parse RA value: %3" )
							.arg(lnum).arg(i).arg(d[i]) );
				return false;
			}
		}

		if ( Columns[i] == "Dc" ) {
			if ( ! Dec.setFromString( d[i], true ) ) {
				if ( showerrs )
					errs.append( i18n( "Line %1, field %2: Unable to parse Dec value: %3" )
							.arg(lnum).arg(i).arg(d[i]) );
				return false;
			}
		}

		if ( Columns[i] == "Tp" ) {
			bool ok(false);
			iType = d[i].toUInt( &ok );
			if ( ok ) {
				if ( iType == 2 || iType > 8 ) {
					if ( showerrs )
						errs.append( i18n( "Line %1, field %2: Invalid object type: %3" )
								.arg(lnum).arg(i).arg(d[i]) +
								i18n( "Must be one of 0, 1, 3, 4, 5, 6, 7, 8." ) );
					return false;
				}
			} else {
				if ( showerrs )
					errs.append( i18n( "Line %1, field %2: Unable to parse Object type: %3" )
							.arg(lnum).arg(i).arg(d[i]) );
				return false;
			}
		}

		if ( Columns[i] == "Mg" ) {
			bool ok(false);
			mag = d[i].toFloat( &ok );
			if ( ! ok ) {
				if ( showerrs )
					errs.append( i18n( "Line %1, field %2: Unable to parse Magnitude: %3" )
							.arg(lnum).arg(i).arg(d[i]) );
				return false;
			}
		}

		if ( Columns[i] == "Mj" ) {
			bool ok(false);
			a = d[i].toFloat( &ok );
			if ( ! ok ) {
				if ( showerrs )
					errs.append( i18n( "Line %1, field %2: Unable to parse Major Axis: %3" )
							.arg(lnum).arg(i).arg(d[i]) );
				return false;
			}
		}

		if ( Columns[i] == "Mn" ) {
			bool ok(false);
			b = d[i].toFloat( &ok );
			if ( ! ok ) {
				if ( showerrs )
					errs.append( i18n( "Line %1, field %2: Unable to parse Minor Axis: %3" )
							.arg(lnum).arg(i).arg(d[i]) );
				return false;
			}
		}

		if ( Columns[i] == "PA" ) {
			bool ok(false);
			PA = d[i].toFloat( &ok );
			if ( ! ok ) {
				if ( showerrs )
					errs.append( i18n( "Line %1, field %2: Unable to parse Position Angle: %3" )
							.arg(lnum).arg(i).arg(d[i]) );
				return false;
			}
		}
	}

	if ( iType == 0 ) { //Add a star
		StarObject *o = new StarObject( RA, Dec, mag, lname );
		objList.append( o );
	} else { //Add a deep-sky object
		DeepSkyObject *o = new DeepSkyObject( iType, RA, Dec, mag, 
					name, "", lname, Prefix, a, b, PA );
		objList.append( o );
	}

	return true;
}

bool KStarsData::parseCustomDataHeader( QStringList lines, QStringList &Columns, 
			QString &CatalogName, QString &CatalogPrefix, QString &CatalogColor, float &CatalogEpoch,
			int &iStart, bool showerrs, QStringList &errs ) {

	bool foundDataColumns( false ); //set to true if description of data columns found
	int ncol( 0 );

	QStringList::Iterator it = lines.begin();
	QStringList::Iterator itEnd  = lines.end();

	CatalogName = "";
	CatalogPrefix = "";
	CatalogColor = "";
	CatalogEpoch = 0.;
	for ( ; it != itEnd; it++ ) {
		QString d( *it ); //current data line
		if ( d.left(1) != "#" ) break;  //no longer in header!

		int iname   = d.find( "# Name: " );
		int iprefix = d.find( "# Prefix: " );
		int icolor  = d.find( "# Color: " );
		int iepoch  = d.find( "# Epoch: " );

		if ( iname == 0 ) { //line contains catalog name
			iname = d.find(":")+2;
			if ( CatalogName.isEmpty() ) { 
				CatalogName = d.mid( iname );
			} else { //duplicate name in header
				if ( showerrs )
					errs.append( i18n( "Parsing header: " ) + 
							i18n( "Extra Name field in header: %1.  Will be ignored" ).arg( d.mid(iname) ) );
			}
		} else if ( iprefix == 0 ) { //line contains catalog prefix
			iprefix = d.find(":")+2;
			if ( CatalogPrefix.isEmpty() ) { 
				CatalogPrefix = d.mid( iprefix );
			} else { //duplicate prefix in header
				if ( showerrs )
					errs.append( i18n( "Parsing header: " ) + 
							i18n( "Extra Prefix field in header: %1.  Will be ignored" ).arg( d.mid(iprefix) ) );
			}
		} else if ( icolor == 0 ) { //line contains catalog prefix
			icolor = d.find(":")+2;
			if ( CatalogColor.isEmpty() ) { 
				CatalogColor = d.mid( icolor );
			} else { //duplicate prefix in header
				if ( showerrs )
					errs.append( i18n( "Parsing header: " ) + 
							i18n( "Extra Color field in header: %1.  Will be ignored" ).arg( d.mid(icolor) ) );
			}
		} else if ( iepoch == 0 ) { //line contains catalog epoch
			iepoch = d.find(":")+2;
			if ( CatalogEpoch == 0. ) {
				bool ok( false );
				CatalogEpoch = d.mid( iepoch ).toFloat( &ok );
				if ( !ok ) {
					if ( showerrs )
						errs.append( i18n( "Parsing header: " ) + 
								i18n( "Could not convert Epoch to float: %1.  Using 2000. instead" ).arg( d.mid(iepoch) ) );
					CatalogEpoch = 2000.; //adopt default value
				}
			} else { //duplicate epoch in header
				if ( showerrs )
					errs.append( i18n( "Parsing header: " ) + 
							i18n( "Extra Epoch field in header: %1.  Will be ignored" ).arg( d.mid(iepoch) ) );
			}
		} else if ( ! foundDataColumns ) { //don't try to parse data column descriptors if we already found them
			//Chomp off leading "#" character
			d = d.replace( QRegExp( "#" ), "" );

			QStringList fields = QStringList::split( " ", d ); //split on whitespace

			//we need a copy of the master list of data fields, so we can 
			//remove fields from it as they are encountered in the "fields" list.
			//this allows us to detect duplicate entries
			QStringList master( KStarsData::CustomColumns ); 

			QStringList::Iterator itf    = fields.begin();
			QStringList::Iterator itfEnd = fields.end();

			for ( ; itf != itfEnd; itf++ ) {
				QString s( *itf );
				if ( master.contains( s ) ) {
					//add the data field
					Columns.append( s );

					// remove the field from the master list and inc the 
					// count of "good" columns (unless field is "Ignore")
					if ( s != "Ig" ) {
						master.remove( master.find( s ) );
						ncol++;
					}
				} else if ( fields.contains( s ) ) { //duplicate field
					fields.append( "Ig" ); //skip this column
					if ( showerrs )
						errs.append( i18n( "Parsing header: " ) + 
								i18n( "Duplicate data field descriptor \"%1\" will be ignored" ).arg( s ) );
				} else { //Invalid field
					fields.append( "Ig" ); //skip this column
					if ( showerrs )
						errs.append( i18n( "Parsing header: " ) + 
								i18n( "Invalid data field descriptor \"%1\" will be ignored" ).arg( s ) );
				}
			}

			if ( ncol ) foundDataColumns = true;
		}
	}

	if ( ! foundDataColumns ) {
		if ( showerrs )
			errs.append( i18n( "Parsing header: " ) + 
					i18n( "No valid column descriptors found.  Exiting" ) );
		return false;
	}

	if ( it == itEnd ) {
		if ( showerrs ) errs.append( i18n( "Parsing header: " ) +
				i18n( "No data lines found after header.  Exiting." ) );
		return false; //fatal error
	} else {
		//Make sure Name, Prefix, Color and Epoch were set
		if ( CatalogName.isEmpty() ) { 
			if ( showerrs ) errs.append( i18n( "Parsing header: " ) +
				i18n( "No Catalog Name specified; setting to \"Custom\"" ) );
			CatalogName = i18n("Custom");
		}
		if ( CatalogPrefix.isEmpty() ) { 
			if ( showerrs ) errs.append( i18n( "Parsing header: " ) +
				i18n( "No Catalog Prefix specified; setting to \"CC\"" ) );
			CatalogPrefix = "CC";
		}
		if ( CatalogColor.isEmpty() ) { 
			if ( showerrs ) errs.append( i18n( "Parsing header: " ) +
				i18n( "No Catalog Color specified; setting to Red" ) );
			CatalogColor = "#CC0000";
		}
		if ( CatalogEpoch == 0. ) { 
			if ( showerrs ) errs.append( i18n( "Parsing header: " ) +
				i18n( "No Catalog Epoch specified; assuming 2000." ) );
			CatalogEpoch = 2000.;
		}

		//the it iterator now points to the first line past the header
		iStart = lines.findIndex( QString( *it ) );
		return true;
	}
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
	file.setName( locate( "appdata", "mycities.dat" ) ); //determine filename in local user KDE directory tree.
	if ( file.exists() && file.open( IO_ReadOnly ) ) {
		QTextStream stream( &file );

  	while ( !stream.eof() ) {
			QString line = stream.readLine();
			citiesFound |= processCity( line );
		}	// while ( !stream.eof() )
		file.close();
	}	// if ( fileopen() )

	return citiesFound;
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
	} /*else if ( newMagnitude < maxSetMagnitude ) {
		StarObject *lastStar = starList.last();
		while ( lastStar->mag() > newMagnitude  && starList.count() ) {
			//check if star is named.  If so, remove it from ObjNames
			if ( ! lastStar->name().isEmpty() ) {
				ObjNames.remove( lastStar->name() );
			}
			starList.removeLast();
			lastStar = starList.last();

			//Need to recompute names of objects
			sendClearCache();
		}
	}*/

	// change current magnitude level in KStarsOptions
	Options::setMagLimitDrawStar( newMagnitude );
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
			emit progressText( i18n("Reading Time Zone Rules") );

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

			emit progressText( i18n("Loading City Data") );

			if ( !readCityData( ) )
				initError( "Cities.dat", true );
			
			break;
		}

		case 2: //Load stellar database//

			emit progressText(i18n("Loading Star Data (%1%)" ).arg(0) );
			if ( !readStarData( ) )
				initError( "hipNNN.dat", true );
			if (!readVARData())
				initError( "valaav.dat", false);
			if (!readADVTreeData())
				initError( "advinterface.dat", false);
			break;

		case 3: //Load NGC/IC database and custom catalogs//

			emit progressText( i18n("Loading NGC/IC Data (%1%)" ).arg(0) );
			if ( !readDeepSkyData( ) )
				initError( "ngcicN.dat", true );

			emit progressText( i18n("Loading Custom catalogs" ) );
			readCustomCatalogs( );
//			if ( !readCustomCatalogs( ) )
//				initError( "<custom catalog>", true );
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

		case 6: //Load Constellation boundaries//

			emit progressText( i18n("Loading Constellation Boundaries" ) );
			if ( !readCBoundData( ) )
				initError( "cbound.dat", true );
			break;

		case 7: //Load Milky Way//

			emit progressText( i18n("Loading Milky Way" ) );
			if ( !readMWData( ) )
				initError( "mw*.dat", true );
			break;

		case 8: //Initialize the Planets//

			emit progressText( i18n("Creating Planets" ) );
			if (PCat->initialize())
				PCat->addObject( ObjNames );

			jmoons = new JupiterMoons();
			break;

		case 9: //Initialize Asteroids & Comets//

			emit progressText( i18n( "Creating Asteroids and Comets" ) );
			if ( !readAsteroidData() )
				initError( "asteroids.dat", false );
			if ( !readCometData() )
				initError( "comets.dat", false );

			break;

		case 10: //Initialize the Moon//

			emit progressText( i18n("Creating Moon" ) );
			Moon = new KSMoon(this);
			ObjNames.append( Moon );
			Moon->loadData();
			break;

		case 11: //Load Image URLs//

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

		case 12: //Load Information URLs//

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
	setNextDSTChange( geo->tzrule()->nextDSTChange() );
	//reset LTime, because TZoffset has changed
	LTime = geo->UTtoLT( ut() );
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

	bool needNewCoords = false;
	if ( fabs( ut().djd() - LastNumUpdate.djd() ) > 1.0 ) {
		//update time-dependent variables once per day
		needNewCoords = true;
		LastNumUpdate = ut().djd();
	}

	// Update positions of objects, if necessary
	if ( fabs( ut().djd() - LastPlanetUpdate.djd() ) > 0.01 ) {
		LastPlanetUpdate = ut().djd();

		if ( Options::showPlanets() ) PCat->findPosition( &num, geo->lat(), LST );

		//Asteroids
		if ( Options::showPlanets() && Options::showAsteroids() )
			for ( KSAsteroid *ast = asteroidList.first(); ast; ast = asteroidList.next() )
				ast->findPosition( &num, geo->lat(), LST, earth() );

		//Comets
		if ( Options::showPlanets() && Options::showComets() )
			for ( KSComet *com = cometList.first(); com; com = cometList.next() )
				com->findPosition( &num, geo->lat(), LST, earth() );

		//Recompute the Ecliptic
		if ( Options::showEcliptic() ) {
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
	if ( fabs( ut().djd() - LastMoonUpdate.djd() ) > 0.00069444 ) {
		LastMoonUpdate = ut();
		if ( Options::showMoon() ) {
			Moon->findPosition( &num, geo->lat(), LST );
			Moon->findPhase( PCat->planetSun() );
		}

		//for now, update positions of Jupiter's moons here also
		if ( Options::showPlanets() && Options::showJupiter() )
			jmoons->findPosition( &num, (const KSPlanet*)PCat->findByName("Jupiter"), PCat->planetSun() );
	}

	//Update Alt/Az coordinates.  Timescale varies with zoom level
	//If Clock is in Manual Mode, always update. (?)
	if ( fabs( ut().djd() - LastSkyUpdate.djd() ) > 0.25/Options::zoomFactor() || clock()->isManualMode() ) {
		LastSkyUpdate = ut();

		//Recompute Alt, Az coords for all objects
		//Planets
		//This updates trails as well
		PCat->EquatorialToHorizontal( LST, geo->lat() );

		jmoons->EquatorialToHorizontal( LST, geo->lat() );
		if ( Options::showMoon() ) {
			Moon->EquatorialToHorizontal( LST, geo->lat() );
			if ( Moon->hasTrail() ) Moon->updateTrail( LST, geo->lat() );
		}

//		//Planet Trails
//		for( SkyPoint *p = PlanetTrail.first(); p; p = PlanetTrail.next() )
//			p->EquatorialToHorizontal( LST, geo->lat() );

		//Asteroids
		if ( Options::showAsteroids() ) {
			for ( KSAsteroid *ast = asteroidList.first(); ast; ast = asteroidList.next() ) {
				ast->EquatorialToHorizontal( LST, geo->lat() );
				if ( ast->hasTrail() ) ast->updateTrail( LST, geo->lat() );
			}
		}

		//Comets
		if ( Options::showComets() ) {
			for ( KSComet *com = cometList.first(); com; com = cometList.next() ) {
				com->EquatorialToHorizontal( LST, geo->lat() );
				if ( com->hasTrail() ) com->updateTrail( LST, geo->lat() );
			}
		}

		//Stars (need to update if constell. lines or stars are shown)
		if ( Options::showStars() || Options::showCLines() ) {
			// use MINDRAWSTARMAG for calculating constellation lines right
			float mag = Options::magLimitDrawStar();
			if (mag < MINDRAWSTARMAG) mag = MINDRAWSTARMAG;
			for ( StarObject *star = starList.first(); star; star = starList.next() ) {
				if ( star->mag() > mag ) break;
				if (needNewCoords) star->updateCoords( &num );
				star->EquatorialToHorizontal( LST, geo->lat() );
			}
		}

		//Deep-sky objects. Keep lists separated for performance reasons
		if ( Options::showMessier() || Options::showMessierImages() ) {
			for ( SkyObject *o = deepSkyListMessier.first(); o; o = deepSkyListMessier.next() ) {
				if (needNewCoords) o->updateCoords( &num );
				o->EquatorialToHorizontal( LST, geo->lat() );
			}
		}
		if ( Options::showNGC() ) {
			for ( SkyObject *o = deepSkyListNGC.first(); o; o = deepSkyListNGC.next() ) {
				if (needNewCoords) o->updateCoords( &num );
				o->EquatorialToHorizontal( LST, geo->lat() );
			}
		}
		if ( Options::showIC() ) {
			for ( SkyObject *o = deepSkyListIC.first(); o; o = deepSkyListIC.next() ) {
				if (needNewCoords) o->updateCoords( &num );
				o->EquatorialToHorizontal( LST, geo->lat() );
			}
		}
		if ( Options::showOther() ) {
			for ( SkyObject *o = deepSkyListOther.first(); o; o = deepSkyListOther.next() ) {
				if (needNewCoords) o->updateCoords( &num );
				o->EquatorialToHorizontal( LST, geo->lat() );
			}
		}


		//Custom Catalogs
		for ( unsigned int j=0; j< CustomCatalogs.count(); ++j ) {
			QPtrList<SkyObject> cat = CustomCatalogs.at(j)->objList();
			if ( Options::showCatalog()[j] ) {
                                 for ( SkyObject *o = cat.first(); o; o = cat.next() ) {
					if (needNewCoords) o->updateCoords( &num );
					o->EquatorialToHorizontal( LST, geo->lat() );
				}
			}
		}

		//Milky Way
		if ( Options::showMilkyWay() ) {
			for ( unsigned int j=0; j<11; ++j ) {
				for ( SkyPoint *p = MilkyWay[j].first(); p; p = MilkyWay[j].next() ) {
					if (needNewCoords) p->updateCoords( &num );
					p->EquatorialToHorizontal( LST, geo->lat() );
				}
			}
		}

		//CNames
		if ( Options::showCNames() ) {
			for ( SkyPoint *p = cnameList.first(); p; p = cnameList.next() ) {
				if (needNewCoords) p->updateCoords( &num );
				p->EquatorialToHorizontal( LST, geo->lat() );
			}
		}

		//Constellation Boundaries
		if ( Options::showCBounds() ) {  
			for ( CSegment *seg = csegmentList.first(); seg; seg = csegmentList.next() ) {
				for ( SkyPoint *p = seg->firstNode(); p; p = seg->nextNode() ) {
					if ( needNewCoords ) p->updateCoords( &num );
					p->EquatorialToHorizontal( LST, geo->lat() );
				}
			}
		}
		
		//Celestial Equator
		if ( Options::showEquator() ) {
			for ( SkyPoint *p = Equator.first(); p; p = Equator.next() ) {
				p->EquatorialToHorizontal( LST, geo->lat() );
			}
		}

		//Ecliptic
		if ( Options::showEcliptic() ) {
			for ( SkyPoint *p = Ecliptic.first(); p; p = Ecliptic.next() ) {
				p->EquatorialToHorizontal( LST, geo->lat() );
			}
		}

		//Horizon: different than the others; Alt & Az remain constant, RA, Dec must keep up
		if ( Options::showHorizon() || Options::showGround() ) {
			for ( SkyPoint *p = Horizon.first(); p; p = Horizon.next() ) {
				p->HorizontalToEquatorial( LST, geo->lat() );
			}
		}

		for (SkyObject *o = INDITelescopeList.first(); o; o = INDITelescopeList.next())
			o->EquatorialToHorizontal(LST, geo->lat());

		//Update focus
		skymap->updateFocus();

		if ( clock()->isManualMode() )
			QTimer::singleShot( 0, skymap, SLOT( forceUpdateNow() ) );
		else skymap->forceUpdate();
	}
}

void KStarsData::setTimeDirection( float scale ) {
	TimeRunsForward = ( scale < 0 ? false : true );
}

void KStarsData::setFullTimeUpdate() {
	LastSkyUpdate.setDJD(    (long double)INVALID_DAY );
	LastPlanetUpdate.setDJD( (long double)INVALID_DAY );
	LastMoonUpdate.setDJD(   (long double)INVALID_DAY );
	LastNumUpdate.setDJD(    (long double)INVALID_DAY );
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

SkyObject* KStarsData::objectNamed( const QString &name ) {
	if ( (name== "star") || (name== "nothing") || name.isEmpty() ) return NULL;
	if ( name== Moon->name() ) return Moon;

	SkyObject *so = PCat->findByName(name);

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

	//Still no match.  Try interpreting the string as a genetive star name 
	//(with ascii characters instead of a Greek letter)
	for ( unsigned int i=0; i<starList.count(); ++i ) {
		if ( name==starList.at(i)->gname( false ) ) return starList.at(i);
	}

       //Custom catalogs. 
        for ( unsigned int i=0; i<CustomCatalogs.count(); ++i ) {
                QPtrList<SkyObject> custCatObjs = CustomCatalogs.at(i)->objList();
                for ( unsigned int j = 0; j < custCatObjs.count(); ++j ) {
                        if ( name==custCatObjs.at(j)->name() ) return custCatObjs.at(j);
                }
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
				if ( az >= 0.0 ) { 
					map->setFocusAltAz( 15.0, az ); 
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
					changeDateTime( geo()->LTtoUT( KStarsDateTime( ExtDate(yr, mth, day), QTime(hr,min,sec) ) ) );
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
