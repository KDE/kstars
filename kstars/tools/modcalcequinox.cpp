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
	showCurrentYear();
	show();
}

modCalcEquinox::~modCalcEquinox(){
}

int modCalcEquinox::getYear (QString eName)
{
	bool ok = FALSE;
	int equinoxYear = eName.toInt(&ok);
	if ( ok ) 
		return equinoxYear;
	else {
		kDebug() << i18n( "Could not parse epoch string; assuming J2000" ) << endl;
		return 2000;
	}
}

void modCalcEquinox::showCurrentYear (void)
{
	KStarsDateTime dt( KStarsDateTime::currentDateTime() );
	yearEdit->setText( QString( "%1").arg( dt.date().year() ) );
}

void modCalcEquinox::slotComputeEquinoxesAndSolstices (void)
{
	long double julianDay = 0., jdf = 0.;
	float deltaJd;
	KStarsData *kd = (KStarsData*) topLevelWidget()->parent();
	KSSun *Sun = new KSSun(kd);
	int year0 = getYear( yearEdit->text() );
	
	if (equinoxSolsticesComboBox->currentIndex() == 0 ) {
		julianDay = Sun->springEquinox(year0);
		jdf = Sun->summerSolstice(year0);
	}
	else if(equinoxSolsticesComboBox->currentIndex() == 1) {
		julianDay = Sun->summerSolstice(year0);
		jdf = Sun->autumnEquinox(year0);
	}
	else if (equinoxSolsticesComboBox->currentIndex() == 2 ) {
		julianDay = Sun->autumnEquinox(year0);
		jdf = Sun->winterSolstice(year0);
	}
	else if(equinoxSolsticesComboBox->currentIndex() == 3) {
		julianDay = Sun->winterSolstice(year0);
		jdf = Sun->springEquinox(year0+1);
	}
	
	deltaJd = (float) (jdf - julianDay);
	showStartDateTime(julianDay);
	showSeasonDuration(deltaJd);
	
}

void modCalcEquinox::slotClear(void){
	yearEdit->setText(QString());
	seasonDuration->setText(QString());
}

void modCalcEquinox::showStartDateTime(long double jd)
{
	KStarsDateTime dt( jd );
	startDateTimeEquinox->setDateTime( dt );
}

void modCalcEquinox::showSeasonDuration(float deltaJd)
{
	seasonDuration->setText( QString( "%1").arg( deltaJd ) );
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
	InputFileBoxBatch->setURL( inputFileName );
}

void modCalcEquinox::slotOutputFile() {
	QString outputFileName;
	outputFileName = KFileDialog::getSaveFileName( );
	OutputFileBoxBatch->setURL( outputFileName );
}

void modCalcEquinox::slotRunBatch() {

	QString inputFileName;

	inputFileName = InputFileBoxBatch->url();

	// We open the input file and read its content

	if ( QFile::exists(inputFileName) ) {
		QFile f( inputFileName );
		if ( !f.open( QIODevice::ReadOnly) ) {
			QString message = i18n( "Could not open file %1.", f.name() );
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

void modCalcEquinox::processLines( QTextStream &istream ) {

	// we open the output file

//	QTextStream istream(&fIn);
	QString outputFileName;
	outputFileName = OutputFileBoxBatch->url();
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

		QStringList fields = QStringList::split( " ", line );

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
