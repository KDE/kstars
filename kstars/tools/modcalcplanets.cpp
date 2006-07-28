/***************************************************************************
                          modcalcequinox.cpp  -  description
                             -------------------
    begin                : dom may 2 2004
    copyright            : (C) 2004-2005 by Pablo de Vicente
    email                : p.devicentea@wanadoo.es
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QString>
#include <kfiledialog.h>
#include <kmessagebox.h>

#include "modcalcplanets.h"
#include "dms.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksnumbers.h"
#include "kssun.h"
#include "ksplanet.h"
#include "ksmoon.h"
#include "kspluto.h"
#include "widgets/dmsbox.h"
#include "libkdeedu/extdate/extdatetimeedit.h"

modCalcPlanets::modCalcPlanets(QWidget *parentSplit) 
: QFrame(parentSplit) {
	setupUi(this);
	showCurrentDateTime();
	showLongLat();
	RABox->setDegType(false);

    // signals and slots connections
    connect(PlanetComboBox, SIGNAL(activated(int)), this, SLOT(slotComputePosition()));
    connect(UTCheckBatch, SIGNAL(clicked()), this, SLOT(slotUtCheckedBatch()));
    connect(DateCheckBatch, SIGNAL(clicked()), this, SLOT(slotDateCheckedBatch()));
    connect(LatCheckBatch, SIGNAL(clicked()), this, SLOT(slotLatCheckedBatch()));
    connect(LongCheckBatch, SIGNAL(clicked()), this, SLOT(slotLongCheckedBatch()));
    connect(PlanetCheckBatch, SIGNAL(clicked()), this, SLOT(slotPlanetsCheckedBatch()));

	show();
}

modCalcPlanets::~modCalcPlanets(){
}

void modCalcPlanets::showCurrentDateTime (void)
{
	KStarsDateTime dt( KStarsDateTime::currentDateTime() );

	DateBox->setDate( dt.date() );
	TimeBox->setTime( dt.time() );

	DateBoxBatch->setDate( dt.date() );
	UTBoxBatch->setTime( dt.time() );
}

KStarsDateTime modCalcPlanets::getDateTime (void)
{
	return KStarsDateTime( DateBox->date() , TimeBox->time() );
}

void modCalcPlanets::showLongLat(void)
{

	KStars *ks = (KStars*) topLevelWidget()->parent();
	LongBox->show( ks->geo()->lng() );
	LatBox->show( ks->geo()->lat() );

	LongBoxBatch->show( ks->geo()->lng() );
	LatBoxBatch->show( ks->geo()->lat() );
}


GeoLocation modCalcPlanets::getObserverPosition (void)
{
	GeoLocation geoPlace;

	geoPlace.setLong( LongBox->createDms() );
	geoPlace.setLat(  LatBox->createDms() );
	geoPlace.setHeight( 0.0 );

	return geoPlace;
}

void modCalcPlanets::slotComputePosition (void)
{
	KStarsData *kd = ((KStars*)topLevelWidget()->parent())->data();
	KStarsDateTime dt = getDateTime(); 
	long double julianDay = dt.djd();
	GeoLocation position( getObserverPosition() );
	KSNumbers num( julianDay );
	dms LST( position.GSTtoLST( dt.gst() ) );

	// Earth
	KSPlanet Earth( kd, I18N_NOOP( "Earth" ));
	Earth.findPosition( &num );
	
	// Mercury
	if (PlanetComboBox->currentIndex() == 0 ) {
		KSPlanet p( kd, I18N_NOOP( "Mercury" ));
		p.findPosition( &num, position.lat(), &LST, &Earth);
		p.EquatorialToHorizontal( &LST, position.lat());
		showCoordinates( p );
	}
	else if(PlanetComboBox->currentIndex() == 1) {
		KSPlanet p( kd, I18N_NOOP( "Venus" ));
		p.findPosition( &num, position.lat(), &LST, &Earth);
		p.EquatorialToHorizontal( &LST, position.lat());
		showCoordinates( p );
	}
	else if(PlanetComboBox->currentIndex() == 2) {
		showCoordinates( Earth );
	}
	else if(PlanetComboBox->currentIndex() == 3) {
		KSPlanet p( kd, I18N_NOOP( "Mars" ));
		p.findPosition( &num, position.lat(), &LST, &Earth);
		p.EquatorialToHorizontal( &LST, position.lat());
		showCoordinates( p );
	}
	else if(PlanetComboBox->currentIndex() == 4) {
		KSPlanet p( kd, I18N_NOOP( "Jupiter" ));
		p.findPosition( &num, position.lat(), &LST, &Earth);
		p.EquatorialToHorizontal( &LST, position.lat());
		showCoordinates( p );
	}
	else if(PlanetComboBox->currentIndex() == 5) {
		KSPlanet p( kd, I18N_NOOP( "Saturn" ));
		p.findPosition( &num, position.lat(), &LST, &Earth);
		p.EquatorialToHorizontal( &LST, position.lat());
		showCoordinates( p );
	}
	else if(PlanetComboBox->currentIndex() == 6) {
		KSPlanet p( kd, I18N_NOOP( "Uranus" ));
		p.findPosition( &num, position.lat(), &LST, &Earth);
		p.EquatorialToHorizontal( &LST, position.lat());
		showCoordinates( p );
	}
	else if(PlanetComboBox->currentIndex() == 7) {
		KSPlanet p( kd, I18N_NOOP( "Neptune" ));
		p.findPosition( &num, position.lat(), &LST, &Earth);
		p.EquatorialToHorizontal( &LST, position.lat());
		showCoordinates( p );
	}
	else if(PlanetComboBox->currentIndex() == 8) {
		KSPluto p( kd );
		p.findPosition( &num, position.lat(), &LST, &Earth);
		p.EquatorialToHorizontal( &LST, position.lat());
		showCoordinates( p );
	}
	else if(PlanetComboBox->currentIndex() == 9) {
		KSMoon p( kd );
		p.findPosition( &num, position.lat(), &LST, &Earth);
		p.EquatorialToHorizontal( &LST, position.lat());
		showCoordinates( p );
	}
	else if(PlanetComboBox->currentIndex() == 10) {
		KSSun p( kd );
		p.findPosition( &num, position.lat(), &LST, &Earth);
		p.EquatorialToHorizontal( &LST, position.lat());
		p.setRsun(0.0);
		showCoordinates( p );
	}
}

void modCalcPlanets::showCoordinates( const KSPlanet &ksp) {

	showHeliocentricEclipticCoords(ksp.helEcLong(), ksp.helEcLat(), ksp.rsun() );
	showGeocentricEclipticCoords(ksp.ecLong(), ksp.ecLat(), ksp.rearth() );
	showEquatorialCoords(ksp.ra(), ksp.dec() );
	showTopocentricCoords(ksp.az(), ksp.alt() );
	
}

void modCalcPlanets::showCoordinates( const KSMoon &ksp ) {

	showHeliocentricEclipticCoords(ksp.helEcLong(), ksp.helEcLat(), ksp.rsun() );
	showGeocentricEclipticCoords(ksp.ecLong(), ksp.ecLat(), ksp.rearth() );
	showEquatorialCoords(ksp.ra(), ksp.dec() );
	showTopocentricCoords(ksp.az(), ksp.alt() );
	
}
void modCalcPlanets::showCoordinates( const KSPluto &ksp ) {

	showHeliocentricEclipticCoords(ksp.helEcLong(), ksp.helEcLat(), ksp.rsun() );
	showGeocentricEclipticCoords(ksp.ecLong(), ksp.ecLat(), ksp.rearth() );
	showEquatorialCoords(ksp.ra(), ksp.dec() );
	showTopocentricCoords(ksp.az(), ksp.alt() );
	
}

void modCalcPlanets::showCoordinates( const KSSun &ksp ) {

	showHeliocentricEclipticCoords(ksp.helEcLong(), ksp.helEcLat(), ksp.rsun() );
	showGeocentricEclipticCoords(ksp.ecLong(), ksp.ecLat(), ksp.rearth() );
	showEquatorialCoords(ksp.ra(), ksp.dec() );
	showTopocentricCoords(ksp.az(), ksp.alt() );
	
}

void modCalcPlanets::showHeliocentricEclipticCoords(const dms *hLong, const dms *hLat, double dist)
{
	HelioLongBox->show( hLong );
	HelioLatBox->show( hLat );
	HelioDistBox->setText( KGlobal::locale()->formatNumber( dist,6));
}

void modCalcPlanets::showGeocentricEclipticCoords(const dms *eLong, const dms *eLat, double dist)
{
	GeoLongBox->show( eLong );
	GeoLatBox->show( eLat );
	GeoDistBox->setText( KGlobal::locale()->formatNumber( dist,6));
}

void modCalcPlanets::showEquatorialCoords(const dms *ra, const dms *dec)
{
	RABox->show( ra, false );
	DecBox->show( dec );
}

void modCalcPlanets::showTopocentricCoords(const dms *az, const dms *el)
{
	AzBox->show( az );
	AltBox->show( el );
}

void modCalcPlanets::slotPlanetsCheckedBatch(){

	if ( PlanetCheckBatch->isChecked() )
		PlanetComboBoxBatch->setEnabled( false );
	else {
		PlanetComboBoxBatch->setEnabled( true );
	}
}

void modCalcPlanets::slotUtCheckedBatch(){

	if ( UTCheckBatch->isChecked() )
		UTBoxBatch->setEnabled( false );
	else {
		UTBoxBatch->setEnabled( true );
	}
}

void modCalcPlanets::slotDateCheckedBatch(){

	if ( DateCheckBatch->isChecked() )
		DateBoxBatch->setEnabled( false );
	else {
		DateBoxBatch->setEnabled( true );
	}
}

void modCalcPlanets::slotLongCheckedBatch(){

	if ( LongCheckBatch->isChecked() )
		LongBoxBatch->setEnabled( false );
	else {
		LongBoxBatch->setEnabled( true );
	}
}

void modCalcPlanets::slotLatCheckedBatch(){

	if ( LatCheckBatch->isChecked() )
		LatBoxBatch->setEnabled( false );
	else {
		LatBoxBatch->setEnabled( true );
	}
}

void modCalcPlanets::slotRunBatch() {

	QString inputFileName;

	inputFileName = InputFileBoxBatch->url();

	// We open the input file and read its content

	if ( QFile::exists(inputFileName) ) {
		QFile f( inputFileName );
		if ( !f.open( QIODevice::ReadOnly) ) {
			QString message = i18n( "Could not open file %1.", f.fileName() );
			KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
			inputFileName = QString();
			return;
		}

		QTextStream istream(&f);
		processLines(istream);
		f.close();
	} else  {
		QString message = i18n( "Invalid file: %1", inputFileName );
		KMessageBox::sorry( 0, message, i18n( "Invalid file" ) );
		inputFileName = QString();
		InputFileBoxBatch->setUrl( inputFileName );
		return;
	}
}

unsigned int modCalcPlanets::requiredBatchFields(void) {
	unsigned int i = 0;
	
	if(PlanetCheckBatch->isChecked() )
		i++;
	if(UTCheckBatch->isChecked() )
		i++;
	if(DateCheckBatch->isChecked() )
		i++;
	if (LongCheckBatch->isChecked() )
		i++;
	if (LatCheckBatch->isChecked() )
		i++;

	return i;
	
}

void modCalcPlanets::processLines( QTextStream &istream ) {

	// we open the output file

	QString outputFileName, lineToWrite;
	outputFileName = OutputFileBoxBatch->url();
	QFile fOut( outputFileName );
	fOut.open(QIODevice::WriteOnly);
	QTextStream ostream(&fOut);
	bool lineIsValid = true;
	QString message;

	QString line;
	QString space = " ";
	QString planetB;
	unsigned int i = 0, nline = 0;
	QTime utB;
	ExtDate dtB;
	dms longB, latB, hlongB, hlatB, glongB, glatB, raB, decB, azmB, altB;
	double rSunB(0.0), rEarthB(0.0);

	KStarsData *kd = ((KStars*)topLevelWidget()->parent())->data();

	//Initialize planet names
	QString pn=QString();
	QStringList pNames, pNamesi18n;
	pNames << "Mercury" << "Venus" << "Earth" << "Mars" << "Jupiter"
	      << "Saturn" << "Uranus" << "Neptune" << "Pluto" 
	      << "Sun" << "Moon";
	pNamesi18n << i18n("Mercury") << i18n("Venus") << i18n("Earth") 
		  << i18n("Mars") << i18n("Jupiter") << i18n("Saturn") 
		  << i18n("Uranus") << i18n("Neptune") << i18n("Pluto") 
		  << i18n("Sun") << i18n("Moon");

	///Parse the input file
	int numberOfRequiredFields = requiredBatchFields();
       	while ( ! istream.atEnd() ) {
		lineToWrite=QString();
		line = istream.readLine();
		line.trimmed();

		//Go through the line, looking for parameters

		QStringList fields = line.split( " " );

		if (fields.count() != numberOfRequiredFields ) {
			lineIsValid = false;
			kWarning() << i18n( "Incorrect number of fields in line %1: " , nline) 
			            << i18n( "Present fields %1. " , fields.count())
			            << i18n( "Required fields %1. " , numberOfRequiredFields) << endl;
			nline++;
			continue;
		}

		i = 0;
		if(PlanetCheckBatch->isChecked() ) {
			planetB = fields[i];

			int j = pNamesi18n.indexOf( planetB );
			if (j == -1) {
				kWarning() << i18n("Unknown planet ")  
					    << fields[i]  
					    << i18n(" in line %1: ", nline) << endl;
				continue;
			}
			pn = pNames.at(j); //untranslated planet name
			i++;
		} else
			planetB = PlanetComboBoxBatch->currentText( );
		
		if ( AllRadioBatch->isChecked() ) {
			lineToWrite = planetB;
			lineToWrite += space;
		}
		else
			if(PlanetCheckBatch->isChecked() ) {
				lineToWrite = planetB;
		  		lineToWrite += space;
			}
		
		// Read Ut and write in ostream if corresponds
		
		if(UTCheckBatch->isChecked() ) {
			utB = QTime::fromString( fields[i] );
			if ( !utB.isValid() ) {
				kWarning() << i18n( "Line %1 contains an invalid time" , nline) << endl;
				lineIsValid=false;
				nline++;
				continue;
			}
			i++;
		} else
			utB = UTBoxBatch->time();
		
		if ( AllRadioBatch->isChecked() ) 
			lineToWrite += utB.toString().append(space);
		else
			if(UTCheckBatch->isChecked() ) 
				lineToWrite += utB.toString().append(space);
			
		// Read date and write in ostream if corresponds
		
		if(DateCheckBatch->isChecked() ) {
			dtB = ExtDate::fromString( fields[i], Qt::ISODate );
			if ( !dtB.isValid() ) {
				kWarning() << i18n( "Line %1 contains an invalid date: " , nline) << 
				fields[i] << endl ;
				lineIsValid=false;
				nline++;
				continue;
			}
			i++;
		} else
			dtB = DateBoxBatch->date();
		if ( AllRadioBatch->isChecked() )
			lineToWrite += dtB.toString().append(space);
		else
			if(DateCheckBatch->isChecked() )
			 	lineToWrite += dtB.toString().append(space);
		
		// Read Longitude and write in ostream if corresponds
		
		if (LongCheckBatch->isChecked() ) {
			longB = dms::fromString( fields[i],true);
			i++;
		} else
			longB = LongBoxBatch->createDms(true);
		
		if ( AllRadioBatch->isChecked() )
			lineToWrite += longB.toDMSString() + space;
		else
			if (LongCheckBatch->isChecked() )
				lineToWrite += longB.toDMSString() +  space;
		
		// Read Latitude

		if (LatCheckBatch->isChecked() ) {
			latB = dms::fromString( fields[i], true);
			i++;
		} else
			latB = LatBoxBatch->createDms(true);
		if ( AllRadioBatch->isChecked() )
			lineToWrite += latB.toDMSString() + space;
		else
			if (LatCheckBatch->isChecked() )
				lineToWrite += latB.toDMSString() + space;	

		KStarsDateTime edt( dtB, utB );
		dms LST = edt.gst().Degrees() + longB.Degrees();

		KSNumbers num( edt.djd() );
		KSPlanet Earth( kd, I18N_NOOP( "Earth" ));
		Earth.findPosition( &num );

		KSPlanetBase *kspb;
		if ( pn == "Pluto" ) {
		  KSPluto ksp( kd );
		  ksp.findPosition( &num, &latB, &LST, &Earth );
		  ksp.EquatorialToHorizontal( &LST, &latB );
		  kspb = (KSPlanetBase*)&ksp;
		} else if ( pn == "Sun" ) {
		  KSSun ksp( kd );
		  ksp.findPosition( &num, &latB, &LST, &Earth );
		  ksp.EquatorialToHorizontal( &LST, &latB );
		  kspb = (KSPlanetBase*)&ksp;
		} else if ( pn == "Moon" ) {
		  KSMoon ksp( kd );
		  ksp.findPosition( &num, &latB, &LST, &Earth );
		  ksp.EquatorialToHorizontal( &LST, &latB );
		  kspb = (KSPlanetBase*)&ksp;
		} else {
		  KSPlanet ksp( kd, i18n( pn.toLocal8Bit() ), QString(), Qt::white, 1.0 );
		  ksp.findPosition( &num, &latB, &LST, &Earth );
		  ksp.EquatorialToHorizontal( &LST, &latB );
		  kspb = (KSPlanetBase*)&ksp;
		}

		// Heliocentric Ecl. coords.
		hlongB.setD( kspb->helEcLong()->Degrees());
		hlatB.setD( kspb->helEcLat()->Degrees());
		rSunB = kspb->rsun();
		// Geocentric Ecl. coords.
		glongB.setD( kspb->ecLong()->Degrees() );
		glatB.setD( kspb->ecLat()->Degrees() );
		rEarthB = kspb->rearth();
		// Equatorial coords.
		decB.setD( kspb->dec()->Degrees() );
		raB.setD( kspb->ra()->Degrees() );
		// Topocentric Coords.
		azmB.setD( kspb->az()->Degrees() );
		altB.setD( kspb->alt()->Degrees() );
			
		ostream << lineToWrite;

		if ( HelioEclCheckBatch->isChecked() ) 
		  ostream << hlongB.toDMSString() << space << hlatB.toDMSString() << space << rSunB << space ;
		if ( GeoEclCheckBatch->isChecked() ) 
		  ostream << glongB.toDMSString() << space << glatB.toDMSString() << space << rEarthB << space ;
		if ( EquatorialCheckBatch->isChecked() ) 
		  ostream << raB.toHMSString() << space << decB.toDMSString() << space ;
		if ( HorizontalCheckBatch->isChecked() ) 
		  ostream << azmB.toDMSString() << space << altB.toDMSString() << space ;
		
		ostream << endl;

		nline++;
	}

	if (!lineIsValid) { 
		QString message = i18n("Errors found while parsing some lines in the input file");
		KMessageBox::sorry( 0, message, i18n( "Errors in lines" ) );
	}

	fOut.close();
}

#include "modcalcplanets.moc"
