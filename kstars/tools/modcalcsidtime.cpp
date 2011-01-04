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

#include <tqdatetimeedit.h>  //need for QTimeEdit
#include <tqradiobutton.h>
#include <tqcheckbox.h>
#include <tqstring.h>
#include <tqtextstream.h>
#include <kfiledialog.h>
#include <kmessagebox.h>

#include "dmsbox.h"
#include "modcalcsidtime.h"
#include "modcalcsidtime.moc"
#include "kstars.h"
#include "kstarsdata.h"
#include "kstarsdatetime.h"
#include "simclock.h"
#include "libkdeedu/extdate/extdatetimeedit.h"

modCalcSidTime::modCalcSidTime(TQWidget *parentSplit, const char *name) : modCalcSidTimeDlg (parentSplit,name) {

	showCurrentTimeAndLong();
	show();		
}

modCalcSidTime::~modCalcSidTime(void) {

}

void modCalcSidTime::showCurrentTimeAndLong (void)
{
	KStars *ks = (KStars*) parent()->parent()->parent();
	 // modCalcSidTimeDlg -> QSplitter->AstroCalc->KStars

	showUT( ks->data()->ut().time() );
	datBox->setDate( ks->data()->ut().date() );

	longBox->show( ks->geo()->lng() );
}

TQTime modCalcSidTime::computeUTtoST (TQTime ut, ExtDate dt, dms longitude)
{
	KStarsDateTime utdt = KStarsDateTime( dt, ut);
	dms st = longitude.Degrees() + utdt.gst().Degrees();
	return TQTime( st.hour(), st.minute(), st.second() );
}

TQTime modCalcSidTime::computeSTtoUT (TQTime st, ExtDate dt, dms longitude)
{
	KStarsDateTime dtt = KStarsDateTime( dt, TQTime());
	dms lst(st.hour(), st.minute(), st.second());
	dms gst( lst.Degrees() - longitude.Degrees() );
	return dtt.GSTtoUT( gst );
}

void modCalcSidTime::showUT( TQTime dt )
{
	UtBox->setTime( dt );
}

void modCalcSidTime::showST( TQTime st )
{
	StBox->setTime( st );
}

TQTime modCalcSidTime::getUT( void ) 
{
	return UtBox->time();
}

TQTime modCalcSidTime::getST( void ) 
{
	return StBox->time();
}

ExtDate modCalcSidTime::getDate( void ) 
{
	return datBox->date();
}

dms modCalcSidTime::getLongitude( void )
{
	return longBox->createDms();
}

void modCalcSidTime::slotClearFields(){
	datBox->setDate(ExtDate::tqcurrentDate());
	TQTime time(0,0,0);
	UtBox->setTime(time);
	StBox->setTime(time);
}

void modCalcSidTime::slotComputeTime(){
	TQTime ut, st;

	ExtDate dt = getDate();
	dms longitude = getLongitude();

	if(UtRadio->isChecked()) {
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

	if ( utCheckBatch->isChecked() )
		utBoxBatch->setEnabled( false );
	else 
		utBoxBatch->setEnabled( true );
}

void modCalcSidTime::slotDateChecked(){

	if ( dateCheckBatch->isChecked() )
		dateBoxBatch->setEnabled( false );
	else 
		dateBoxBatch->setEnabled( true );
}

void modCalcSidTime::slotStChecked(){

	if ( stCheckBatch->isChecked() )
		stBoxBatch->setEnabled( false );
	else 
		stBoxBatch->setEnabled( true );
}

void modCalcSidTime::slotLongChecked(){

	if ( longCheckBatch->isChecked() )
		longBoxBatch->setEnabled( false );
	else
		longBoxBatch->setEnabled( true );
}

void modCalcSidTime::sidNoCheck() {

	stBoxBatch->setEnabled(false);
	stInputTime = FALSE;

}

void modCalcSidTime::utNoCheck() {

	utBoxBatch->setEnabled(false);
	stInputTime = TRUE;
}

void modCalcSidTime::slotInputFile() {
	TQString inputFileName;
	inputFileName = KFileDialog::getOpenFileName( );
	InputLineEditBatch->setText( inputFileName );
}

void modCalcSidTime::slotOutputFile() {
	TQString outputFileName;
	outputFileName = KFileDialog::getSaveFileName( );
	OutputLineEditBatch->setText( outputFileName );
}


void modCalcSidTime::slotRunBatch() {

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

void modCalcSidTime::processLines( TQTextStream &istream ) {

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
	long double jd0, jdf;
	dms longB, LST;
	double epoch0B(0.0);
	TQTime utB, stB;
	ExtDate dtB;

	while ( ! istream.eof() ) {
		line = istream.readLine();
		line.stripWhiteSpace();

		//Go through the line, looking for parameters

		TQStringList fields = TQStringList::split( " ", line );

		i = 0;
		
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

		
		// We make the first calculations
		KStarsDateTime dt;
		dt.setFromEpoch( epoch0B );
		jdf = KStarsDateTime(dtB,utB).djd();
		jd0 = dt.djd();

		LST = dms( longB.Degrees() + KStarsDateTime(dtB,utB).gst().Degrees() );
		
		// Universal Time is the input time.
		if (!stInputTime) {

		// Read Ut and write in ostream if corresponds
		
			if(utCheckBatch->isChecked() ) {
				utB = TQTime::fromString( fields[i] );
				i++;
			} else
				utB = utBoxBatch->time();
		
			if ( allRadioBatch->isChecked() )
				ostream << utB.toString() << space;
			else
				if(utCheckBatch->isChecked() )
					ostream << utB.toString() << space;
			

			stB = computeUTtoST( utB, dtB, longB );
			ostream << stB.toString()  << endl;

		// Input coords are horizontal coordinates
		
		} else {

			if(stCheckBatch->isChecked() ) {
				stB = TQTime::fromString( fields[i] );
				i++;
			} else
				stB = stBoxBatch->time();
		
			if ( allRadioBatch->isChecked() )
				ostream << stB.toString() << space;
			else
				if(stCheckBatch->isChecked() )
					ostream << stB.toString() << space;
			

			utB = computeSTtoUT( stB, dtB, longB );
			ostream << utB.toString()  << endl;

		}

	}


	fOut.close();
}
