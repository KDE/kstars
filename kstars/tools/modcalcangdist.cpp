/***************************************************************************
                          modcalcapcoord.cpp  -  description
                             -------------------
    begin                : Sun May 30 2004
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

#include "modcalcangdist.h"
#include "modcalcangdist.moc"
#include "dms.h"
#include "dmsbox.h"
#include "skypoint.h"

#include <tqcheckbox.h>
#include <tqradiobutton.h>
#include <tqtextstream.h>
#include <klocale.h>
#include <kfiledialog.h>
#include <kmessagebox.h>

//#include <kapplication.h> ..already included in modcalcapcoord.h

modCalcAngDist::modCalcAngDist(TQWidget *parentSplit, const char *name) : modCalcAngDistDlg(parentSplit,name) {

	ra0Box->setDegType(FALSE);
	ra1Box->setDegType(FALSE);
	show();

}

modCalcAngDist::~modCalcAngDist(){
}

SkyPoint modCalcAngDist::getCoords (dmsBox* rBox, dmsBox* dBox) {
	dms raCoord, decCoord;

	raCoord = rBox->createDms(FALSE);
	decCoord = dBox->createDms();

	SkyPoint sp = SkyPoint (raCoord, decCoord);

	return sp;
}

void modCalcAngDist::showDist ( dms angDist ) {
	distBox->show( angDist );
}

void modCalcAngDist::slotClearCoords(){

	ra0Box->clearFields();
	dec0Box->clearFields();
	ra1Box->clearFields();
	dec1Box->clearFields();
	distBox->clearFields();
}

void modCalcAngDist::slotComputeDist(){

	SkyPoint sp0,sp1;
	sp0 = getCoords(ra0Box, dec0Box);
	sp1 = getCoords(ra1Box, dec1Box);

	dms aDist = sp0.angularDistanceTo(&sp1);
	showDist( aDist );
}

void modCalcAngDist::slotInputFile() {
	TQString inputFileName;
	inputFileName = KFileDialog::getOpenFileName( );
	InputLineEditBatch->setText( inputFileName );
}

void modCalcAngDist::slotOutputFile() {
	TQString outputFileName;
	outputFileName = KFileDialog::getSaveFileName( );
	OutputLineEditBatch->setText( outputFileName );
}

void modCalcAngDist::slotRunBatch() {

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

//void modCalcAngDist::processLines( const TQFile * fIn ) {
void modCalcAngDist::processLines( TQTextStream &istream ) {

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
	SkyPoint sp0, sp1;
	dms ra0B, dec0B, ra1B, dec1B, dist;

	while ( ! istream.eof() ) {
		line = istream.readLine();
		line.stripWhiteSpace();

		//Go through the line, looking for parameters

		TQStringList fields = TQStringList::split( " ", line );

		i = 0;

		// Read RA and write in ostream if corresponds

		if(ra0CheckBatch->isChecked() ) {
			ra0B = dms::fromString( fields[i],FALSE);
			i++;
		} else
			ra0B = ra0BoxBatch->createDms(FALSE);

		if ( allRadioBatch->isChecked() )
			ostream << ra0B.toHMSString() << space;
		else
			if(ra0CheckBatch->isChecked() )
				ostream << ra0B.toHMSString() << space;

		// Read DEC and write in ostream if corresponds

		if(dec0CheckBatch->isChecked() ) {
			dec0B = dms::fromString( fields[i], TRUE);
			i++;
		} else
			dec0B = dec0BoxBatch->createDms();

		if ( allRadioBatch->isChecked() )
			ostream << dec0B.toDMSString() << space;
		else
			if(dec0CheckBatch->isChecked() )
				ostream << dec0B.toDMSString() << space;
		
		// Read RA and write in ostream if corresponds

		if(ra1CheckBatch->isChecked() ) {
			ra1B = dms::fromString( fields[i],FALSE);
			i++;
		} else
			ra1B = ra1BoxBatch->createDms(FALSE);

		if ( allRadioBatch->isChecked() )
			ostream << ra1B.toHMSString() << space;
		else
			if(ra1CheckBatch->isChecked() )
				ostream << ra1B.toHMSString() << space;

		// Read DEC and write in ostream if corresponds

		if(dec1CheckBatch->isChecked() ) {
			dec1B = dms::fromString( fields[i], TRUE);
			i++;
		} else
			dec1B = dec1BoxBatch->createDms();

		if ( allRadioBatch->isChecked() )
			ostream << dec1B.toDMSString() << space;
		else
			if(dec1CheckBatch->isChecked() )
				ostream << dec1B.toDMSString() << space;

		sp0 = SkyPoint (ra0B, dec0B);
		sp1 = SkyPoint (ra1B, dec1B);
		dist = sp0.angularDistanceTo(&sp1);

		ostream << dist.toDMSString() << endl;
	}

	fOut.close();
}
