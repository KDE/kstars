/***************************************************************************
                          modcalcgeodcoord.cpp  -  description
                             -------------------
    begin                : Tue Jan 15 2002
    copyright            : (C) 2002 by Pablo de Vicente
    email                : vicente@oan.es
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "modcalcgeodcoord.h"
#include "modcalcgeodcoord.moc"
#include "dms.h"
#include "dmsbox.h"
#include "geolocation.h"
#include "kstars.h"

#include <qradiobutton.h>
#include <qstring.h>
#include <qcheckbox.h>
#include <qtextstream.h>

#include <kcombobox.h>
#include <klineedit.h>
#include <kapplication.h>
#include <knumvalidator.h>
#include <klocale.h>
#include <klineedit.h>
#include <kfiledialog.h>
#include <kmessagebox.h>


modCalcGeodCoord::modCalcGeodCoord(QWidget *parentSplit, const char *name) : modCalcGeodCoordDlg(parentSplit,name) {

	static const char *ellipsoidList[] = {
    "IAU76", "GRS80", "MERIT83", "WGS84", "IERS89"};

	spheRadio->setChecked(TRUE);
	ellipsoidBox->insertStrList (ellipsoidList,5);
	geoPlace = new GeoLocation();
	showLongLat();
	setEllipsoid(0);
	show();

}

modCalcGeodCoord::~modCalcGeodCoord(){
    delete geoPlace;
}

void modCalcGeodCoord::showLongLat(void)
{

	KStars *ks = (KStars*) parent()->parent()->parent(); // QSplitter->AstroCalc->KStars
	lonGeoBox->show( ks->geo()->lng() );
	latGeoBox->show( ks->geo()->lat() );
	altGeoBox->setText( QString("0.0") );
}

void modCalcGeodCoord::setEllipsoid(int index) {

	geoPlace->changeEllipsoid(index);

}

void modCalcGeodCoord::getCartGeoCoords (void)
{

	geoPlace->setXPos( KGlobal::locale()->readNumber(xGeoName->text())*1000. );
	geoPlace->setYPos( KGlobal::locale()->readNumber(yGeoName->text())*1000. );
	geoPlace->setZPos( KGlobal::locale()->readNumber(zGeoName->text())*1000. );
	//geoPlace->setXPos( xGeoName->text().toDouble()*1000. );
	//geoPlace->setYPos( yGeoName->text().toDouble()*1000. );
	//geoPlace->setZPos( zGeoName->text().toDouble()*1000. );
}

void modCalcGeodCoord::getSphGeoCoords (void)
{
	geoPlace->setLong( lonGeoBox->createDms() );
	geoPlace->setLat(  latGeoBox->createDms() );
	geoPlace->setHeight( altGeoBox->text().toDouble() );
}

void modCalcGeodCoord::slotClearGeoCoords (void)
{

	geoPlace->setLong( 0.0 );
	geoPlace->setLat(  0.0 );
	geoPlace->setHeight( 0.0 );
	latGeoBox->clearFields();
	lonGeoBox->clearFields();
	//showSpheGeoCoords();
	//showCartGeoCoords();

}

void modCalcGeodCoord::slotComputeGeoCoords (void)
{

	if(cartRadio->isChecked()) {
		getCartGeoCoords();
		showSpheGeoCoords();
	} else {
		getSphGeoCoords();
		showCartGeoCoords();
	}

}

void modCalcGeodCoord::showSpheGeoCoords(void)
{
	lonGeoBox->show( geoPlace->lng() );
	latGeoBox->show( geoPlace->lat() );
	altGeoBox->setText( KGlobal::locale()->formatNumber( geoPlace->height(), 3) );
}

void modCalcGeodCoord::showCartGeoCoords(void)
{

	xGeoName->setText( KGlobal::locale()->formatNumber( geoPlace->xPos()/1000. ,6));
	yGeoName->setText( KGlobal::locale()->formatNumber( geoPlace->yPos()/1000. ,6));
	zGeoName->setText( KGlobal::locale()->formatNumber( geoPlace->zPos()/1000. ,6));
}

void modCalcGeodCoord::geoCheck(void) {

	xBoxBatch->setEnabled( false );
	xCheckBatch->setChecked( false );
	yBoxBatch->setEnabled( false );
	yCheckBatch->setChecked( false );
	zBoxBatch->setEnabled( false );
	zCheckBatch->setChecked( false );
	xyzInputCoords = FALSE;
}

void modCalcGeodCoord::xyzCheck(void) {

	longBoxBatch->setEnabled( false );
	longCheckBatch->setChecked( false );
	latBoxBatch->setEnabled( false );
	latCheckBatch->setChecked( false );
	elevBoxBatch->setEnabled( false );
	elevCheckBatch->setChecked( false );
	xyzInputCoords = TRUE;

}

void modCalcGeodCoord::slotLongCheckedBatch(){

	if ( longCheckBatch->isChecked() ) {
		longBoxBatch->setEnabled( false );
		geoCheck();
	} else {
		longBoxBatch->setEnabled( true );
	}
}

void modCalcGeodCoord::slotLatCheckedBatch(){

	if ( latCheckBatch->isChecked() ) {
		latBoxBatch->setEnabled( false );
		geoCheck();
	} else {
		latBoxBatch->setEnabled( true );
	}
}

void modCalcGeodCoord::slotElevCheckedBatch(){

	if ( elevCheckBatch->isChecked() ) {
		elevBoxBatch->setEnabled( false );
		geoCheck();
	} else {
		elevBoxBatch->setEnabled( true );
	}
}

void modCalcGeodCoord::slotXCheckedBatch(){

	if ( xCheckBatch->isChecked() ) {
		xBoxBatch->setEnabled( false );
		xyzCheck();
	} else {
		xBoxBatch->setEnabled( true );
	}
}

void modCalcGeodCoord::slotYCheckedBatch(){

	if ( yCheckBatch->isChecked() ) {
		yBoxBatch->setEnabled( false );
		xyzCheck();
	} else {
		yBoxBatch->setEnabled( true );
	}
}

void modCalcGeodCoord::slotZCheckedBatch(){

	if ( zCheckBatch->isChecked() ) {
		zBoxBatch->setEnabled( false );
		xyzCheck();
	} else {
		zBoxBatch->setEnabled( true );
	}
}
void modCalcGeodCoord::slotInputFile() {

	QString inputFileName;
	inputFileName = KFileDialog::getOpenFileName( );
	InputLineEditBatch->setText( inputFileName );
}

void modCalcGeodCoord::slotOutputFile() {

	QString outputFileName;
	outputFileName = KFileDialog::getSaveFileName( );
	OutputLineEditBatch->setText( outputFileName );
}

void modCalcGeodCoord::slotRunBatch(void) {

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

void modCalcGeodCoord::processLines( QTextStream &istream ) {

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
	GeoLocation *geoPl = new GeoLocation();
	geoPl->setEllipsoid(0);

	double xB, yB, zB, hB;
	dms latB, longB;


	while ( ! istream.eof() ) {
		line = istream.readLine();
		line.stripWhiteSpace();

		//Go through the line, looking for parameters

		QStringList fields = QStringList::split( " ", line );

		i = 0;

		// Input coords are XYZ:

		if (xyzInputCoords) {

			// Read X and write in ostream if corresponds

			if(xCheckBatch->isChecked() ) {
				xB = fields[i].toDouble();
				i++;
			} else
				xB = KGlobal::locale()->readNumber(xBoxBatch->text()) ;

			if ( allRadioBatch->isChecked() )
				ostream << xB << space;
			else
				if(xCheckBatch->isChecked() )
					ostream << xB << space;

			// Read Y and write in ostream if corresponds

			if(yCheckBatch->isChecked() ) {
				yB = fields[i].toDouble();
				i++;
			} else
				yB = KGlobal::locale()->readNumber( yBoxBatch->text()) ;

			if ( allRadioBatch->isChecked() )
				ostream << yB << space;
			else
				if(yCheckBatch->isChecked() )
					ostream << yB << space;
			// Read Z and write in ostream if corresponds

			if(zCheckBatch->isChecked() ) {
				zB = fields[i].toDouble();
				i++;
			} else
				zB = KGlobal::locale()->readNumber( zBoxBatch->text());

			if ( allRadioBatch->isChecked() )
				ostream << zB << space;
			else
				if(yCheckBatch->isChecked() )
					ostream << zB << space;

			geoPl->setXPos( xB*1000.0 );
			geoPl->setYPos( yB*1000.0 );
			geoPl->setZPos( zB*1000.0 );
			ostream << geoPl->lng()->toDMSString() << space <<
				geoPl->lat()->toDMSString() << space <<
				geoPl->height() << endl;

		// Input coords. are Long, Lat and Height

		} else {

			// Read Longitude and write in ostream if corresponds

			if(longCheckBatch->isChecked() ) {
				longB = dms::fromString( fields[i],TRUE);
				i++;
			} else
				longB = longBoxBatch->createDms(TRUE);

			if ( allRadioBatch->isChecked() )
				ostream << longB.toDMSString() << space;
			else
				if(longCheckBatch->isChecked() )
					ostream << longB.toDMSString() << space;

			// Read Latitude and write in ostream if corresponds

			if(latCheckBatch->isChecked() ) {
				latB = dms::fromString( fields[i], TRUE);
				i++;
			} else
				latB = latBoxBatch->createDms(TRUE);

			if ( allRadioBatch->isChecked() )
				ostream << latB.toDMSString() << space;
			else
				if(latCheckBatch->isChecked() )
					ostream << latB.toDMSString() << space;

			// Read Height and write in ostream if corresponds

			if(elevCheckBatch->isChecked() ) {
				hB = fields[i].toDouble();
				i++;
			} else
				hB = elevBoxBatch->text().toDouble() ;

			if ( allRadioBatch->isChecked() )
				ostream << hB << space;
			else
				if(elevCheckBatch->isChecked() )
					ostream << hB << space;

			geoPl->setLong( longB );
			geoPl->setLat(  latB );
			geoPl->setHeight( hB );

			ostream << geoPl->xPos()/1000.0 << space <<
				geoPl->yPos()/1000.0 << space <<
				geoPl->zPos()/1000.0 << endl;

		}

	}


	fOut.close();
}
