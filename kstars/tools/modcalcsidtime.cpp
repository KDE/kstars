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

#include <kfiledialog.h>
#include <kmessagebox.h>

#include "modcalcsidtime.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "kstarsdatetime.h"
#include "simclock.h"
#include "locationdialog.h"
#include "widgets/dmsbox.h"
#include "libkdeedu/extdate/extdatetimeedit.h"

modCalcSidTime::modCalcSidTime(QWidget *parentSplit) : CalcFrame(parentSplit) {
	setupUi(this);

	//Preset date and location
	showCurrentTimeAndLocation();

	// signals and slots connections
	connect(LocationButton, SIGNAL(clicked()), this, SLOT(slotChangeLocation()));
	connect(Date, SIGNAL(valueChanged(ExtDate)), this, SLOT(slotChangeDate()));
	connect(LT, SIGNAL(timeChanged(const QTime&)), this, SLOT(slotConvertST(const QTime&)));
	connect(ST, SIGNAL(timeChanged(const QTime&)), this, SLOT(slotConvertLT(const QTime&)));
	connect(this, SIGNAL(frameShown()), this, SLOT(slotShown()));

	connect(LongCheckBatch, SIGNAL(clicked()), this, SLOT(slotLongChecked()));
	connect(DateCheckBatch, SIGNAL(clicked()), this, SLOT(slotDateChecked()));
	connect(UTCheckBatch, SIGNAL(clicked()), this, SLOT(slotUtChecked()));
	connect(STCheckBatch, SIGNAL(clicked()), this, SLOT(slotStChecked()));
	connect(RunButtonBatch, SIGNAL(clicked()), this, SLOT(slotRunBatch()));

	bSyncTime = false;
	show();
}

modCalcSidTime::~modCalcSidTime(void) {

}

void modCalcSidTime::showCurrentTimeAndLocation()
{
	KStars *ks = (KStars*) topLevelWidget()->parent();

	LT->setTime( ks->data()->lt().time() );
	Date->setDate( ks->data()->lt().date() );

	geo = ks->geo();
	LocationButton->setText( geo->fullName() );

	slotConvertST( LT->time() );
}

void modCalcSidTime::slotChangeLocation() {
	KStars *ks = (KStars*) topLevelWidget()->parent();
	LocationDialog ld(ks);

	if ( ld.exec() == QDialog::Accepted ) {
		GeoLocation *newGeo = ld.selectedCity();
		if ( newGeo ) {
			geo = newGeo;
			LocationButton->setText( geo->fullName() );

			//Update the displayed ST
			slotConvertST( LT->time() );
		}
	}
}

void modCalcSidTime::slotChangeDate() {
	slotConvertST( LT->time() );
}

void modCalcSidTime::slotShown() {
	slotConvertST( LT->time() );
}

void modCalcSidTime::slotConvertST(const QTime &lt){
	if ( bSyncTime == false ) {
		bSyncTime = true;
		ST->setTime( computeLTtoST( lt ) );
	} else {
		bSyncTime = false;
	}
}

void modCalcSidTime::slotConvertLT(const QTime &st){
	if ( bSyncTime == false ) {
		bSyncTime = true;
		LT->setTime( computeSTtoLT( st ) );
	} else {
		bSyncTime = false;
	}
}


QTime modCalcSidTime::computeLTtoST( QTime lt )
{
	KStarsDateTime utdt = geo->LTtoUT( KStarsDateTime( Date->date(), lt ) );
	dms st = geo->GSTtoLST( utdt.gst() );
	return QTime( st.hour(), st.minute(), st.second() );
}

QTime modCalcSidTime::computeSTtoLT( QTime st )
{
	KStarsDateTime dt0 = KStarsDateTime( Date->date(), QTime(0,0,0));
	dms lst;
	lst.setH( st.hour(), st.minute(), st.second() );
	dms gst = geo->LSTtoGST( lst );
	return geo->UTtoLT(KStarsDateTime(Date->date(), dt0.GSTtoUT(gst))).time();
}

//** Batch mode **//
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
	stInputTime = false;

}

void modCalcSidTime::utNoCheck() {

	InputTimeBoxBatch->setEnabled(false);
	stInputTime = true;
}

void modCalcSidTime::slotRunBatch() {

	QString inputFileName;

	inputFileName = InputFileBoxBatch->url().path();

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
		InputFileBoxBatch->setUrl( inputFileName );
		return;
	}
}

void modCalcSidTime::processLines( QTextStream &istream ) {

	// we open the output file

//	QTextStream istream(&fIn);
	QString outputFileName;
	outputFileName = OutputFileBoxBatch->url().path();
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

		QStringList fields = line.split( " " );

		i = 0;

		// Read Longitude and write in ostream if corresponds

		if (LongCheckBatch->isChecked() ) {
			longB = dms::fromString( fields[i],true);
			i++;
		} else
			longB = LongitudeBoxBatch->createDms(true);

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


//FIXME: UT became LT
//			stB = computeUTtoST( utB, dtB, longB );
//			ostream << stB.toString()  << endl;

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


//FIXME: UT became LT
//			utB = computeSTtoUT( stB, dtB, longB );
//			ostream << utB.toString()  << endl;

		}

	}


	fOut.close();
}

#include "modcalcsidtime.moc"
