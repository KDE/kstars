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

//#include <q3datetimeedit.h>  //need for QTimeEdit
//#include <qcheckbox.h>
//#include <qradiobutton.h>
//#include <qstring.h>
//#include <qtextstream.h>
#include <kfiledialog.h>
#include <kmessagebox.h>

#include "modcalcvlsr.h"

#include "ksnumbers.h"
#include "dms.h"
#include "skypoint.h"
#include "geolocation.h"
#include "kstars.h"
#include "kstarsdatetime.h"
#include "widgets/dmsbox.h"
#include "libkdeedu/extdate/extdatetimeedit.h"

modCalcVlsr::modCalcVlsr(QWidget *parentSplit) : QFrame(parentSplit) {

	setupUi( parentSplit );
	showCurrentDateTime();
 	initGeo();
	showLongLat();
	RABox->setDegType(FALSE);
	show();
}

modCalcVlsr::~modCalcVlsr(){
    delete geoPlace;
}

SkyPoint modCalcVlsr::getEquCoords (void)
{
	dms raCoord, decCoord;

	raCoord = RABox->createDms(FALSE);
	decCoord = DecBox->createDms();

	SkyPoint sp = SkyPoint (raCoord, decCoord);

	return sp;
}

double modCalcVlsr::getVLSR (void)
{
	double vlsr = VLSRBox->text().toDouble();
	return vlsr;
}

double modCalcVlsr::getVhel (void)
{
	double vh = VHelioBox->text().toDouble();
	return vh;
}

double modCalcVlsr::getVgeo (void)
{
	double vg = VGeoBox->text().toDouble();
	return vg;
}

double modCalcVlsr::getVtopo (void)
{
	double vt = VTopoBox->text().toDouble();
	return vt;
}


KStarsDateTime modCalcVlsr::getDateTime (void)
{
	return KStarsDateTime( DateBox->date() , UTBox->time() );
}

dms modCalcVlsr::getLongitude(void)
{
	dms longitude;
	longitude = LongitudeBox->createDms();
	return longitude;
}

dms modCalcVlsr::getLatitude(void)
{
	dms latitude;
	latitude = LatitudeBox->createDms();
	return latitude;
}

