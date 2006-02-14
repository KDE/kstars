/***************************************************************************
                          modcalcgal.cpp  -  description
                             -------------------
    begin                : Thu Jan 17 2002
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

#include <QTextStream>

#include <kfiledialog.h>
#include <kmessagebox.h>

#include "modcalcgalcoord.h"

#include "dms.h"
#include "skypoint.h"
#include "kstarsdatetime.h"
#include "widgets/dmsbox.h"

modCalcGalCoord::modCalcGalCoord(QWidget *parentSplit) 
: QFrame(parentSplit) {

	setupUi( parentSplit );
	equRadio->setChecked(TRUE);
	raBox->setDegType(FALSE);
	show();
}

modCalcGalCoord::~modCalcGalCoord() {
}

void modCalcGalCoord::getGalCoords (void) {

	galLong = lgBox->createDms();
	galLat = bgBox->createDms();
	epoch = epochName->text();
}

void modCalcGalCoord::getEquCoords (void) {

	raCoord = raBox->createDms(FALSE);
	decCoord = decBox->createDms();
	epoch = epochName->text();
}

void modCalcGalCoord::slotClearCoords (void) {

	raBox->clearFields();
	decBox->clearFields();
	lgBox->clearFields();
	bgBox->clearFields();

}

void modCalcGalCoord::slotComputeCoords (void) {

	if(galRadio->isChecked()) {
		getGalCoords();
//		checkEpoch();
		GalToEqu();
		showEquCoords();
	} else {
		getEquCoords();
//		checkEpoch();
		EquToGal();
		showGalCoords();
	}

}

void modCalcGalCoord::showEquCoords(void) {
	raBox->show( raCoord , FALSE);
	decBox->show( decCoord );
}

void modCalcGalCoord::showGalCoords(void) {
	lgBox->show( galLong );
	bgBox->show( galLat );
}

void modCalcGalCoord::GalToEqu(void) {

	SkyPoint sp = SkyPoint();

	sp.GalacticToEquatorial1950(&galLong, &galLat);
	sp.set(*sp.ra(), *sp.dec() );

	KStarsDateTime dt;
	dt.setFromEpoch( epoch );
	long double jdf = dt.djd();
	sp.precessFromAnyEpoch(B1950,jdf);

	raCoord.set( *sp.ra() );
	decCoord.set( *sp.dec() );
}

void modCalcGalCoord::EquToGal(void) {

	SkyPoint sp = SkyPoint (raCoord, decCoord);

	KStarsDateTime dt;
	dt.setFromEpoch( epoch );
	long double jd0 = dt.djd();
	sp.precessFromAnyEpoch(jd0,B1950);

	sp.Equatorial1950ToGalactic(galLong, galLat);

}

void modCalcGalCoord::galCheck() {

	galLatCheckBatch->setChecked(false);
	galLatBoxBatch->setEnabled(false);
	galLongCheckBatch->setChecked(false);
	galLongBoxBatch->setEnabled(false);
	galInputCoords = FALSE;

}

void modCalcGalCoord::equCheck() {

	raCheckBatch->setChecked(false);
	raBoxBatch->setEnabled(false);
	decCheckBatch->setChecked(false);
	decBoxBatch->setEnabled(false);
	epochCheckBatch->setChecked(false);
	galInputCoords = TRUE;

}

void modCalcGalCoord::slotRaCheckedBatch(){

	if ( raCheckBatch->isChecked() ) {
		raBoxBatch->setEnabled( false );
		galCheck();
	} else {
		raBoxBatch->setEnabled( true );
	}
}

void modCalcGalCoord::slotDecCheckedBatch(){

	if ( decCheckBatch->isChecked() ) {
		decBoxBatch->setEnabled( false );
		galCheck();
	} else {
		decBoxBatch->setEnabled( true );
	}
}

void modCalcGalCoord::slotEpochCheckedBatch(){

	epochCheckBatch->setChecked(false);

	if ( epochCheckBatch->isChecked() ) {
		epochBoxBatch->setEnabled( false );
		galCheck();
	} else {
		epochBoxBatch->setEnabled( true );
	}
}

void modCalcGalCoord::slotGalLatCheckedBatch(){

	if ( galLatCheckBatch->isChecked() ) {
		galLatBoxBatch->setEnabled( false );
		equCheck();
	} else {
		galLatBoxBatch->setEnabled( true );
	}
}

void modCalcGalCoord::slotGalLongCheckedBatch(){

	if ( galLongCheckBatch->isChecked() ) {
		galLongBoxBatch->setEnabled( false );
		equCheck();
	} else {
		galLongBoxBatch->setEnabled( true );
	}
}

void modCalcGalCoord::slotInputFile() {
	QString inputFileName;
	inputFileName = KFileDialog::getOpenFileName( );
	InputFileBoxBatch->setURL( inputFileName );
}

void modCalcGalCoord::slotOutputFile() {
	QString outputFileName;
	outputFileName = KFileDialog::getSaveFileName( );
	OutputFileBoxBatch->setURL( outputFileName );
}

void modCalcGalCoord::slotRunBatch() {

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

void modCalcGalCoord::processLines( QTextStream &istream ) {

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
	SkyPoint sp;
	dms raB, decB, galLatB, galLongB;
	QString epoch0B;
	KStarsDateTime dt;

	while ( ! istream.atEnd() ) {
		line = istream.readLine();
		line.trimmed();

		//Go through the line, looking for parameters

		QStringList fields = QStringList::split( " ", line );

		i = 0;

		// Input coords are galactic coordinates:

		if (galInputCoords) {

			// Read Galactic Longitude and write in ostream if corresponds

			if(galLongCheckBatch->isChecked() ) {
				galLongB = dms::fromString( fields[i], TRUE);
				i++;
			} else
				galLongB = galLongBoxBatch->createDms(TRUE);

			if ( allRadioBatch->isChecked() )
				ostream << galLongB.toDMSString() << space;
			else
				if(galLongCheckBatch->isChecked() )
					ostream << galLongB.toDMSString() << space;

			// Read Galactic Latitude and write in ostream if corresponds

			if(galLatCheckBatch->isChecked() ) {
				galLatB = dms::fromString( fields[i], TRUE);
				i++;
			} else
				galLatB = galLatBoxBatch->createDms(TRUE);

			if ( allRadioBatch->isChecked() )
				ostream << galLatB.toDMSString() << space;
			else
				if(galLatCheckBatch->isChecked() )
					ostream << galLatB.toDMSString() << space;

			sp = SkyPoint ();
			sp.GalacticToEquatorial1950(&galLongB, &galLatB);
			ostream << sp.ra()->toHMSString() << space << sp.dec()->toDMSString() << epoch0B << endl;
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
				epoch0B = fields[i];
				i++;
			} else
				epoch0B = epochBoxBatch->text();

			if ( allRadioBatch->isChecked() )
				ostream << epoch0B << space;
			else
				if(epochCheckBatch->isChecked() )
					ostream << epoch0B << space;

			sp = SkyPoint (raB, decB);
			dt.setFromEpoch( epoch0B );
			long double jdf = dt.djd();
			sp.precessFromAnyEpoch(B1950,jdf);
			sp.Equatorial1950ToGalactic(galLongB, galLatB);
			ostream << galLongB.toDMSString() << space << galLatB.toDMSString() << endl;

		}

	}


	fOut.close();
}

#include "modcalcgalcoord.moc"
