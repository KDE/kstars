/***************************************************************************
                          modcalcequinox.cpp  -  description
                             -------------------
    begin                : dom apr 18 2002
    copyright            : (C) 2004 by Pablo de Vicente
    email                : p.devicentea@wanadoo.es
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

#include "modcalcequinox.h"

#include "dms.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "kstarsdatetime.h"
#include "kssun.h"
#include "widgets/dmsbox.h"
#include "libkdeedu/extdate/extdatetimeedit.h"

modCalcEquinox::modCalcEquinox(QWidget *parentSplit)
: QFrame(parentSplit) {
	setupUi(this);

	connect( Year, SIGNAL( valueChanged(int) ), this, SLOT( slotCompute() ) );
	showCurrentYear();
	show();
}

modCalcEquinox::~modCalcEquinox(){
}

void modCalcEquinox::showCurrentYear()
{
	KStarsDateTime dt( KStarsDateTime::currentDateTime() );
	Year->setValue( dt.date().year() );
}

void modCalcEquinox::slotCompute()
{
	KStarsDateTime tSpring, tSummer, tAutumn, tWinter, tSpring2;

	KStars *ks = (KStars*) topLevelWidget()->parent();
	KSSun *Sun = new KSSun(ks->data());
	int year0 = Year->value();

	tSpring.setDJD( Sun->springEquinox(year0)  );
	tSummer.setDJD( Sun->summerSolstice(year0) );
	tAutumn.setDJD( Sun->autumnEquinox(year0)  );
	tWinter.setDJD( Sun->winterSolstice(year0) );
	tSpring2.setDJD( Sun->springEquinox(year0+1)); //needed for Winer duration

	SpringEquinoxTime->setText( i18nc( "an event occurred at [time] on [date]", 
																		 "%1 on %2", tSpring.time().toString(), 
																		 tSpring.date().toString() ) );
	SummerSolsticeTime->setText( i18nc( "an event occurred at [time] on [date]", 
																			"%1 on %2", tSummer.time().toString(), 
																			tSummer.date().toString() ) );
	AutumnEquinoxTime->setText( i18nc( "an event occurred at [time] on [date]", 
																		 "%1 on %2", tAutumn.time().toString(), 
																		 tAutumn.date().toString() ) );
	WinterSolsticeTime->setText( i18nc( "an event occurred at [time] on [date]", 
																			"%1 on %2", tWinter.time().toString(), 
																			tWinter.date().toString() ) );

	SpringDuration->setText( i18n( "%1 days", 
																 QString::number( double(tSummer.djd()-tSpring.djd()) ) ) );
	SummerDuration->setText( i18n( "%1 days", 
																 QString::number( double(tAutumn.djd()-tSummer.djd()) ) ) );
	AutumnDuration->setText( i18n( "%1 days", 
																 QString::number( double(tWinter.djd()-tAutumn.djd()) ) ) );
	WinterDuration->setText( i18n( "%1 days", 
																 QString::number( double(tSpring2.djd()-tWinter.djd()) ) ) );

}

void modCalcEquinox::slotYearCheckedBatch(){
	if ( yearCheckBatch->isChecked() )
		yearCheckBatch->setEnabled( false );
	else {
		yearCheckBatch->setEnabled( true );
	}
}

void modCalcEquinox::slotInputFile() {
	QString inputFileName;
	inputFileName = KFileDialog::getOpenFileName( );
	InputFileBoxBatch->setUrl( inputFileName );
}

void modCalcEquinox::slotOutputFile() {
	QString outputFileName;
	outputFileName = KFileDialog::getSaveFileName( );
	OutputFileBoxBatch->setUrl( outputFileName );
}

void modCalcEquinox::slotRunBatch() {

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

void modCalcEquinox::processLines( QTextStream &istream ) {

	// we open the output file

//	QTextStream istream(&fIn);
	QString outputFileName;
	outputFileName = OutputFileBoxBatch->url().path();
	QFile fOut( outputFileName );
	fOut.open(QIODevice::WriteOnly);
	QTextStream ostream(&fOut);

	QString line;
	QString space = " ";
	int yearB;
	int i = 0;
	long double jdsp = 0., jdsu = 0., jdau = 0., jdwin = 0., jdsp1 = 0.;
	KStarsData *kd = (KStarsData*) topLevelWidget()->parent();
	KSSun *Sun = new KSSun(kd);

	while ( ! istream.atEnd() ) {
		line = istream.readLine();
		line.trimmed();

		//Go through the line, looking for parameters

		QStringList fields = line.split( " " );

		i = 0;

		// Read year and write in ostream if corresponds

		if(yearCheckBatch->isChecked() ) {
			yearB = fields[i].toInt();
			i++;
		} else
			yearB = yearEditBatch->text().toInt();

		if ( allRadioBatch->isChecked() )
			ostream << yearB << space;
		else
			if(yearCheckBatch->isChecked() )
				ostream << yearB << space;

		jdsp = Sun->springEquinox(yearB);
		jdsu = Sun->summerSolstice(yearB);
		jdau = Sun->autumnEquinox(yearB);
		jdwin = Sun->winterSolstice(yearB);
		jdsp1 = Sun->springEquinox(yearB+1);

		KStarsDateTime dts( jdsp );
		KStarsDateTime dtu( jdsu );
		KStarsDateTime dta( jdau );
		KStarsDateTime dtw( jdwin );

		ostream << dts.toString(Qt::ISODate) << space << (float)(jdsu - jdsp) << space
						<< dtu.toString(Qt::ISODate) << space << (float)(jdau - jdsu) << space
						<< dta.toString(Qt::ISODate) << space << (float)(jdwin - jdau) << space
						<< dtw.toString(Qt::ISODate) << space << (float)(jdsp1 - jdwin) << endl;
	}


	fOut.close();
}

#include "modcalcequinox.moc"
