/***************************************************************************
                          modcalcazel.cpp  -  description
                             -------------------
    begin                : s� oct 26 2002
    copyright            : (C) 2002 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "modcalcazel.h"

#include "modcalcazel.moc"
#include "dms.h"
#include "dmsbox.h"
#include "skypoint.h"
#include "geolocation.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "kstarsdatetime.h"
#include "libkdeedu/extdate/extdatetimeedit.h"

#include <qdatetimeedit.h>  //need for QTimeEdit
#include <qcheckbox.h>
#include <qradiobutton.h>
#include <qstring.h>
#include <qtextstream.h>
#include <kfiledialog.h>
#include <kmessagebox.h>


modCalcAzel::modCalcAzel(QWidget *parentSplit, const char *name) : modCalcAzelDlg (parentSplit,name) {

	showCurrentDateTime();
 	initGeo();
	showLongLat();
	horInputCoords = FALSE;
	raBox->setDegType(FALSE);
	show();
}

modCalcAzel::~modCalcAzel(){
    delete geoPlace;
}

SkyPoint modCalcAzel::getEquCoords (void)
{
	dms raCoord, decCoord;

	raCoord = raBox->createDms(FALSE);
	decCoord = decBox->createDms();

	SkyPoint sp = SkyPoint (raCoord, decCoord);

	return sp;
}

SkyPoint modCalcAzel::getHorCoords (void)
{
	dms azCoord, elCoord;

	azCoord = azBox->createDms();
	elCoord = elBox->createDms();

	SkyPoint sp = SkyPoint();

	sp.setAz(azCoord);
	sp.setAlt(elCoord);

	return sp;
}

void modCalcAzel::showCurrentDateTime (void)
{
	KStars *ks = (KStars*) parent()->parent()->parent();

	KStarsDateTime dt = ks->data()->geo()->LTtoUT( KStarsDateTime::currentDateTime() );

	datBox->setDate( dt.date() );
	timBox->setTime( dt.time() );
	dateBoxBatch->setDate( dt.date() );
	utBoxBatch->setTime( dt.time() );
}

KStarsDateTime modCalcAzel::getDateTime (void)
{
	return KStarsDateTime( datBox->date() , timBox->time() );
}

double modCalcAzel::getEpoch (QString eName)
{
	bool ok = false;
	double epoch = eName.toDouble(&ok);
	if ( ok )
		return epoch;
	else {
		kdDebug() << i18n( "Could not parse epoch string; assuming J2000" ) << endl;
		return 2000.0;
	}
}

dms modCalcAzel::getLongitude(void)
{
	dms longitude;
	longitude = longBox->createDms();
	return longitude;
}

dms modCalcAzel::getLatitude(void)
{
	dms latitude;
	latitude = latBox->createDms();
	return latitude;
}

void modCalcAzel::getGeoLocation (void)
{
	geoPlace->setLong( longBox->createDms() );
	geoPlace->setLat(  latBox->createDms() );
	geoPlace->setHeight( 0.0);

}

void modCalcAzel::initGeo(void)
{
	KStars *ks = (KStars*) parent()->parent()->parent(); // QSplitter->AstroCalc->KStars
	geoPlace = new GeoLocation( ks->geo() );
}



void modCalcAzel::showLongLat(void)
{

	KStars *ks = (KStars*) parent()->parent()->parent(); // QSplitter->AstroCalc->KStars
	longBox->show( ks->geo()->lng() );
	latBox->show( ks->geo()->lat() );
	longBoxBatch->show( ks->geo()->lng() );
	latBoxBatch->show( ks->geo()->lat() );
}

void modCalcAzel::showHorCoords ( SkyPoint sp )
{

	azBox->show( sp.az() );
	elBox->show( sp.alt() );

}

void modCalcAzel::showEquCoords ( SkyPoint sp )
{
	raBox->show( sp.ra(), FALSE );
	decBox->show( sp.dec() );
	showEpoch( getDateTime() );
}

void modCalcAzel::showEpoch( const KStarsDateTime &dt )
{
	double epochN = dt.epoch();
//	Localization
//	epochName->setText(KGlobal::locale()->formatNumber(epochN,3));
	epochName->setText( KGlobal::locale()->formatNumber( epochN ) );
	
}

void modCalcAzel::slotClearCoords()
{

	raBox->clearFields();
	decBox->clearFields();
	azBox->clearFields();
	elBox->clearFields();
	epochName->setText("");

	datBox->setDate(ExtDate::currentDate());
	timBox->setTime(QTime(0,0,0));

}

void modCalcAzel::slotComputeCoords()
{
	SkyPoint sp;
	double epoch0 = getEpoch( epochName->text() );
	KStarsDateTime dt;
	dt.setFromEpoch( epoch0 );
	long double jd = getDateTime().djd();
	long double jd0 = dt.djd();

	dms LST( getDateTime().gst().Degrees() + getLongitude().Degrees() );

	if(radioApCoords->isChecked()) {
		sp = getEquCoords();
		sp.apparentCoord(jd0, jd);
		dms lat(getLatitude());
		sp.EquatorialToHorizontal( &LST, &lat );
		showHorCoords( sp );

	} else {
		sp = getHorCoords();
		dms lat(getLatitude());
		sp.HorizontalToEquatorial( &LST, &lat );
		showEquCoords( sp );
	}

}
void modCalcAzel::slotUtChecked(){
	if ( utCheckBatch->isChecked() )
		utBoxBatch->setEnabled( false );
	else {
		utBoxBatch->setEnabled( true );
	}
}

void modCalcAzel::slotDateChecked(){
	if ( dateCheckBatch->isChecked() )
		dateBoxBatch->setEnabled( false );
	else {
		dateBoxBatch->setEnabled( true );
	}
}

void modCalcAzel::slotRaChecked(){
	if ( raCheckBatch->isChecked() ) {
		raBoxBatch->setEnabled( false );
		horNoCheck();
	}
	else {
		raBoxBatch->setEnabled( true );
	}
}

void modCalcAzel::slotDecChecked(){
	if ( decCheckBatch->isChecked() ) {
		decBoxBatch->setEnabled( false );
		horNoCheck();
	}
	else {
		decBoxBatch->setEnabled( true );
	}
}

void modCalcAzel::slotEpochChecked(){
	if ( epochCheckBatch->isChecked() )
		epochBoxBatch->setEnabled( false );
	else 
		epochBoxBatch->setEnabled( true );
}

void modCalcAzel::slotLongChecked(){
	if ( longCheckBatch->isChecked() )
		longBoxBatch->setEnabled( false );
	else 
		longBoxBatch->setEnabled( true );
}

void modCalcAzel::slotLatChecked(){
	if ( latCheckBatch->isChecked() )
		latBoxBatch->setEnabled( false );
	else {
		latBoxBatch->setEnabled( true );
	}
}

void modCalcAzel::slotAzChecked(){
	if ( azCheckBatch->isChecked() ) {
		azBoxBatch->setEnabled( false );
		equNoCheck();
	}
	else {
		azBoxBatch->setEnabled( true );
	}
}

void modCalcAzel::slotElChecked(){
	if ( elCheckBatch->isChecked() ) {
		elBoxBatch->setEnabled( false );
		equNoCheck();
	}
	else {
		elBoxBatch->setEnabled( true );
	}
}

void modCalcAzel::horNoCheck() {
	azCheckBatch->setChecked(false);
	azBoxBatch->setEnabled(false);
	elCheckBatch->setChecked(false);
	elBoxBatch->setEnabled(false);
	horInputCoords = FALSE;

}

void modCalcAzel::equNoCheck() {
	raCheckBatch->setChecked(false);
	raBoxBatch->setEnabled(false);
	decCheckBatch->setChecked(false);
	decBoxBatch->setEnabled(false);
	horInputCoords = TRUE;
}


void modCalcAzel::slotInputFile() {
	QString inputFileName;
	inputFileName = KFileDialog::getOpenFileName( );
	InputLineEditBatch->setText( inputFileName );
}

void modCalcAzel::slotOutputFile() {
	QString outputFileName;
	outputFileName = KFileDialog::getSaveFileName( );
	OutputLineEditBatch->setText( outputFileName );
}

void modCalcAzel::slotRunBatch() {
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

//		processLines(&f);
		QTextStream istream(&f);
		processLines(istream);
//		readFile( istream );
		f.close();
	} else  {
		QString message = i18n( "Invalid file: %1" ).arg( inputFileName );
		KMessageBox::sorry( 0, message, i18n( "Invalid file" ) );
		inputFileName = "";
		InputLineEditBatch->setText( inputFileName );
		return;
	}
}

void modCalcAzel::processLines( QTextStream &istream ) {

	// we open the output file

//	QTextStream istream(&fIn);
	QString outputFileName;
	outputFileName = OutputLineEditBatch->text();
	QFile fOut( outputFileName );
	fOut.open(IO_WriteOnly);
	QTextStream ostream(&fOut);

	QString line;
	QString space = " ";
	int i = 0;
	long double jd0, jdf;
	dms LST;
	SkyPoint sp;
	dms raB, decB, latB, longB, azB, elB;
	double epoch0B;
	QTime utB;
	ExtDate dtB;

	while ( ! istream.eof() ) {
		line = istream.readLine();
		line.stripWhiteSpace();

		//Go through the line, looking for parameters

		QStringList fields = QStringList::split( " ", line );

		i = 0;

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
		
		// Read Epoch and write in ostream if corresponds
	
		if(epochCheckBatch->isChecked() ) {
			epoch0B = fields[i].toDouble();
			i++;
		} else
			epoch0B = getEpoch( epochBoxBatch->text() );

		if ( allRadioBatch->isChecked() )
			ostream << epoch0B << space;
		else
			if(epochCheckBatch->isChecked() )
				ostream << epoch0B << space;

		// We make the first calculations
		KStarsDateTime dt;
		dt.setFromEpoch( epoch0B );
		jdf = KStarsDateTime(dtB,utB).djd();
		jd0 = dt.djd();

		LST = KStarsDateTime(dtB,utB).gst().Degrees() + longB.Degrees();
		
		// Equatorial coordinates are the input coords.
		if (!horInputCoords) {
		// Read RA and write in ostream if corresponds

			if(raCheckBatch->isChecked() ) {
				raB = dms::fromString( fields[i],FALSE);
				i++;
			} else
				raB = raBoxBatch->createDms(FALSE);

			if ( allRadioBatch->isChecked() )
				ostream << raB.toHMSString() << space;
			else
				if(raCheckBatch->isChecked() )
					ostream << raB.toHMSString() << space;

			// Read DEC and write in ostream if corresponds

			if(decCheckBatch->isChecked() ) {
				decB = dms::fromString( fields[i], TRUE);
				i++;
			} else
				decB = decBoxBatch->createDms();

			if ( allRadioBatch->isChecked() )
				ostream << decB.toDMSString() << space;
			else
				if(decCheckBatch->isChecked() )
					ostream << decB.toDMSString() << space;

			sp = SkyPoint (raB, decB);
			sp.apparentCoord(jd0, jdf);
			sp.EquatorialToHorizontal( &LST, &latB );
			ostream << sp.az()->toDMSString() << space << sp.alt()->toDMSString() << endl;

		// Input coords are horizontal coordinates
		
		} else {
			if(azCheckBatch->isChecked() ) {
				azB = dms::fromString( fields[i],FALSE);
				i++;
			} else
				azB = azBoxBatch->createDms();

			if ( allRadioBatch->isChecked() )
				ostream << azB.toHMSString() << space;
			else
				if(raCheckBatch->isChecked() )
					ostream << azB.toHMSString() << space;

			// Read DEC and write in ostream if corresponds

			if(elCheckBatch->isChecked() ) {
				elB = dms::fromString( fields[i], TRUE);
				i++;
			} else
				elB = decBoxBatch->createDms();

			if ( allRadioBatch->isChecked() )
				ostream << elB.toDMSString() << space;
			else
				if(elCheckBatch->isChecked() )
					ostream << elB.toDMSString() << space;

			sp.setAz(azB);
			sp.setAlt(elB);
			sp.HorizontalToEquatorial( &LST, &latB );
			ostream << sp.ra()->toHMSString() << space << sp.dec()->toDMSString() << endl;
		}

	}


	fOut.close();
}