double modCalcVlsr::getHeight(void)
{
	bool ok = false;
	double height = ElevationBox->text().toDouble(&ok);
	if (ok) 
		return height;
	else {
		kDebug() << i18n( "Could not parse height string; assuming 0" ) << endl;
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
	KStarsDateTime dt( KStarsDateTime::currentDateTime() );

	DateBox->setDate( dt.date() );
	UTBox->setTime( dt.time() );
	DateBoxBatch->setDate( dt.date() );
	UTBoxBatch->setTime( dt.time() );
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
	LongitudeBox->show( ks->geo()->lng() );
	LatitudeBox->show( ks->geo()->lat() );
	LongitudeBoxBatch->show( ks->geo()->lng() );
	LatitudeBoxBatch->show( ks->geo()->lat() );
}

void modCalcVlsr::showVlsr (const double vlsr )
{
	VLSRBox->setText( KGlobal::locale()->formatNumber( vlsr ) );
}

void modCalcVlsr::showHelVel (const double vhel )
{
	VHelioBox->setText( KGlobal::locale()->formatNumber( vhel ) );
}

void modCalcVlsr::showGeoVel (const double vgeo )
{
	VGeoBox->setText( KGlobal::locale()->formatNumber( vgeo ) );
}

void modCalcVlsr::showTopoVel (const double vtop )
{
	VTopoBox->setText( KGlobal::locale()->formatNumber( vtop ) );
}

void modCalcVlsr::showEpoch( const KStarsDateTime &dt )
{
	double epochN = dt.epoch();
//	Localization
//	EpochBox->setText(KGlobal::locale()->formatNumber(epochN,3));
	EpochBox->setText( KGlobal::locale()->formatNumber( epochN ) );
	
}

void modCalcVlsr::slotClearCoords()
{

	RABox->clearFields();
	DecBox->clearFields();
	LongitudeBox->clearFields();
	LatitudeBox->clearFields();
	EpochBox->setText(QString());
	VLSRBox->setText(QString());
	VHelioBox->setText(QString());
	VGeoBox->setText(QString());
	VTopoBox->setText(QString());

	DateBox->setDate(ExtDate::currentDate());
	UTBox->setTime(QTime(0,0,0));

}

void modCalcVlsr::slotComputeVelocities()
{
	SkyPoint sp = getEquCoords();
	getGeoLocation();


	QString epoch0 = EpochBox->text();
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
	if ( UTCheckBatch->isChecked() )
		UTBoxBatch->setEnabled( false );
	else {
		UTBoxBatch->setEnabled( true );
	}
}

void modCalcVlsr::slotDateChecked(){
	if ( DateCheckBatch->isChecked() )
		DateBoxBatch->setEnabled( false );
	else {
		DateBoxBatch->setEnabled( true );
	}
}

void modCalcVlsr::slotRaChecked(){
	if ( RACheckBatch->isChecked() ) {
		RABoxBatch->setEnabled( false );
	}
	else {
		RABoxBatch->setEnabled( true );
	}
}

void modCalcVlsr::slotDecChecked(){
	if ( DecCheckBatch->isChecked() ) {
		DecBoxBatch->setEnabled( false );
	}
	else {
		DecBoxBatch->setEnabled( true );
	}
}

void modCalcVlsr::slotEpochChecked(){
	if ( EpochCheckBatch->isChecked() )
		EpochBoxBatch->setEnabled( false );
	else 
		EpochBoxBatch->setEnabled( true );
}

void modCalcVlsr::slotLongChecked(){
	if ( LongCheckBatch->isChecked() )
		LongitudeBoxBatch->setEnabled( false );
	else 
		LongitudeBoxBatch->setEnabled( true );
}

void modCalcVlsr::slotLatChecked(){
	if ( LatCheckBatch->isChecked() )
		LatitudeBoxBatch->setEnabled( false );
	else {
		LatitudeBoxBatch->setEnabled( true );
	}
}

void modCalcVlsr::slotHeightChecked(){
	if ( ElevationCheckBatch->isChecked() )
		ElevationBoxBatch->setEnabled( false );
	else {
		ElevationBoxBatch->setEnabled( true );
	}
}

void modCalcVlsr::slotVlsrChecked(){
	if ( InputVelocityCheckBatch->isChecked() )
		InputVelocityBoxBatch->setEnabled( false );
	else {
		InputVelocityBoxBatch->setEnabled( true );
	}
}

void modCalcVlsr::slotInputFile() {
	QString inputFileName;
	inputFileName = KFileDialog::getOpenFileName( );
	InputFileBoxBatch->setURL( inputFileName );
}

void modCalcVlsr::slotOutputFile() {
	QString outputFileName;
	outputFileName = KFileDialog::getSaveFileName( );
	OutputFileBoxBatch->setURL( outputFileName );
}

void modCalcVlsr::slotRunBatch() {
	QString inputFileName;

	inputFileName = InputFileBoxBatch->url();

	// We open the input file and read its content

	if ( QFile::exists(inputFileName) ) {
		QFile f( inputFileName );
		if ( !f.open( QIODevice::ReadOnly) ) {
			QString message = i18n( "Could not open file %1.").arg( f.name() );
			KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
			inputFileName = QString();
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
		inputFileName = QString();
		InputFileBoxBatch->setURL( inputFileName );
		return;
	}
}

void modCalcVlsr::processLines( QTextStream &istream ) {

	// we open the output file

//	QTextStream istream(&fIn);
	QString outputFileName;
	outputFileName = OutputFileBoxBatch->url();
	QFile fOut( outputFileName );
	fOut.open(QIODevice::WriteOnly);
	QTextStream ostream(&fOut);

	QString line;
	QString space = " ";
	int i = 0;
	long double jd0;
	SkyPoint spB;
	double sra, cra, sdc, cdc;
	dms raB, decB, latB, longB;
	QString epoch0B;
	double vhB, vgB, vtB, vlsrB, heightB;
	double vtopo[3];
	QTime utB;
	ExtDate dtB;
	KStarsDateTime dt0B;

	while ( ! istream.atEnd() ) {
		line = istream.readLine();
		line.trimmed();

		//Go through the line, looking for parameters

		QStringList fields = QStringList::split( " ", line );

		i = 0;

		// Read Ut and write in ostream if corresponds
		
		if(UTCheckBatch->isChecked() ) {
			utB = QTime::fromString( fields[i] );
			i++;
		} else
			utB = UTBoxBatch->time();
		
		if ( AllRadioBatch->isChecked() )
			ostream << utB.toString() << space;
		else
			if(UTCheckBatch->isChecked() )
				ostream << utB.toString() << space;
			
		// Read date and write in ostream if corresponds
		
		if(DateCheckBatch->isChecked() ) {
			 dtB = ExtDate::fromString( fields[i] );
			 i++;
		} else
			dtB = DateBoxBatch->date();
		if ( AllRadioBatch->isChecked() )
			ostream << dtB.toString().append(space);
		else
			if(DateCheckBatch->isChecked() )
			 	ostream << dtB.toString().append(space);
		
		// Read RA and write in ostream if corresponds

		if(RACheckBatch->isChecked() ) {
			raB = dms::fromString( fields[i],FALSE);
			i++;
		} else
			raB = RABoxBatch->createDms(FALSE);

		if ( AllRadioBatch->isChecked() )
			ostream << raB.toHMSString() << space;
		else
			if(RACheckBatch->isChecked() )
				ostream << raB.toHMSString() << space;

		// Read DEC and write in ostream if corresponds

		if(DecCheckBatch->isChecked() ) {
			decB = dms::fromString( fields[i], TRUE);
			i++;
		} else
			decB = DecBoxBatch->createDms();

		if ( AllRadioBatch->isChecked() )
			ostream << decB.toDMSString() << space;
		else
			if(DecCheckBatch->isChecked() )
				ostream << decB.toDMSString() << space;

		// Read Epoch and write in ostream if corresponds
	
		if(EpochCheckBatch->isChecked() ) {
			epoch0B = fields[i];
			i++;
		} else
			epoch0B = EpochBoxBatch->text();

		if ( AllRadioBatch->isChecked() )
			ostream << epoch0B << space;
		else
			if(EpochCheckBatch->isChecked() )
				ostream << epoch0B << space;

		// Read vlsr and write in ostream if corresponds
	
		if(InputVelocityCheckBatch->isChecked() ) {
			vlsrB = fields[i].toDouble();
			i++;
		} else
			vlsrB = InputVelocityComboBatch->currentText().toDouble();

		if ( AllRadioBatch->isChecked() )
			ostream << vlsrB << space;
		else
			if(InputVelocityCheckBatch->isChecked() )
				ostream << vlsrB << space;
		
		// Read Longitude and write in ostream if corresponds
		
		if (LongCheckBatch->isChecked() ) {
			longB = dms::fromString( fields[i],TRUE);
			i++;
		} else
			longB = LongitudeBoxBatch->createDms(TRUE);
		
		if ( AllRadioBatch->isChecked() )
			ostream << longB.toDMSString() << space;
		else
			if (LongCheckBatch->isChecked() )
				ostream << longB.toDMSString() << space;
		
		// Read Latitude


		if (LatCheckBatch->isChecked() ) {
			latB = dms::fromString( fields[i], TRUE);
			i++;
		} else
			latB = LatitudeBoxBatch->createDms(TRUE);
		if ( AllRadioBatch->isChecked() )
			ostream << latB.toDMSString() << space;
		else
			if (LatCheckBatch->isChecked() )
				ostream << latB.toDMSString() << space;
		
		// Read height and write in ostream if corresponds
	
		if(ElevationCheckBatch->isChecked() ) {
			heightB = fields[i].toDouble();
			i++;
		} else
			heightB = ElevationBoxBatch->text().toDouble();

		if ( AllRadioBatch->isChecked() )
			ostream << heightB << space;
		else
			if(ElevationCheckBatch->isChecked() )
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

#include "modcalcvlsr.moc"
