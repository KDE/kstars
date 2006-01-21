/***************************************************************************
                          modcalcapcoord.cpp  -  description
                             -------------------
    begin                : Wed Apr 10 2002
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

#include "modcalcapcoord.h"
#include "modcalcapcoord.moc"
#include "modcalcprec.h"
#include "dms.h"
#include "dmsbox.h"
#include "skypoint.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "kstarsdatetime.h"
#include "libkdeedu/extdate/extdatetimeedit.h"

#include <qcheckbox.h>
#include <qradiobutton.h>
#include <qdatetimeedit.h>  //needed for QTimeEdit
#include <klineedit.h>
#include <qtextstream.h>
#include <klocale.h>
#include <kfiledialog.h>
#include <kmessagebox.h>

//#include <kapplication.h> ..already included in modcalcapcoord.h

modCalcApCoord::modCalcApCoord(QWidget *parentSplit, const char *name) : modCalcApCoordDlg(parentSplit,name) {

	showCurrentTime();
	ra0Box->setDegType(FALSE);
	rafBox->setDegType(FALSE);
	show();

}

modCalcApCoord::~modCalcApCoord(){
}

SkyPoint modCalcApCoord::getEquCoords (void) {
	dms raCoord, decCoord;

	raCoord = ra0Box->createDms(FALSE);
	decCoord = dec0Box->createDms();

	SkyPoint sp = SkyPoint (raCoord, decCoord);

	return sp;
}

void modCalcApCoord::showCurrentTime (void)
{
	KStars *ks = (KStars*) parent()->parent()->parent();

	KStarsDateTime dt = ks->data()->geo()->LTtoUT( KStarsDateTime::currentDateTime() );
	datBox->setDate( dt.date() );
	timBox->setTime( dt.time() );
}

KStarsDateTime modCalcApCoord::getDateTime (void)
{
	return KStarsDateTime( datBox->date() , timBox->time() );
}

double modCalcApCoord::getEpoch (QString eName) {
	bool ok = false;
	double epoch = eName.toDouble(&ok);

	if ( ok )
		return epoch;
	else {
		kdDebug() << i18n( "Could not parse epoch string; assuming J2000" ) << endl;
		return 2000.0;
	}
}

void modCalcApCoord::showEquCoords ( SkyPoint sp ) {
	rafBox->show( sp.ra() , FALSE);
	decfBox->show( sp.dec() );
}

void modCalcApCoord::slotClearCoords(){

	ra0Box->clearFields();
	dec0Box->clearFields();
	rafBox->clearFields();
	decfBox->clearFields();
	epoch0Name->setText("");
	datBox->setDate(ExtDate::currentDate());
	timBox->setTime(QTime(0,0,0));
}

void modCalcApCoord::slotComputeCoords(){
	long double jd = getDateTime().djd();
	KStarsDateTime dt;
	dt.setFromEpoch( getEpoch( epoch0Name->text() ) );
	long double jd0 = dt.djd();

	SkyPoint sp;
	sp = getEquCoords();

	sp.apparentCoord(jd0, jd);
	showEquCoords( sp );
}

void modCalcApCoord::slotUtCheckedBatch(){
	if ( utCheckBatch->isChecked() )
		utBoxBatch->setEnabled( false );
	else {
		utBoxBatch->setEnabled( true );
	}
}

void modCalcApCoord::slotDateCheckedBatch(){

	if ( dateCheckBatch->isChecked() )
		dateBoxBatch->setEnabled( false );
	else {
		dateBoxBatch->setEnabled( true );
	}
}

void modCalcApCoord::slotRaCheckedBatch(){

	if ( raCheckBatch->isChecked() )
		raBoxBatch->setEnabled( false );
	else {
		raBoxBatch->setEnabled( true );
	}
}

void modCalcApCoord::slotDecCheckedBatch(){

	if ( decCheckBatch->isChecked() )
		decBoxBatch->setEnabled( false );
	else {
		decBoxBatch->setEnabled( true );
	}
}

void modCalcApCoord::slotEpochCheckedBatch(){

	if ( epochCheckBatch->isChecked() )
		epochBoxBatch->setEnabled( false );
	else {
		epochBoxBatch->setEnabled( true );
	}
}

void modCalcApCoord::slotInputFile() {
	QString inputFileName;
	inputFileName = KFileDialog::getOpenFileName( );
	InputLineEditBatch->setText( inputFileName );
}

void modCalcApCoord::slotOutputFile() {
	QString outputFileName;
	outputFileName = KFileDialog::getSaveFileName( );
	OutputLineEditBatch->setText( outputFileName );
}

void modCalcApCoord::slotRunBatch() {

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

//void modCalcApCoord::processLines( const QFile * fIn ) {
void modCalcApCoord::processLines( QTextStream &istream ) {

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
	long double jd, jd0;
	SkyPoint sp;
	QTime utB;
	ExtDate dtB;
	dms raB, decB;
	double epoch0B;

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
				ostream << decB.toHMSString() << space;

		// Read Epoch and write in ostream if corresponds

		if(epochCheckBatch->isChecked() ) {
			epoch0B = fields[i].toDouble();
			i++;
		} else
			epoch0B = getEpoch( epochBoxBatch->text() );

		if ( allRadioBatch->isChecked() )
			ostream << epoch0B;
		else
			if(decCheckBatch->isChecked() )
				ostream << epoch0B;

		KStarsDateTime dt;
		dt.setFromEpoch( epoch0B );
		jd = KStarsDateTime(dtB,utB).djd();
		jd0 = dt.djd();
		sp = SkyPoint (raB, decB);
		sp.apparentCoord(jd0, jd);

		ostream << sp.ra()->toHMSString() << sp.dec()->toDMSString() << endl;
	}

	fOut.close();
}
