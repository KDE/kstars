/***************************************************************************
                          modcalceclipticcoords.cpp  -  description
                             -------------------
    begin                : Fri May 14 2004
    copyright            : (C) 2004 by Pablo de Vicente
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

#include "dms.h"
#include "dmsbox.h"
#include "modcalceclipticcoords.h"
#include "modcalceclipticcoords.moc"
#include "skypoint.h"
#include "ksutils.h"
#include "ksnumbers.h"
#include "kstarsdatetime.h"
#include <tqradiobutton.h>
#include <tqstring.h>
#include <tqcheckbox.h>
#include <tqradiobutton.h>
#include <tqtextstream.h>
#include <klocale.h>
#include <klineedit.h>
#include <kapplication.h>
#include <kfiledialog.h>
#include <kmessagebox.h>


modCalcEclCoords::modCalcEclCoords(TQWidget *parentSplit, const char *name) : modCalcEclCoordsDlg(parentSplit,name) {

	equRadio->setChecked(TRUE);
	raBox->setDegType(FALSE);
	this->show();
}

modCalcEclCoords::~modCalcEclCoords() {
}

void modCalcEclCoords::getEclCoords (void) {

	eclipLong = ecLongBox->createDms();
	eclipLat = ecLatBox->createDms();
	epoch = getEpoch( epochName->text() );
}

void modCalcEclCoords::getEquCoords (void) {

	raCoord = raBox->createDms(FALSE);
	decCoord = decBox->createDms();
	epoch = getEpoch( epochName->text() );
}

double modCalcEclCoords::getEpoch (TQString eName) {

	double epoch = eName.toDouble();

	return epoch;
}

void modCalcEclCoords::slotClearCoords (void) {

	raBox->clearFields();
	decBox->clearFields();
	ecLongBox->clearFields();
	ecLatBox->clearFields();

}

void modCalcEclCoords::slotComputeCoords (void) {

	if(eclRadio->isChecked()) {
		getEclCoords();
		EclToEqu();
		showEquCoords();
	} else {
		getEquCoords();
		EquToEcl();
		showEclCoords();
	}

}

void modCalcEclCoords::showEquCoords(void) {
	raBox->show( raCoord , FALSE);
	decBox->show( decCoord );
}

void modCalcEclCoords::showEclCoords(void) {
	ecLongBox->show( eclipLong );
	ecLatBox->show( eclipLat );
}

void modCalcEclCoords::EclToEqu(void) {

	SkyPoint sp = SkyPoint();

	KStarsDateTime dt;
	dt.setFromEpoch( epoch );
	KSNumbers *num = new KSNumbers( dt.djd() );

//	sp.setEclLong(eclipLong);
//	sp.setEclLat(eclipLat);
	sp.setFromEcliptic(num->obliquity(), &eclipLong, &eclipLat);
	
	raCoord.set( *sp.ra() );
	decCoord.set( *sp.dec() );

	delete num;
}

void modCalcEclCoords::EquToEcl(void) {

	SkyPoint sp = SkyPoint (raCoord, decCoord);
	KStarsDateTime dt;
	dt.setFromEpoch( epoch );
	KSNumbers *num = new KSNumbers( dt.djd() );

	sp.findEcliptic(num->obliquity(), eclipLong, eclipLat);

	delete num;
}

void modCalcEclCoords::eclCheck() {

	eclLatCheckBatch->setChecked(false);
	eclLatBoxBatch->setEnabled(false);
	eclLongCheckBatch->setChecked(false);
	eclLongBoxBatch->setEnabled(false);
	eclInputCoords = FALSE;

}

void modCalcEclCoords::equCheck() {

	raCheckBatch->setChecked(false);
	raBoxBatch->setEnabled(false);
	decCheckBatch->setChecked(false);
	decBoxBatch->setEnabled(false);
	//epochCheckBatch->setChecked(false);
	eclInputCoords = TRUE;

}

void modCalcEclCoords::slotRaCheckedBatch(){

	if ( raCheckBatch->isChecked() ) {
		raBoxBatch->setEnabled( false );
		eclCheck();
	} else {
		raBoxBatch->setEnabled( true );
	}
}

void modCalcEclCoords::slotDecCheckedBatch(){

	if ( decCheckBatch->isChecked() ) {
		decBoxBatch->setEnabled( false );
		eclCheck();
	} else {
		decBoxBatch->setEnabled( true );
	}
}


void modCalcEclCoords::slotEpochCheckedBatch(){
	if ( epochCheckBatch->isChecked() ) {
		epochBoxBatch->setEnabled( false );
	} else {
		epochBoxBatch->setEnabled( true );
	}
}


void modCalcEclCoords::slotEclLatCheckedBatch(){

	if ( eclLatCheckBatch->isChecked() ) {
		eclLatBoxBatch->setEnabled( false );
		equCheck();
	} else {
		eclLatBoxBatch->setEnabled( true );
	}
}

void modCalcEclCoords::slotEclLongCheckedBatch(){

	if ( eclLongCheckBatch->isChecked() ) {
		eclLongBoxBatch->setEnabled( false );
		equCheck();
	} else {
		eclLongBoxBatch->setEnabled( true );
	}
}

void modCalcEclCoords::slotInputFile() {
	TQString inputFileName;
	inputFileName = KFileDialog::getOpenFileName( );
	InputLineEditBatch->setText( inputFileName );
}

void modCalcEclCoords::slotOutputFile() {
	TQString outputFileName;
	outputFileName = KFileDialog::getSaveFileName( );
	OutputLineEditBatch->setText( outputFileName );
}

void modCalcEclCoords::slotRunBatch() {

	TQString inputFileName;

	inputFileName = InputLineEditBatch->text();

	// We open the input file and read its content

	if ( TQFile::exists(inputFileName) ) {
		TQFile f( inputFileName );
		if ( !f.open( IO_ReadOnly) ) {
			TQString message = i18n( "Could not open file %1.").arg( f.name() );
			KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
			inputFileName = "";
			return;
		}

//		processLines(&f);
		TQTextStream istream(&f);
		processLines(istream);
//		readFile( istream );
		f.close();
	} else  {
		TQString message = i18n( "Invalid file: %1" ).arg( inputFileName );
		KMessageBox::sorry( 0, message, i18n( "Invalid file" ) );
		inputFileName = "";
		InputLineEditBatch->setText( inputFileName );
		return;
	}
}

void modCalcEclCoords::processLines( TQTextStream &istream ) {

	// we open the output file

//	TQTextStream istream(&fIn);
	TQString outputFileName;
	outputFileName = OutputLineEditBatch->text();
	TQFile fOut( outputFileName );
	fOut.open(IO_WriteOnly);
	TQTextStream ostream(&fOut);

	TQString line;
	TQString space = " ";
	int i = 0;
	SkyPoint sp;
	dms raB, decB, eclLatB, eclLongB;
	double epoch0B;

	while ( ! istream.eof() ) {
		line = istream.readLine();
		line.stripWhiteSpace();

		//Go through the line, looking for parameters

		TQStringList fields = TQStringList::split( " ", line );

		i = 0;

		// Input coords are ecliptic coordinates:

		if (eclInputCoords) {

			// Read Ecliptic Longitude and write in ostream if corresponds

			if(eclLongCheckBatch->isChecked() ) {
				eclLongB = dms::fromString( fields[i], TRUE);
				i++;
			} else
				eclLongB = eclLongBoxBatch->createDms(TRUE);

			if ( allRadioBatch->isChecked() )
				ostream << eclLongB.toDMSString() << space;
			else
				if(eclLongCheckBatch->isChecked() )
					ostream << eclLongB.toDMSString() << space;

			// Read Ecliptic Latitude and write in ostream if corresponds

			if(eclLatCheckBatch->isChecked() ) {
				eclLatB = dms::fromString( fields[i], TRUE);
				i++;
			} else
			if ( allRadioBatch->isChecked() )
				ostream << eclLatB.toDMSString() << space;
			else
				if(eclLatCheckBatch->isChecked() )
					ostream << eclLatB.toDMSString() << space;

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

			sp = SkyPoint ();

			KStarsDateTime dt;
			dt.setFromEpoch( epoch0B );
			KSNumbers *num = new KSNumbers( dt.djd() );
			sp.setFromEcliptic(num->obliquity(), &eclLongB, &eclLatB);
			ostream << sp.ra()->toHMSString() << space << sp.dec()->toDMSString() << endl;
		// Input coords. are equatorial coordinates:

		} else {

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

			sp = SkyPoint (raB, decB);
			KStarsDateTime dt;
			dt.setFromEpoch( epoch0B );
			KSNumbers *num = new KSNumbers( dt.djd() );
			sp.findEcliptic(num->obliquity(), eclLongB, eclLatB);
			ostream << eclLongB.toDMSString() << space << eclLatB.toDMSString() << endl;
			delete num;

		}

	}


	fOut.close();
}
