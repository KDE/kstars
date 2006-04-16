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

//#include <qcheckbox.h>
//#include <qradiobutton.h>
//#include <qtextstream.h>
#include <klocale.h>
#include <klineedit.h>
#include <kapplication.h>
#include <kfiledialog.h>
#include <kmessagebox.h>

#include "modcalcprec.h"
#include "modcalcprec.moc"
#include "skypoint.h"
#include "dms.h"
#include "kstarsdatetime.h"
#include "widgets/dmsbox.h"

modCalcPrec::modCalcPrec(QWidget *parentSplit) : QFrame(parentSplit) {
	setupUi(this);
	InputRABox->setDegType(FALSE);
	TargetRABox->setDegType(FALSE);
	show();
}

modCalcPrec::~modCalcPrec(){

}

SkyPoint modCalcPrec::getEquCoords (void) {
	dms raCoord, decCoord;

	raCoord = InputRABox->createDms(FALSE);
	decCoord = InputDecBox->createDms();

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

void modCalcPrec::slotClearCoords (void) {

	InputRABox->clearFields();
	InputDecBox->clearFields();
	TargetRABox->clearFields();
	TargetDecBox->clearFields();
	InputEpochBox->setText(QString());
	TargetEpochBox->setText(QString());

}

void modCalcPrec::slotComputeCoords (void) {

	SkyPoint sp;

	sp = getEquCoords();

	QString epoch0 = InputEpochBox->text();
	QString epochf = TargetEpochBox->text();

	KStarsDateTime dt;
	dt.setFromEpoch( epoch0 );
	long double jd0 = dt.djd();
	dt.setFromEpoch( epochf );
	long double jdf = dt.djd();

	sp.precessFromAnyEpoch(jd0, jdf);

	showEquCoords( sp );

}

void modCalcPrec::showEquCoords ( const SkyPoint &sp ) {
	TargetRABox->show( sp.ra(),FALSE );
	TargetDecBox->show( sp.dec() );
}

void modCalcPrec::slotRaCheckedBatch(){

	if ( RACheckBatch->isChecked() )
		RABoxBatch->setEnabled( false );
	else {
		RABoxBatch->setEnabled( true );
	}
}

void modCalcPrec::slotDecCheckedBatch(){

	if ( DecCheckBatch->isChecked() )
		DecBoxBatch->setEnabled( false );
	else {
		DecBoxBatch->setEnabled( true );
	}
}

void modCalcPrec::slotEpochCheckedBatch(){

	if ( EpochCheckBatch->isChecked() )
		InputEpochBoxBatch->setEnabled( false );
	else {
		InputEpochBoxBatch->setEnabled( true );
	}
}

void modCalcPrec::slotTargetEpochCheckedBatch(){

	if ( TargetEpochCheckBatch->isChecked() )
		TargetEpochBoxBatch->setEnabled( false );
	else {
		TargetEpochBoxBatch->setEnabled( true );
	}
}

void modCalcPrec::slotInputFile() {
	QString inputFileName;
	inputFileName = KFileDialog::getOpenFileName( );
	InputFileBoxBatch->setURL( inputFileName );
}

void modCalcPrec::slotOutputFile() {
	QString outputFileName;
	outputFileName = KFileDialog::getSaveFileName( );
	OutputFileBoxBatch->setURL( outputFileName );


}

void modCalcPrec::slotRunBatch() {

	QString inputFileName;

	inputFileName = InputFileBoxBatch->url();

	// We open the input file and read its content

	if ( QFile::exists(inputFileName) ) {
		QFile f( inputFileName );
		if ( !f.open( QIODevice::ReadOnly) ) {
			QString message = i18n( "Could not open file %1.", f.fileName() );
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
		QString message = i18n( "Invalid file: %1", inputFileName );
		KMessageBox::sorry( 0, message, i18n( "Invalid file" ) );
		inputFileName = QString();
		InputFileBoxBatch->setURL( inputFileName );
		return;
	}
}

void modCalcPrec::processLines( QTextStream &istream ) {

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
	long double jd0, jdf;
	SkyPoint sp;
	dms raB, decB;
	QString epoch0B, epochfB;
	KStarsDateTime dt0, dtf;

	while ( ! istream.atEnd() ) {
		line = istream.readLine();
		line.trimmed();

		//Go through the line, looking for parameters

		QStringList fields = line.split( " " );

		i = 0;

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
			epoch0B = InputEpochBoxBatch->text();

		if ( AllRadioBatch->isChecked() )
			ostream << epoch0B;
		else
			if(EpochCheckBatch->isChecked() )
				ostream << epoch0B;

		// Read Target epoch and write in ostream if corresponds

		if(TargetEpochCheckBatch->isChecked() ) {
			epochfB = fields[i];
			i++;
		} else
			epochfB = TargetEpochBoxBatch->text();

		if ( AllRadioBatch->isChecked() )
			ostream << epochfB << space;
		else
			if(TargetEpochCheckBatch->isChecked() )
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
