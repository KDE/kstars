/***************************************************************************
                          modcalcprec.cpp  -  description
                             -------------------
    begin                : Sun Jan 27 2002
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

#include "modcalcprec.h"
#include "modcalcprec.moc"
#include "dmsbox.h"
#include "skypoint.h"
#include "dms.h"
#include "kstarsdatetime.h"

#include <qcheckbox.h>
#include <qradiobutton.h>
#include <qtextstream.h>
#include <klocale.h>
#include <klineedit.h>
#include <kapplication.h>
#include <kfiledialog.h>
#include <kmessagebox.h>

modCalcPrec::modCalcPrec(QWidget *parentSplit, const char *name) : modCalcPrecDlg(parentSplit,name) {

	ra0Box->setDegType(FALSE);
	rafBox->setDegType(FALSE);
	show();

}

modCalcPrec::~modCalcPrec(){

}

SkyPoint modCalcPrec::getEquCoords (void) {
	dms raCoord, decCoord;

	raCoord = ra0Box->createDms(FALSE);
	decCoord = dec0Box->createDms();

	SkyPoint sp = SkyPoint (raCoord, decCoord);

	return sp;
}

QString modCalcPrec:: showCurrentEpoch () {

	double epoch = setCurrentEpoch();
	QString eName = QString("%1").arg(epoch,7,'f',2);

	return eName;
}

double modCalcPrec::setCurrentEpoch () {
	return KStarsDateTime::currentDateTime().epoch();
}

double modCalcPrec::getEpoch (QString eName) {
	bool ok = false;
	double epoch = eName.toDouble(&ok);

	if ( ok )
		return epoch;
	else {
		kdDebug() << i18n( "Could not parse epoch string; assuming J2000" ) << endl;
		return 2000.0;
	}
}

void modCalcPrec::slotClearCoords (void) {

	ra0Box->clearFields();
	dec0Box->clearFields();
	rafBox->clearFields();
	decfBox->clearFields();
	epoch0Name->setText("");
	epochfName->setText("");

}

void modCalcPrec::slotComputeCoords (void) {

	SkyPoint sp;

	sp = getEquCoords();

	double epoch0 = getEpoch( epoch0Name->text() );
	double epochf = getEpoch( epochfName->text() );

	KStarsDateTime dt;
	dt.setFromEpoch( epoch0 );
	long double jd0 = dt.djd();
	dt.setFromEpoch( epochf );
	long double jdf = dt.djd();

	sp.precessFromAnyEpoch(jd0, jdf);

	showEquCoords( sp );

}

void modCalcPrec::showEquCoords ( SkyPoint sp ) {
	rafBox->show( sp.ra(),FALSE );
	decfBox->show( sp.dec() );
}

void modCalcPrec::slotRaCheckedBatch(){

	if ( raCheckBatch->isChecked() )
		raBoxBatch->setEnabled( false );
	else {
		raBoxBatch->setEnabled( true );
	}
}

void modCalcPrec::slotDecCheckedBatch(){

	if ( decCheckBatch->isChecked() )
		decBoxBatch->setEnabled( false );
	else {
		decBoxBatch->setEnabled( true );
	}
}

void modCalcPrec::slotEpochCheckedBatch(){

	if ( epochCheckBatch->isChecked() )
		epochBoxBatch->setEnabled( false );
	else {
		epochBoxBatch->setEnabled( true );
	}
}

void modCalcPrec::slotTargetEpochCheckedBatch(){

	if ( targetEpochCheckBatch->isChecked() )
		targetEpochBoxBatch->setEnabled( false );
	else {
		targetEpochBoxBatch->setEnabled( true );
	}
}

void modCalcPrec::slotInputFile() {
	QString inputFileName;
	inputFileName = KFileDialog::getOpenFileName( );
	InputLineEditBatch->setText( inputFileName );
}

void modCalcPrec::slotOutputFile() {
	QString outputFileName;
	outputFileName = KFileDialog::getSaveFileName( );
	OutputLineEditBatch->setText( outputFileName );


}

void modCalcPrec::slotRunBatch() {

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

void modCalcPrec::processLines( QTextStream &istream ) {

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
	SkyPoint sp;
	dms raB, decB;
	double epoch0B, epochfB;
	KStarsDateTime dt0, dtf;

	while ( ! istream.eof() ) {
		line = istream.readLine();
		line.stripWhiteSpace();

		//Go through the line, looking for parameters

		QStringList fields = QStringList::split( " ", line );

		i = 0;

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
			ostream << epoch0B;
		else
			if(epochCheckBatch->isChecked() )
				ostream << epoch0B;

		// Read Target epoch and write in ostream if corresponds

		if(targetEpochCheckBatch->isChecked() ) {
			epochfB = fields[i].toDouble();
			i++;
		} else
			epochfB = getEpoch( targetEpochBoxBatch->text() );

		if ( allRadioBatch->isChecked() )
			ostream << epochfB << space;
		else
			if(targetEpochCheckBatch->isChecked() )
				ostream << epochfB << space;

		dt0.setFromEpoch( epoch0B );
		dtf.setFromEpoch( epoch0B );
		jd0 = dt0.djd();
		jdf = dtf.djd();
		sp = SkyPoint (raB, decB);
		sp.precessFromAnyEpoch(jd0, jdf);

		ostream << sp.ra()->toHMSString() << space << sp.dec()->toDMSString() << endl;
	}


	fOut.close();
}
