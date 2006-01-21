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

#include "modcalcplanets.h"
#include "modcalcplanets.moc"
#include "dms.h"
#include "dmsbox.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "kssun.h"
#include "ksplanet.h"
#include "ksmoon.h"
#include "kspluto.h"
#include "libkdeedu/extdate/extdatetimeedit.h"
#include "ksnumbers.h"

#include <qcombobox.h>
#include <qdatetimeedit.h>
#include <qstring.h>
#include <qtextstream.h>
#include <kfiledialog.h>
#include <kmessagebox.h>
#include <qcheckbox.h>
#include <qradiobutton.h>


modCalcPlanets::modCalcPlanets(QWidget *parentSplit, const char *name) : modCalcPlanetsDlg (parentSplit,name) {
	showCurrentDateTime();
	showLongLat();
	raBox->setDegType(FALSE);
	show();
}

modCalcPlanets::~modCalcPlanets(){
}

void modCalcPlanets::showCurrentDateTime (void)
{
	KStars *ks = (KStars*) parent()->parent()->parent(); // QSplitter->AstroCalc->KStars
	KStarsDateTime dt = ks->data()->geo()->LTtoUT( KStarsDateTime::currentDateTime() );

	dateBox->setDate( dt.date() );
	timeBox->setTime( dt.time() );

	dateBoxBatch->setDate( dt.date() );
	utBoxBatch->setTime( dt.time() );
}

KStarsDateTime modCalcPlanets::getDateTime (void)
{
	return KStarsDateTime( dateBox->date() , timeBox->time() );
}

void modCalcPlanets::showLongLat(void)
{

	KStars *ks = (KStars*) parent()->parent()->parent(); // QSplitter->AstroCalc->KStars
	longBox->show( ks->geo()->lng() );
	latBox->show( ks->geo()->lat() );

	longBoxBatch->show( ks->geo()->lng() );
	latBoxBatch->show( ks->geo()->lat() );
}


GeoLocation modCalcPlanets::getObserverPosition (void)
{
	GeoLocation geoPlace;

	geoPlace.setLong( longBox->createDms() );
	geoPlace.setLat(  latBox->createDms() );
	geoPlace.setHeight( 0.0 );

	return geoPlace;
}

