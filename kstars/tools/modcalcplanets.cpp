/***************************************************************************
                          modcalcequinox.cpp  -  description
                             -------------------
    begin                : dom may 2 2004
    copyright            : (C) 2004 by Pablo de Vicente
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
#include "ksutils.h"
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
	this->show();
}

modCalcPlanets::~modCalcPlanets(){
}

void modCalcPlanets::showCurrentDateTime (void)
{
	ExtDateTime dt = ExtDateTime::currentDateTime();

	dateBox->setDate( dt.date() );
	timeBox->setTime( dt.time() );
}

ExtDateTime modCalcPlanets::getExtDateTime (void)
{
	ExtDateTime dt ( dateBox->date() , timeBox->time() );

	return dt;
}

void modCalcPlanets::showLongLat(void)
{

	KStars *ks = (KStars*) parent()->parent()->parent(); // QSplitter->AstroCalc->KStars
	longBox->show( ks->geo()->lng() );
	latBox->show( ks->geo()->lat() );
}


GeoLocation modCalcPlanets::getObserverPosition (void)
{
	GeoLocation geoPlace;

	geoPlace.setLong( longBox->createDms() );
	geoPlace.setLat(  latBox->createDms() );
	geoPlace.setHeight( 0.0 );

	return geoPlace;
}

long double modCalcPlanets::computeJdFromCalendar (ExtDateTime edt)
{
	long double julianDay;
	julianDay = KSUtils::UTtoJD( edt );

	return julianDay;
}

void modCalcPlanets::slotComputePosition (void)
{
	KStarsData *kd = (KStarsData*) parent()->parent()->parent();
	ExtDateTime edt = getExtDateTime(); 
	long double julianDay = computeJdFromCalendar(edt);
	GeoLocation *position = new GeoLocation( getObserverPosition() );

	KSNumbers *num = new KSNumbers(julianDay);
	dms *LST = new dms( KSUtils::UTtoLST( edt, position->lng() ) );

	// We create the Earth and the Sun
	KSPlanet *Earth = new KSPlanet( kd, I18N_NOOP( "Earth" ));
	Earth->findPosition(num);
//	KSPlanet *Sun = new KSSun( kd );
//	Sun->findPosition(num, position->lat(), LST, Earth);
	
	// Mercury
	if (planetComboBox->currentItem() == 0 ) {
		KSPlanet *ksp = new KSPlanet( kd, I18N_NOOP( "Mercury" ));
		ksp->findPosition(num, position->lat(), LST, Earth);
		ksp->EquatorialToHorizontal(position->lat(), LST);
		showCoordinates(ksp);
	}
	else if(planetComboBox->currentItem() == 1) {
		KSPlanet *ksp = new KSPlanet( kd, I18N_NOOP( "Venus" ));
		ksp->findPosition(num, position->lat(), LST, Earth);
		ksp->EquatorialToHorizontal(position->lat(), LST);
		showCoordinates(ksp);
	}
	else if(planetComboBox->currentItem() == 2) {
		showCoordinates(Earth);
	}
	else if(planetComboBox->currentItem() == 3) {
		KSPlanet *ksp = new KSPlanet( kd, I18N_NOOP( "Mars" ));
		ksp->findPosition(num, position->lat(), LST, Earth);
		ksp->EquatorialToHorizontal(position->lat(), LST);
		showCoordinates(ksp);
	}
	else if(planetComboBox->currentItem() == 4) {
		KSPlanet *ksp = new KSPlanet( kd, I18N_NOOP( "Jupiter" ));
		ksp->findPosition(num, position->lat(), LST, Earth);
		ksp->EquatorialToHorizontal(position->lat(), LST);
		showCoordinates(ksp);
	}
	else if(planetComboBox->currentItem() == 5) {
		KSPlanet *ksp = new KSPlanet( kd, I18N_NOOP( "Saturn" ));
		ksp->findPosition(num, position->lat(), LST, Earth);
		ksp->EquatorialToHorizontal(position->lat(), LST);
		showCoordinates(ksp);
	}
	else if(planetComboBox->currentItem() == 6) {
		KSPlanet *ksp = new KSPlanet( kd, I18N_NOOP( "Uranus" ));
		ksp->findPosition(num, position->lat(), LST, Earth);
		ksp->EquatorialToHorizontal(position->lat(), LST);
		showCoordinates(ksp);
	}
	else if(planetComboBox->currentItem() == 7) {
		KSPlanet *ksp = new KSPlanet( kd, I18N_NOOP( "Neptune" ));
		ksp->findPosition(num, position->lat(), LST, Earth);
		ksp->EquatorialToHorizontal(position->lat(), LST);
		showCoordinates(ksp);
	}
	else if(planetComboBox->currentItem() == 8) {
		KSPluto *ksp = new KSPluto( kd );
		ksp->findPosition(num, position->lat(), LST, Earth);
		ksp->EquatorialToHorizontal(position->lat(), LST);
		showCoordinates(ksp);
	}
	else if(planetComboBox->currentItem() == 9) {
		KSMoon *ksp = new KSMoon( kd );
		ksp->findPosition(num, position->lat(), LST, Earth);
		ksp->EquatorialToHorizontal(position->lat(), LST);
		showCoordinates(ksp);
	}
	else if(planetComboBox->currentItem() == 10) {
		KSSun *ksp = new KSSun( kd);
		ksp->findPosition(num, position->lat(), LST);
		ksp->EquatorialToHorizontal(position->lat(), LST);
		ksp->setRsun(0.0);
		showCoordinates(ksp);
	}
	
}

void modCalcPlanets::showCoordinates(KSPlanet *ksp) {

	showHeliocentricEclipticCoords(ksp->helEcLong(), ksp->helEcLat(), ksp->rsun() );
	showGeocentricEclipticCoords(ksp->ecLong(), ksp->ecLat(), ksp->rearth() );
	showEquatorialCoords(ksp->ra(), ksp->dec() );
	showTopocentricCoords(ksp->az(), ksp->alt() );
	
}

void modCalcPlanets::showCoordinates(KSMoon *ksp) {

	showHeliocentricEclipticCoords(ksp->helEcLong(), ksp->helEcLat(), ksp->rsun() );
	showGeocentricEclipticCoords(ksp->ecLong(), ksp->ecLat(), ksp->rearth() );
	showEquatorialCoords(ksp->ra(), ksp->dec() );
	showTopocentricCoords(ksp->az(), ksp->alt() );
	
}
void modCalcPlanets::showCoordinates(KSPluto *ksp) {

	showHeliocentricEclipticCoords(ksp->helEcLong(), ksp->helEcLat(), ksp->rsun() );
	showGeocentricEclipticCoords(ksp->ecLong(), ksp->ecLat(), ksp->rearth() );
	showEquatorialCoords(ksp->ra(), ksp->dec() );
	showTopocentricCoords(ksp->az(), ksp->alt() );
	
}

void modCalcPlanets::showCoordinates(KSSun *ksp) {

	showHeliocentricEclipticCoords(ksp->helEcLong(), ksp->helEcLat(), ksp->rsun() );
	showGeocentricEclipticCoords(ksp->ecLong(), ksp->ecLat(), ksp->rearth() );
	showEquatorialCoords(ksp->ra(), ksp->dec() );
	showTopocentricCoords(ksp->az(), ksp->alt() );
	
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
			QString message = i18n( "Could not open file %1"
			).arg( f.name() );
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

void modCalcPlanets::processLines( QTextStream &istream ) {

	// we open the output file

	QString outputFileName;
	outputFileName = OutputLineEditBatch->text();
	QFile fOut( outputFileName );
	fOut.open(IO_WriteOnly);
	QTextStream ostream(&fOut);

	QString line;
	QString space = " ";
	QString *planetB;
	int yearB;
	int i = 0;
	QTime utB;
	ExtDate dtB;
	dms longB, latB;
	KStarsData *kd = (KStarsData*) parent()->parent()->parent();
	KSPlanet *Earth = new KSPlanet( kd, I18N_NOOP( "Earth" ));
	KSNumbers *num;
	
	while ( ! istream.eof() ) {
		line = istream.readLine();
		line.stripWhiteSpace();

		//Go through the line, looking for parameters

		QStringList fields = QStringList::split( " ", line );

		i = 0;
		if(planetCheckBatch->isChecked() ) {
			planetB = new QString( fields[i] );
			i++;
		} else
			planetB = new QString( planetComboBoxBatch->currentText( ) );
		
		if ( allRadioBatch->isChecked() )
			ostream << planetB << space;
		else
			if(planetCheckBatch->isChecked() )
				ostream << planetB << space;
		
		// Read Ut and write in ostream if corresponds
		
		if(utCheckBatch->isChecked() ) {
			utB = QTime::fromString( fields[i] );
			i++;
		} else
			utB = utBoxBatch->time();
		
		if ( allRadioBatch->isChecked() )
			ostream << utB.toString() << space;
		else
			if(utCheckBatch->isChecked() )
				ostream << utB.toString() << space;
			
		// Read date and write in ostream if corresponds
		
		if(dateCheckBatch->isChecked() ) {
			 dtB = ExtDate::fromString( fields[i] );
			 i++;
		} else
			dtB = dateBoxBatch->date();
		if ( allRadioBatch->isChecked() )
			ostream << dtB.toString().append(space);
		else
			if(dateCheckBatch->isChecked() )
			 	ostream << dtB.toString().append(space);
		
		// Read Longitude and write in ostream if corresponds
		
		if (longCheckBatch->isChecked() ) {
			longB = dms::fromString( fields[i],TRUE);
			i++;
		} else
			longB = longBoxBatch->createDms(TRUE);
		
		if ( allRadioBatch->isChecked() )
			ostream << longB.toDMSString() << space;
		else
			if (longCheckBatch->isChecked() )
				ostream << longB.toDMSString() << space;
		
		// Read Latitude

		if (latCheckBatch->isChecked() ) {
			latB = dms::fromString( fields[i], TRUE);
			i++;
		} else
			latB = latBoxBatch->createDms(TRUE);
		if ( allRadioBatch->isChecked() )
			ostream << latB.toDMSString() << space;
		else
			if (latCheckBatch->isChecked() )
				ostream << latB.toDMSString() << space;	


//		ostream << dts.toString(Qt::ISODate) << space << (float)(jdsu - jdsp) << space <<
//			dtu.toString(Qt::ISODate) << space << (float)(jdau - jdsu) << space <<
//			dta.toString(Qt::ISODate) << space << (float)(jdwin - jdau) << space <<
//			dtw.toString(Qt::ISODate) << space << (float)(jdsp1 - jdwin) << endl;
	}


	fOut.close();
}
