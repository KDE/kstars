/***************************************************************************
                          modcalceclipticcoords.cpp  -  description
                             -------------------
    begin                : Fri May 14 2004
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

#include "modcalceclipticcoords.h"

#include <QTextStream>

#include <klocale.h>
#include <kfiledialog.h>
#include <kmessagebox.h>
#include <kurlrequester.h>

#include "dms.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skyobjects/skypoint.h"
#include "ksnumbers.h"
#include "dialogs/finddialog.h"
#include "widgets/dmsbox.h"

modCalcEclCoords::modCalcEclCoords(QWidget *parentSplit)
        : QFrame(parentSplit) {

    setupUi(this);
    RA->setDegType(false);

    //Initialize Date/Time and Location data
    DateTime->setDateTime( KStarsData::Instance()->lt().dateTime() );
    kdt = DateTime->dateTime();

    connect(NowButton, SIGNAL(clicked()), this, SLOT(slotNow()));
    connect(ObjectButton, SIGNAL(clicked()), this, SLOT(slotObject()));
    connect(DateTime, SIGNAL(dateTimeChanged(const QDateTime&)), this, SLOT(slotDateTimeChanged(const QDateTime&)));

    connect(RA,     SIGNAL(editingFinished()), this, SLOT(slotCompute()));
    connect(Dec,    SIGNAL(editingFinished()), this, SLOT(slotCompute()));
    connect(EcLong, SIGNAL(editingFinished()), this, SLOT(slotCompute()));
    connect(EcLat,  SIGNAL(editingFinished()), this, SLOT(slotCompute()));

    connect(ecLatCheckBatch, SIGNAL(clicked()), this, SLOT(slotEclLatCheckedBatch()));
    connect(ecLongCheckBatch, SIGNAL(clicked()), this, SLOT(slotEclLongCheckedBatch()));
    connect(epochCheckBatch, SIGNAL(clicked()), this, SLOT(slotEpochCheckedBatch()));
    connect(raCheckBatch, SIGNAL(clicked()), this, SLOT(slotRaCheckedBatch()));
    connect(decCheckBatch, SIGNAL(clicked()), this, SLOT(slotDecCheckedBatch()));
    connect(runButtonBatch, SIGNAL(clicked()), this, SLOT(slotRunBatch()));

    this->show();
}

modCalcEclCoords::~modCalcEclCoords() {
}

void modCalcEclCoords::slotNow() {
    DateTime->setDateTime( KStarsDateTime::currentDateTime().dateTime() );
    slotCompute();
}

void modCalcEclCoords::slotObject() {
    QPointer<FindDialog> fd = new FindDialog( KStars::Instance() );
    if ( fd->exec() == QDialog::Accepted ) {
        SkyObject *o = fd->selectedObject();
        RA->showInHours( o->ra() );
        Dec->showInDegrees( o->dec() );
        slotCompute();
    }
    delete fd;
}

void modCalcEclCoords::slotDateTimeChanged(const QDateTime &edt) {
    kdt = ((KStarsDateTime)edt);
}

void modCalcEclCoords::slotCompute(void) {
    KSNumbers num( kdt.djd() );

    //Determine whether we are calculating ecliptic coordinates from equatorial,
    //or vice versa.  We calculate ecliptic by default, unless the signal
    //was sent by changing the EcLong or EcLat value.
    if ( sender()->objectName() == "EcLong" || sender()->objectName() == "EcLat" ) {
        //Validate ecliptic coordinates
        bool ok( false );
        dms elat;
        dms elong = EcLong->createDms( true, &ok );
        if ( ok ) elat = EcLat->createDms( true, &ok );
        if ( ok ) {
            SkyPoint sp;
            sp.setFromEcliptic( num.obliquity(), elong, elat );
            RA->showInHours( sp.ra() );
            Dec->showInDegrees( sp.dec() );
        }

    } else {
        //Validate RA and Dec coordinates
        bool ok( false );
        dms ra;
        dms dec = Dec->createDms( true, &ok );
        if ( ok ) ra = RA->createDms( false, &ok );
        if ( ok ) {
            SkyPoint sp( ra, dec );
            dms elong, elat;
            sp.findEcliptic( num.obliquity(), elong, elat );
            EcLong->showInDegrees( elong );
            EcLat->showInDegrees( elat );
        }
    }
}

void modCalcEclCoords::eclCheck() {

    ecLatCheckBatch->setChecked(false);
    ecLatBoxBatch->setEnabled(false);
    ecLongCheckBatch->setChecked(false);
    ecLongBoxBatch->setEnabled(false);
    //	eclInputCoords = false;

}

void modCalcEclCoords::equCheck() {

    raCheckBatch->setChecked(false);
    raBoxBatch->setEnabled(false);
    decCheckBatch->setChecked(false);
    decBoxBatch->setEnabled(false);
    //epochCheckBatch->setChecked(false);
    //	eclInputCoords = true;

}

void modCalcEclCoords::slotRaCheckedBatch(){

    if ( raCheckBatch->isChecked() ) {
        raBoxBatch->setEnabled( false );
        eclCheck();
    } else {
        raBoxBatch->setEnabled( true );
    }
}

void modCalcEclCoords::slotDecCheckedBatch(){

    if ( decCheckBatch->isChecked() ) {
        decBoxBatch->setEnabled( false );
        eclCheck();
    } else {
        decBoxBatch->setEnabled( true );
    }
}


void modCalcEclCoords::slotEpochCheckedBatch(){
    if ( epochCheckBatch->isChecked() ) {
        epochBoxBatch->setEnabled( false );
    } else {
        epochBoxBatch->setEnabled( true );
    }
}


void modCalcEclCoords::slotEclLatCheckedBatch(){

    if ( ecLatCheckBatch->isChecked() ) {
        ecLatBoxBatch->setEnabled( false );
        equCheck();
    } else {
        ecLatBoxBatch->setEnabled( true );
    }
}

void modCalcEclCoords::slotEclLongCheckedBatch(){

    if ( ecLongCheckBatch->isChecked() ) {
        ecLongBoxBatch->setEnabled( false );
        equCheck();
    } else {
        ecLongBoxBatch->setEnabled( true );
    }
}

void modCalcEclCoords::slotRunBatch() {

    QString inputFileName = InputFileBoxBatch->url().toLocalFile();

    // We open the input file and read its content

    if ( QFile::exists(inputFileName) ) {
        QFile f( inputFileName );
        if ( !f.open( QIODevice::ReadOnly) ) {
            QString message = i18n( "Could not open file %1.", f.fileName() );
            KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
            inputFileName.clear();
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
        inputFileName.clear();
        InputFileBoxBatch->setUrl( inputFileName );
        return;
    }
}

void modCalcEclCoords::processLines( QTextStream &istream ) {

    // we open the output file

    //	QTextStream istream(&fIn);
    QString outputFileName = OutputFileBoxBatch->url().toLocalFile();
    QFile fOut( outputFileName );
    fOut.open(QIODevice::WriteOnly); // TODO error checking
    QTextStream ostream(&fOut);

    QString line;
    QChar space = ' ';
    int i = 0;
    SkyPoint sp;
    dms raB, decB, eclLatB, eclLongB;
    QString epoch0B;

    while ( ! istream.atEnd() ) {
        line = istream.readLine();
        line.trimmed();

        //Go through the line, looking for parameters

        QStringList fields = line.split( ' ' );

        i = 0;

        // Input coords are ecliptic coordinates:

        // 		if (eclInputCoords) {
        //
        // 			// Read Ecliptic Longitude and write in ostream if corresponds
        //
        // 			if(ecLongCheckBatch->isChecked() ) {
        // 				eclLongB = dms::fromString( fields[i], true);
        // 				i++;
        // 			} else
        // 				eclLongB = ecLongBoxBatch->createDms(true);
        //
        // 			if ( allRadioBatch->isChecked() )
        // 				ostream << eclLongB.toDMSString() << space;
        // 			else
        // 				if(ecLongCheckBatch->isChecked() )
        // 					ostream << eclLongB.toDMSString() << space;
        //
        // 			// Read Ecliptic Latitude and write in ostream if corresponds
        //
        // 			if(ecLatCheckBatch->isChecked() ) {
        // 				eclLatB = dms::fromString( fields[i], true);
        // 				i++;
        // 			} else
        // 			if ( allRadioBatch->isChecked() )
        // 				ostream << eclLatB.toDMSString() << space;
        // 			else
        // 				if(ecLatCheckBatch->isChecked() )
        // 					ostream << eclLatB.toDMSString() << space;
        //
        // 			// Read Epoch and write in ostream if corresponds
        //
        // 			if(epochCheckBatch->isChecked() ) {
        // 				epoch0B = fields[i];
        // 				i++;
        // 			} else
        // 				epoch0B = epochBoxBatch->text();
        //
        // 			if ( allRadioBatch->isChecked() )
        // 				ostream << epoch0B << space;
        // 			else
        // 				if(epochCheckBatch->isChecked() )
        // 					ostream << epoch0B << space;
        //
        // 			sp = SkyPoint ();
        //
        // 			KStarsDateTime dt;
        // 			dt.setFromEpoch( epoch0B );
        // 			KSNumbers *num = new KSNumbers( dt.djd() );
        // 			sp.setFromEcliptic(num->obliquity(), &eclLongB, &eclLatB);
        // 			ostream << sp.ra().toHMSString() << space << sp.dec().toDMSString() << endl;
        // 		// Input coords. are equatorial coordinates:
        //
        // 		} else {

        // Read RA and write in ostream if corresponds

        if(raCheckBatch->isChecked() ) {
            raB = dms::fromString( fields[i],false);
            i++;
        } else
            raB = raBoxBatch->createDms(false);

        if ( allRadioBatch->isChecked() )
            ostream << raB.toHMSString() << space;
        else
            if(raCheckBatch->isChecked() )
                ostream << raB.toHMSString() << space;

        // Read DEC and write in ostream if corresponds

        if(decCheckBatch->isChecked() ) {
            decB = dms::fromString( fields[i], true);
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
        KStarsDateTime dt;
        dt.setFromEpoch( epoch0B );
        KSNumbers *num = new KSNumbers( dt.djd() );
        sp.findEcliptic(num->obliquity(), eclLongB, eclLatB);
        ostream << eclLongB.toDMSString() << space << eclLatB.toDMSString() << endl;
        delete num;

        //		}

    }


    fOut.close();
}

#include "modcalceclipticcoords.moc"
