/***************************************************************************
                          modcalcsidtime.cpp  -  description
                             -------------------
    begin                : Wed Jan 23 2002
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

//#include <q3datetimeedit.h>  //need for QTimeEdit
//#include <qradiobutton.h>
//#include <qcheckbox.h>
//#include <qstring.h>
//#include <qtextstream.h>
#include <kfiledialog.h>
#include <kmessagebox.h>

#include "modcalcsidtime.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "kstarsdatetime.h"
#include "simclock.h"
#include "widgets/dmsbox.h"
#include "libkdeedu/extdate/extdatetimeedit.h"

modCalcSidTime::modCalcSidTime(QWidget *parentSplit) : QFrame(parentSplit) {
	setupUi(parentSplit);
	showCurrentTimeAndLong();
	show();		
}

modCalcSidTime::~modCalcSidTime(void) {

}

void modCalcSidTime::showCurrentTimeAndLong (void)
{
	KStars *ks = (KStars*) topLevelWidget()->parent();

	showUT( ks->data()->ut().time() );
	DateBox->setDate( ks->data()->ut().date() );

	LongitudeBox->show( ks->geo()->lng() );
}

QTime modCalcSidTime::computeUTtoST (QTime ut, ExtDate dt, dms longitude)
{
	KStarsDateTime utdt = KStarsDateTime( dt, ut);
	dms st = longitude.Degrees() + utdt.gst().Degrees();
	return QTime( st.hour(), st.minute(), st.second() );
}

QTime modCalcSidTime::computeSTtoUT (QTime st, ExtDate dt, dms longitude)
{
	KStarsDateTime dtt = KStarsDateTime( dt, QTime());
	dms lst(st.hour(), st.minute(), st.second());
	dms gst( lst.Degrees() - longitude.Degrees() );
	return dtt.GSTtoUT( gst );
}

void modCalcSidTime::showUT( QTime dt )
{
	InputTimeBox->setTime( dt );
}

void modCalcSidTime::showST( QTime st )
{
	OutputTimeBox->setTime( st );
}

QTime modCalcSidTime::getUT( void ) 
{
	return InputTimeBox->time();
}

QTime modCalcSidTime::getST( void ) 
{
	return OutputTimeBox->time();
}

ExtDate modCalcSidTime::getDate( void ) 
{
	return DateBox->date();
}

dms modCalcSidTime::getLongitude( void )
{
	return LongitudeBox->createDms();
}

void modCalcSidTime::slotClearFields(){
	DateBox->setDate(ExtDate::currentDate());
	QTime time(0,0,0);
	InputTimeBox->setTime(time);
	OutputTimeBox->setTime(time);
}

void modCalcSidTime::slotComputeTime(){
	QTime ut, st;

	ExtDate dt = getDate();
	dms longitude = getLongitude();

	if(UTRadio->isChecked()) {
		ut = getUT();
		st = computeUTtoST( ut, dt, longitude );
		showST( st );
	} else {
		st = getST();
		ut = computeSTtoUT( st, dt, longitude );
		showUT( ut );
	}

}

void modCalcSidTime::slotUtChecked(){

	if ( UTCheckBatch->isChecked() )
		InputTimeBoxBatch->setEnabled( false );
	else 
		InputTimeBoxBatch->setEnabled( true );
}

void modCalcSidTime::slotDateChecked(){

	if ( DateCheckBatch->isChecked() )
		DateBoxBatch->setEnabled( false );
	else 
		DateBoxBatch->setEnabled( true );
}

void modCalcSidTime::slotStChecked(){

	if ( STCheckBatch->isChecked() )
		OutputTimeBoxBatch->setEnabled( false );
	else 
		OutputTimeBoxBatch->setEnabled( true );
}

void modCalcSidTime::slotLongChecked(){

	if ( LongCheckBatch->isChecked() )
		LongitudeBoxBatch->setEnabled( false );
	else
		LongitudeBoxBatch->setEnabled( true );
}

void modCalcSidTime::sidNoCheck() {

	OutputTimeBoxBatch->setEnabled(false);
	stInputTime = FALSE;

}

void modCalcSidTime::utNoCheck() {

	InputTimeBoxBatch->setEnabled(false);
	stInputTime = TRUE;
}

void modCalcSidTime::slotInputFile() {
	QString inputFileName;
	inputFileName = KFileDialog::getOpenFileName( );
	InputFileBoxBatch->setURL( inputFileName );
}

void modCalcSidTime::slotOutputFile() {
	QString outputFileName;
	outputFileName = KFileDialog::getSaveFileName( );
	OutputFileBoxBatch->setURL( outputFileName );
}


void modCalcSidTime::slotRunBatch() {

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

void modCalcSidTime::processLines( QTextStream &istream ) {

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
	dms longB, LST;
	double epoch0B(0.0);
	QTime utB, stB;
	ExtDate dtB;

	while ( ! istream.atEnd() ) {
		line = istream.readLine();
		line.trimmed();

		//Go through the line, looking for parameters

		QStringList fields = QStringList::split( " ", line );

		i = 0;
		
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

		
		// We make the first calculations
		KStarsDateTime dt;
		dt.setFromEpoch( epoch0B );
		jdf = KStarsDateTime(dtB,utB).djd();
		jd0 = dt.djd();

		LST = dms( longB.Degrees() + KStarsDateTime(dtB,utB).gst().Degrees() );
		
		// Universal Time is the input time.
		if (!stInputTime) {

		// Read Ut and write in ostream if corresponds
		
			if(UTCheckBatch->isChecked() ) {
				utB = QTime::fromString( fields[i] );
				i++;
			} else
				utB = InputTimeBoxBatch->time();
		
			if ( AllRadioBatch->isChecked() )
				ostream << utB.toString() << space;
			else
				if(UTCheckBatch->isChecked() )
					ostream << utB.toString() << space;
			

			stB = computeUTtoST( utB, dtB, longB );
			ostream << stB.toString()  << endl;

		// Input coords are horizontal coordinates
		
		} else {

			if(STCheckBatch->isChecked() ) {
				stB = QTime::fromString( fields[i] );
				i++;
			} else
				stB = OutputTimeBoxBatch->time();
		
			if ( AllRadioBatch->isChecked() )
				ostream << stB.toString() << space;
			else
				if(STCheckBatch->isChecked() )
					ostream << stB.toString() << space;
			

			utB = computeSTtoUT( stB, dtB, longB );
			ostream << utB.toString()  << endl;

		}

	}


	fOut.close();
}

#include "modcalcsidtime.moc"