void modCalcPlanets::slotComputePosition (void)
{
	KStarsData *kd = (KStarsData*) parent()->parent()->parent();
	KStarsDateTime dt = getDateTime(); 
	long double julianDay = dt.djd();
	GeoLocation position( getObserverPosition() );
	KSNumbers num( julianDay );
	dms LST( position.GSTtoLST( dt.gst() ) );

	// Earth
	KSPlanet Earth( kd, I18N_NOOP( "Earth" ));
	Earth.findPosition( &num );
	
	// Mercury
	if (planetComboBox->currentItem() == 0 ) {
		KSPlanet p( kd, I18N_NOOP( "Mercury" ));
		p.findPosition( &num, position.lat(), &LST, &Earth);
		p.EquatorialToHorizontal( &LST, position.lat());
		showCoordinates( p );
	}
	else if(planetComboBox->currentItem() == 1) {
		KSPlanet p( kd, I18N_NOOP( "Venus" ));
		p.findPosition( &num, position.lat(), &LST, &Earth);
		p.EquatorialToHorizontal( &LST, position.lat());
		showCoordinates( p );
	}
	else if(planetComboBox->currentItem() == 2) {
		showCoordinates( Earth );
	}
	else if(planetComboBox->currentItem() == 3) {
		KSPlanet p( kd, I18N_NOOP( "Mars" ));
		p.findPosition( &num, position.lat(), &LST, &Earth);
		p.EquatorialToHorizontal( &LST, position.lat());
		showCoordinates( p );
	}
	else if(planetComboBox->currentItem() == 4) {
		KSPlanet p( kd, I18N_NOOP( "Jupiter" ));
		p.findPosition( &num, position.lat(), &LST, &Earth);
		p.EquatorialToHorizontal( &LST, position.lat());
		showCoordinates( p );
	}
	else if(planetComboBox->currentItem() == 5) {
		KSPlanet p( kd, I18N_NOOP( "Saturn" ));
		p.findPosition( &num, position.lat(), &LST, &Earth);
		p.EquatorialToHorizontal( &LST, position.lat());
		showCoordinates( p );
	}
	else if(planetComboBox->currentItem() == 6) {
		KSPlanet p( kd, I18N_NOOP( "Uranus" ));
		p.findPosition( &num, position.lat(), &LST, &Earth);
		p.EquatorialToHorizontal( &LST, position.lat());
		showCoordinates( p );
	}
	else if(planetComboBox->currentItem() == 7) {
		KSPlanet p( kd, I18N_NOOP( "Neptune" ));
		p.findPosition( &num, position.lat(), &LST, &Earth);
		p.EquatorialToHorizontal( &LST, position.lat());
		showCoordinates( p );
	}
	else if(planetComboBox->currentItem() == 8) {
		KSPluto p( kd );
		p.findPosition( &num, position.lat(), &LST, &Earth);
		p.EquatorialToHorizontal( &LST, position.lat());
		showCoordinates( p );
	}
	else if(planetComboBox->currentItem() == 9) {
		KSMoon p( kd );
		p.findPosition( &num, position.lat(), &LST, &Earth);
		p.EquatorialToHorizontal( &LST, position.lat());
		showCoordinates( p );
	}
	else if(planetComboBox->currentItem() == 10) {
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

void modCalcPlanets::slotClear(void){
	helLongBox->setText( "" );
	helLatBox->setText( "" );
	sunDistBox->setText( "" );
	geoLongBox->setText("");
	geoLatBox->setText("");
	earthDistBox->setText("");
	raBox->clearFields();
	decBox->clearFields();
	azBox->setText("");
	altBox->setText("");
}

void modCalcPlanets::showHeliocentricEclipticCoords(const dms *hLong, const dms *hLat, double dist)
{
	helLongBox->show( hLong );
	helLatBox->show( hLat );
	sunDistBox->setText( KGlobal::locale()->formatNumber( dist,6));
}

void modCalcPlanets::showGeocentricEclipticCoords(const dms *eLong, const dms *eLat, double dist)
{
	geoLongBox->show( eLong );
	geoLatBox->show( eLat );
	earthDistBox->setText( KGlobal::locale()->formatNumber( dist,6));
}

void modCalcPlanets::showEquatorialCoords(const dms *ra, const dms *dec)
{
	raBox->show( ra, FALSE );
	decBox->show( dec );
}

void modCalcPlanets::showTopocentricCoords(const dms *az, const dms *el)
{
	azBox->show( az );
	altBox->show( el );
}

void modCalcPlanets::slotPlanetsCheckedBatch(){

	if ( planetCheckBatch->isChecked() )
		planetComboBoxBatch->setEnabled( false );
	else {
		planetComboBoxBatch->setEnabled( true );
	}
}

void modCalcPlanets::slotUtCheckedBatch(){

	if ( utCheckBatch->isChecked() )
		utBoxBatch->setEnabled( false );
	else {
		utBoxBatch->setEnabled( true );
	}
}

void modCalcPlanets::slotDateCheckedBatch(){

	if ( dateCheckBatch->isChecked() )
		dateBoxBatch->setEnabled( false );
	else {
		dateBoxBatch->setEnabled( true );
	}
}

void modCalcPlanets::slotLongCheckedBatch(){

	if ( longCheckBatch->isChecked() )
		longBoxBatch->setEnabled( false );
	else {
		longBoxBatch->setEnabled( true );
	}
}

void modCalcPlanets::slotLatCheckedBatch(){

	if ( latCheckBatch->isChecked() )
		latBoxBatch->setEnabled( false );
	else {
		latBoxBatch->setEnabled( true );
	}
}

void modCalcPlanets::slotInputFile() {
	QString inputFileName;
	inputFileName = KFileDialog::getOpenFileName( );
	InputLineEditBatch->setText( inputFileName );
}

void modCalcPlanets::slotOutputFile() {
	QString outputFileName;
	outputFileName = KFileDialog::getSaveFileName( );
	OutputLineEditBatch->setText( outputFileName );
}

void modCalcPlanets::slotRunBatch() {

	QString inputFileName;

	inputFileName = InputLineEditBatch->text();

	// We open the input file and read its content

	if ( QFile::exists(inputFileName) ) {
		QFile f( inputFileName );
		if ( !f.open( IO_ReadOnly) ) {
			QString message = i18n( "Could not open file %1.").arg( f.name() );
			KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
			inputFileName = "";
			return;
		}

		QTextStream istream(&f);
		processLines(istream);
		f.close();
	} else  {
		QString message = i18n( "Invalid file: %1" ).arg( inputFileName );
		KMessageBox::sorry( 0, message, i18n( "Invalid file" ) );
		inputFileName = "";
		InputLineEditBatch->setText( inputFileName );
		return;
	}
}

unsigned int modCalcPlanets::requiredBatchFields(void) {
	unsigned int i = 0;
	
	if(planetCheckBatch->isChecked() )
		i++;
	if(utCheckBatch->isChecked() )
		i++;
	if(dateCheckBatch->isChecked() )
		i++;
	if (longCheckBatch->isChecked() )
		i++;
	if (latCheckBatch->isChecked() )
		i++;

	return i;
	
}

void modCalcPlanets::processLines( QTextStream &istream ) {

	// we open the output file

	QString outputFileName, lineToWrite;
	outputFileName = OutputLineEditBatch->text();
	QFile fOut( outputFileName );
	fOut.open(IO_WriteOnly);
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
	KStarsData *kd = (KStarsData*) parent()->parent()->parent(); // QSplitter->AstroCalc->KStars
	PlanetCatalog PCat( kd ); 
	PCat.initialize();

	QString pName[11], pNamei18n[11];

	pName[0] = "Mercury";  pNamei18n[0]= i18n("Mercury"); 
	pName[1] = "Venus";    pNamei18n[1]= i18n("Venus");
	pName[2] = "Earth";    pNamei18n[2]= i18n("Earth");
	pName[3] = "Mars";     pNamei18n[3]= i18n("Mars");
	pName[4] = "Jupiter";  pNamei18n[4]= i18n("Jupiter");
	pName[5] = "Saturn";   pNamei18n[5]= i18n("Saturn");
	pName[6] = "Uranus";   pNamei18n[6]= i18n("Uranus");
	pName[7] = "Neptune";  pNamei18n[7]= i18n("Neptune");
	pName[8] = "Pluto";    pNamei18n[8]= i18n("Pluto");
	pName[9] = "Sun";      pNamei18n[9]= i18n("Sun");
	pName[10] = "Moon";    pNamei18n[10]= i18n("Moon");

	unsigned int numberOfRequiredFields = requiredBatchFields();
	
	while ( ! istream.eof() ) {
		lineToWrite="";
		line = istream.readLine();
		line.stripWhiteSpace();

		//Go through the line, looking for parameters

		QStringList fields = QStringList::split( " ", line );

		if (fields.count() != numberOfRequiredFields ) {
			lineIsValid = false;
			kdWarning() << i18n( "Incorrect number of fields in line %1: " ).arg(nline) 
			            << i18n( "Present fields %1. " ).arg(fields.count())
			            << i18n( "Required fields %1. " ).arg(numberOfRequiredFields) << endl;
			nline++;
			continue;
		}

		i = 0;
		if(planetCheckBatch->isChecked() ) {
			planetB = fields[i];
			unsigned int result = 1;
			int j = 0;
			while (j < 11) {
			//while (result != 0 && j < 11) {
				result = QString::compare( pNamei18n[j] , planetB );
				if (result == 0)
					break;
				j++;
			}
			if (j == 11) {
				kdWarning() << i18n("Unknown planet ")  
					    << fields[i]  
					    << i18n(" in line %1: ").arg(nline) << endl;
				continue;
			}
			i++;
		} else
			planetB = planetComboBoxBatch->currentText( );
		
		if ( allRadioBatch->isChecked() ) {
			lineToWrite = planetB;
			lineToWrite += space;
		}
		else
			if(planetCheckBatch->isChecked() ) {
				lineToWrite = planetB;
		  		lineToWrite += space;
			}
		
		// Read Ut and write in ostream if corresponds
		
		if(utCheckBatch->isChecked() ) {
			utB = QTime::fromString( fields[i] );
			if ( !utB.isValid() ) {
				kdWarning() << i18n( "Line %1 contains an invalid time" ).arg(nline) << endl;
				lineIsValid=false;
				nline++;
				continue;
			}
			i++;
		} else
			utB = utBoxBatch->time();
		
		if ( allRadioBatch->isChecked() ) 
			lineToWrite += utB.toString().append(space);
		else
			if(utCheckBatch->isChecked() ) 
				lineToWrite += utB.toString().append(space);
			
		// Read date and write in ostream if corresponds
		
		if(dateCheckBatch->isChecked() ) {
			dtB = ExtDate::fromString( fields[i], Qt::ISODate );
			if ( !dtB.isValid() ) {
				kdWarning() << i18n( "Line %1 contains an invalid date: " ).arg(nline) << 
				fields[i] << endl ;
				lineIsValid=false;
				nline++;
				continue;
			}
			i++;
		} else
			dtB = dateBoxBatch->date();
		if ( allRadioBatch->isChecked() )
			lineToWrite += dtB.toString().append(space);
		else
			if(dateCheckBatch->isChecked() )
			 	lineToWrite += dtB.toString().append(space);
		
		// Read Longitude and write in ostream if corresponds
		
		if (longCheckBatch->isChecked() ) {
			longB = dms::fromString( fields[i],TRUE);
			i++;
		} else
			longB = longBoxBatch->createDms(TRUE);
		
		if ( allRadioBatch->isChecked() )
			lineToWrite += longB.toDMSString() + space;
		else
			if (longCheckBatch->isChecked() )
				lineToWrite += longB.toDMSString() +  space;
		
		// Read Latitude

		if (latCheckBatch->isChecked() ) {
			latB = dms::fromString( fields[i], TRUE);
			i++;
		} else
			latB = latBoxBatch->createDms(TRUE);
		if ( allRadioBatch->isChecked() )
			lineToWrite += latB.toDMSString() + space;
		else
			if (latCheckBatch->isChecked() )
				lineToWrite += latB.toDMSString() + space;	

		KStarsDateTime edt( dtB, utB );
		dms LST = edt.gst().Degrees() + longB.Degrees();

		KSNumbers num( edt.djd() );
		
		PCat.findPosition( &num, &latB, &LST );
		PCat.EquatorialToHorizontal(&LST, &latB);

		KSPlanet Earth( kd, I18N_NOOP( "Earth" ));
		Earth.findPosition( &num );

		KSMoon Moon( kd );
		Moon.findPosition( &num, &latB, &LST, &Earth );
		Moon.EquatorialToHorizontal( &LST, &latB );

		int result = 1;
		int jp = -1;
		while (result != 0 && jp < 10) {
			jp++;
			result = QString::compare( pNamei18n[jp] , planetB );
		}
		
		if (jp < 11) {
			if (jp < 10) {
				// Heliocentric Ecl. coords.
				hlongB.setD(PCat.findByName( pName[jp] )->helEcLong()->Degrees());
				hlatB.setD( PCat.findByName( pName[jp] )->helEcLat()->Degrees());
				rSunB = PCat.findByName( pName[jp] )->rsun();

				// Geocentric Ecl. coords.
				glongB .setD( PCat.findByName( pName[jp] )->ecLong()->Degrees() );
				glatB.setD( PCat.findByName( pName[jp] )->ecLat()->Degrees() );
				rEarthB = PCat.findByName( pName[jp] )->rearth();

				// Equatorial coords.
				decB.setD( PCat.findByName( pName[jp] )->dec()->Degrees() );
				raB.setD( PCat.findByName( pName[jp] )->ra()->Degrees() );

				// Topocentric Coords.
				azmB.setD( PCat.findByName( pName[jp] )->az()->Degrees() );
				altB.setD( PCat.findByName( pName[jp] )->alt()->Degrees() );
			} else {
			
				// Heliocentric Ecl. coords.
				hlongB.setD( Moon.helEcLong()->Degrees() );
				hlatB.setD( Moon.helEcLat()->Degrees() );

				// Geocentric Ecl. coords.
				glongB .setD( Moon.ecLong()->Degrees() );
				glatB.setD( Moon.ecLat()->Degrees() );
				rEarthB = Moon.rearth();

				// Equatorial coords.
				decB.setD( Moon.dec()->Degrees() );
				raB.setD( Moon.ra()->Degrees() );

				// Topocentric Coords.
				azmB.setD( Moon.az()->Degrees() );
				altB.setD( Moon.alt()->Degrees() );
			}
			

			ostream << lineToWrite;

			if ( helEclCheckBatch->isChecked() ) 
				ostream << hlongB.toDMSString() << space << hlatB.toDMSString() << space << rSunB << space ;
			if ( geoEclCheckBatch->isChecked() ) 
				ostream << glongB.toDMSString() << space << glatB.toDMSString() << space << rEarthB << space ;
			if ( equGeoCheckBatch->isChecked() ) 
				ostream << raB.toHMSString() << space << decB.toDMSString() << space ;
			if ( topoCheckBatch->isChecked() ) 
				ostream << azmB.toDMSString() << space << altB.toDMSString() << space ;

			ostream << endl;
		}
		nline++;

	}

	if (!lineIsValid) { 
		QString message = i18n("Errors found while parsing some lines in the input file");
		KMessageBox::sorry( 0, message, i18n( "Errors in lines" ) );
	}

	fOut.close();
}
