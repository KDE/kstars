/***************************************************************************
                          modcalcvlsr.cpp  -  description
                             -------------------
    begin                : sun mar 13 2005
    copyright            : (C) 2005 by Pablo de Vicente
    email                : p.devicente@wanadoo.es
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "modcalcvlsr.h"

#include "modcalcvlsr.moc"
#include "ksnumbers.h"
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


modCalcVlsr::modCalcVlsr(QWidget *parentSplit, const char *name) : modCalcVlsrDlg (parentSplit,name) {

	showCurrentDateTime();
 	initGeo();
	showLongLat();
	raBox->setDegType(FALSE);
	show();
}

modCalcVlsr::~modCalcVlsr(){
    delete geoPlace;
}

SkyPoint modCalcVlsr::getEquCoords (void)
{
	dms raCoord, decCoord;

	raCoord = raBox->createDms(FALSE);
	decCoord = decBox->createDms();

	SkyPoint sp = SkyPoint (raCoord, decCoord);

	return sp;
}

double modCalcVlsr::getVLSR (void)
{
	double vlsr = vlsrBox->text().toDouble();
	return vlsr;
}

double modCalcVlsr::getVhel (void)
{
	double vh = vHelioBox->text().toDouble();
	return vh;
}

double modCalcVlsr::getVgeo (void)
{
	double vg = vGeoBox->text().toDouble();
	return vg;
}

double modCalcVlsr::getVtopo (void)
{
	double vt = vTopoBox->text().toDouble();
	return vt;
}


KStarsDateTime modCalcVlsr::getDateTime (void)
{
	return KStarsDateTime( datBox->date() , timBox->time() );
}

double modCalcVlsr::getEpoch (QString eName)
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

dms modCalcVlsr::getLongitude(void)
{
	dms longitude;
	longitude = longBox->createDms();
	return longitude;
}

dms modCalcVlsr::getLatitude(void)
{
	dms latitude;
	latitude = latBox->createDms();
	return latitude;
}

double modCalcVlsr::getHeight(void)
{
	bool ok = false;
	double height = heightBox->text().toDouble(&ok);
	if (ok) 
		return height;
	else {
		kdDebug() << i18n( "Could not parse height string; assuming 0" ) << endl;
		return 0.0;
	}

}

void modCalcVlsr::getGeoLocation (void)
{
	geoPlace->setLong( getLongitude() );
	geoPlace->setLat(  getLatitude() );
	geoPlace->setHeight( getHeight() );

}

void modCalcVlsr::showCurrentDateTime (void)
{
	KStars *ks = (KStars*) parent()->parent()->parent();

	KStarsDateTime dt = ks->data()->geo()->LTtoUT( KStarsDateTime::currentDateTime() );

	datBox->setDate( dt.date() );
	timBox->setTime( dt.time() );
	dateBoxBatch->setDate( dt.date() );
	utBoxBatch->setTime( dt.time() );
}

// SIRVE para algo?
void modCalcVlsr::initGeo(void)
{
	KStars *ks = (KStars*) parent()->parent()->parent(); // QSplitter->AstroCalc->KStars
	geoPlace = new GeoLocation( ks->geo() );
}



void modCalcVlsr::showLongLat(void)
{

	KStars *ks = (KStars*) parent()->parent()->parent(); // QSplitter->AstroCalc->KStars
	longBox->show( ks->geo()->lng() );
	latBox->show( ks->geo()->lat() );
	longBoxBatch->show( ks->geo()->lng() );
	latBoxBatch->show( ks->geo()->lat() );
}

void modCalcVlsr::showVlsr (const double vlsr )
{
	vlsrBox->setText( KGlobal::locale()->formatNumber( vlsr ) );
}

void modCalcVlsr::showHelVel (const double vhel )
{
	vHelioBox->setText( KGlobal::locale()->formatNumber( vhel ) );
}

void modCalcVlsr::showGeoVel (const double vgeo )
{
	vGeoBox->setText( KGlobal::locale()->formatNumber( vgeo ) );
}

void modCalcVlsr::showTopoVel (const double vtop )
{
	vTopoBox->setText( KGlobal::locale()->formatNumber( vtop ) );
}

void modCalcVlsr::showEpoch( const KStarsDateTime &dt )
{
	double epochN = dt.epoch();
//	Localization
//	epochName->setText(KGlobal::locale()->formatNumber(epochN,3));
	epochName->setText( KGlobal::locale()->formatNumber( epochN ) );
	
}

void modCalcVlsr::slotClearCoords()
{

	raBox->clearFields();
	decBox->clearFields();
	longBox->clearFields();
	latBox->clearFields();
	epochName->setText("");
	vlsrBox->setText("");
	vHelioBox->setText("");
	vGeoBox->setText("");
	vTopoBox->setText("");

	datBox->setDate(ExtDate::currentDate());
	timBox->setTime(QTime(0,0,0));

}

void modCalcVlsr::slotComputeVelocities()
{
	SkyPoint sp = getEquCoords();
	getGeoLocation();


	double epoch0 = getEpoch( epochName->text() );
	KStarsDateTime dt0;
	dt0.setFromEpoch(epoch0);
	KStarsDateTime dt1 = getDateTime();
	double vst[3];
	dms gsidt = dt1.gst();
	geoPlace->TopocentricVelocity(vst, gsidt);

	if ( radioVlsr->isChecked() ) {
	
		double vlsr = getVLSR();
		double vhelio = sp.vHeliocentric(vlsr, dt0.djd() );
		showHelVel( vhelio );

		double vg = sp.vGeocentric(vhelio, dt1.djd());
		showGeoVel( vg );

		showTopoVel ( sp.vTopocentric(vg, vst) );
		
	} else if (radioVhelio->isChecked() ) {
		
		double vhel = getVhel();
		double vlsr = sp.vHelioToVlsr(vhel, dt0.djd() );
		showVlsr(vlsr);

		double vg = sp.vGeocentric(vhel, dt1.djd());
		showGeoVel( vg );

		showTopoVel ( sp.vTopocentric(vg, vst) );

	} else if (radioVgeo->isChecked() ) {

		double vgeo = getVgeo();
		double vhel = sp.vGeoToVHelio(vgeo, dt1.djd() );
		showHelVel(vhel) ;

		double vlsr = sp.vHelioToVlsr(vhel, dt0.djd() );
		showVlsr(vlsr);


		showTopoVel ( sp.vTopocentric(vgeo, vst) );

	} else {
		double vtopo = getVtopo();
		double vgeo = sp.vTopoToVGeo(vtopo, vst);
		showGeoVel( vgeo );

		double vhel = sp.vGeoToVHelio(vgeo, dt1.djd() );
		showHelVel(vhel) ;

		double vlsr = sp.vHelioToVlsr(vhel, dt0.djd() );
		showVlsr(vlsr);
	}

}

void modCalcVlsr::slotUtChecked(){
	if ( utCheckBatch->isChecked() )
		utBoxBatch->setEnabled( false );
	else {
		utBoxBatch->setEnabled( true );
	}
}

void modCalcVlsr::slotDateChecked(){
	if ( dateCheckBatch->isChecked() )
		dateBoxBatch->setEnabled( false );
	else {
		dateBoxBatch->setEnabled( true );
	}
}

void modCalcVlsr::slotRaChecked(){
	if ( raCheckBatch->isChecked() ) {
		raBoxBatch->setEnabled( false );
	}
	else {
		raBoxBatch->setEnabled( true );
	}
}

void modCalcVlsr::slotDecChecked(){
	if ( decCheckBatch->isChecked() ) {
		decBoxBatch->setEnabled( false );
	}
	else {
		decBoxBatch->setEnabled( true );
	}
}

void modCalcVlsr::slotEpochChecked(){
	if ( epochCheckBatch->isChecked() )
		epochBoxBatch->setEnabled( false );
	else 
		epochBoxBatch->setEnabled( true );
}

void modCalcVlsr::slotLongChecked(){
	if ( longCheckBatch->isChecked() )
		longBoxBatch->setEnabled( false );
	else 
		longBoxBatch->setEnabled( true );
}

void modCalcVlsr::slotLatChecked(){
	if ( latCheckBatch->isChecked() )
		latBoxBatch->setEnabled( false );
	else {
		latBoxBatch->setEnabled( true );
	}
}

void modCalcVlsr::slotHeightChecked(){
	if ( heightCheckBatch->isChecked() )
		heightBoxBatch->setEnabled( false );
	else {
		heightBoxBatch->setEnabled( true );
	}
}

void modCalcVlsr::slotVlsrChecked(){
	if ( vlsrCheckBatch->isChecked() )
		vlsrBoxBatch->setEnabled( false );
	else {
		vlsrBoxBatch->setEnabled( true );
	}
}

void modCalcVlsr::slotInputFile() {
	QString inputFileName;
	inputFileName = KFileDialog::getOpenFileName( );
	InputLineEditBatch->setText( inputFileName );
}

void modCalcVlsr::slotOutputFile() {
	QString outputFileName;
	outputFileName = KFileDialog::getSaveFileName( );
	OutputLineEditBatch->setText( outputFileName );
}

void modCalcVlsr::slotRunBatch() {
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

void modCalcVlsr::processLines( QTextStream &istream ) {

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
	long double jd0;
	SkyPoint spB;
	double sra, cra, sdc, cdc;
	dms raB, decB, latB, longB;
	double epoch0B, vhB, vgB, vtB, vlsrB, heightB;
	double vtopo[3];
	QTime utB;
	ExtDate dtB;
	KStarsDateTime dt0B;

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

		// Read vlsr and write in ostream if corresponds
	
		if(vlsrCheckBatch->isChecked() ) {
			vlsrB = fields[i].toDouble();
			i++;
		} else
			vlsrB = getEpoch( epochBoxBatch->text() );

		if ( allRadioBatch->isChecked() )
			ostream << vlsrB << space;
		else
			if(vlsrCheckBatch->isChecked() )
				ostream << vlsrB << space;
		
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
		
		// Read height and write in ostream if corresponds
	
		if(heightCheckBatch->isChecked() ) {
			heightB = fields[i].toDouble();
			i++;
		} else
			heightB = getEpoch( epochBoxBatch->text() );

		if ( allRadioBatch->isChecked() )
			ostream << heightB << space;
		else
			if(heightCheckBatch->isChecked() )
				ostream << heightB << space;
		
		// We make the first calculations

		spB = SkyPoint (raB, decB);
		dt0B.setFromEpoch(epoch0B);
		vhB = spB.vHeliocentric(vlsrB, dt0B.djd());
		jd0 = KStarsDateTime(dtB,utB).djd();
		vgB = spB.vGeocentric(vlsrB, jd0); 
		geoPlace->setLong( longB );
		geoPlace->setLat(  latB );
		geoPlace->setHeight( heightB );
		dms gsidt = KStarsDateTime(dtB,utB).gst();
		geoPlace->TopocentricVelocity(vtopo, gsidt);
		spB.ra()->SinCos(sra, cra);
		spB.dec()->SinCos(sdc, cdc);
		vtB = vgB - (vtopo[0]*cdc*cra + vtopo[1]*cdc*sra + vtopo[2]*sdc);

		ostream << vhB << space << vgB << space << vtB << endl;

	}


	fOut.close();
}
