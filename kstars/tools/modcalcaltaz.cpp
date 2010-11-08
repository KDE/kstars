/***************************************************************************
                          modcalcaltaz.cpp  -  description
                             -------------------
    begin                : s� oct 26 2002
    copyright            : (C) 2002 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "modcalcaltaz.h"

#include <QTextStream>

#include <KGlobal>
#include <KLocale>
#include <kfiledialog.h>
#include <kmessagebox.h>

#include "skyobjects/skypoint.h"
#include "geolocation.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "kstarsdatetime.h"
#include "widgets/dmsbox.h"
#include "dialogs/finddialog.h"
#include "dialogs/locationdialog.h"

modCalcAltAz::modCalcAltAz(QWidget *parentSplit)
        : QFrame(parentSplit), horInputCoords(false)
{
    setupUi(this);

    KStarsData *data = KStarsData::Instance();
    RA->setDegType(false);

    //Initialize Date/Time and Location data
    geoPlace = data->geo();
    LocationButton->setText( geoPlace->fullName() );

    //Make sure slotDateTime() gets called, so that LST will be set
    connect(DateTime, SIGNAL(dateTimeChanged(const QDateTime&)), this, SLOT(slotDateTimeChanged(const QDateTime&)));
    DateTime->setDateTime( data->lt().dateTime() );

    connect(NowButton, SIGNAL(clicked()), this, SLOT(slotNow()));
    connect(LocationButton, SIGNAL(clicked()), this, SLOT(slotLocation()));
    connect(ObjectButton, SIGNAL(clicked()), this, SLOT(slotObject()));

    connect(RA,  SIGNAL(editingFinished()), this, SLOT(slotCompute()));
    connect(Dec, SIGNAL(editingFinished()), this, SLOT(slotCompute()));
    connect(Az,  SIGNAL(editingFinished()), this, SLOT(slotCompute()));
    connect(Alt, SIGNAL(editingFinished()), this, SLOT(slotCompute()));

    connect(runButtonBatch, SIGNAL(clicked()), this, SLOT(slotRunBatch()));
    connect(utCheckBatch, SIGNAL(clicked()), this, SLOT(slotUtChecked()));
    connect(dateCheckBatch, SIGNAL(clicked()), this, SLOT(slotDateChecked()));
    connect(azCheckBatch, SIGNAL(clicked()), this, SLOT(slotAzChecked()));
    connect(elCheckBatch, SIGNAL(clicked()), this, SLOT(slotElChecked()));
    connect(latCheckBatch, SIGNAL(clicked()), this, SLOT(slotLatChecked()));
    connect(longCheckBatch, SIGNAL(clicked()), this, SLOT(slotLongChecked()));
    connect(raCheckBatch, SIGNAL(clicked()), this, SLOT(slotRaChecked()));
    connect(decCheckBatch, SIGNAL(clicked()), this, SLOT(slotDecChecked()));

    show();
}

modCalcAltAz::~modCalcAltAz(){
}

void modCalcAltAz::slotNow()
{
    DateTime->setDateTime( KStarsDateTime::currentDateTime().dateTime() );
    slotCompute();
}

void modCalcAltAz::slotLocation()
{
    QPointer<LocationDialog> ld = new LocationDialog( this );
    if ( ld->exec() == QDialog::Accepted ) {
        GeoLocation *newGeo = ld->selectedCity();
        if ( newGeo ) {
            geoPlace = newGeo;
            LocationButton->setText( geoPlace->fullName() );
            slotCompute();
        }
    }
    delete ld;
}

void modCalcAltAz::slotObject()
{
    QPointer<FindDialog> fd = new FindDialog( KStars::Instance() );
    if ( fd->exec() == QDialog::Accepted ) {
        SkyObject *o = fd->selectedObject();
        RA->showInHours( o->ra() );
        Dec->showInDegrees( o->dec() );
        slotCompute();
    }
    delete fd;
}

void modCalcAltAz::slotDateTimeChanged(const QDateTime &dt)
{
    KStarsDateTime ut = geoPlace->LTtoUT( KStarsDateTime( dt ) );
    LST = geoPlace->GSTtoLST( ut.gst() );
}

void modCalcAltAz::slotCompute()
{
    //Determine whether we are calculating Alt/Az coordinates from RA/Dec,
    //or vice versa.  We calculate Alt/Az by default, unless the signal
    //was sent by changing the Az or Alt value.
    if ( sender()->objectName() == "Az" || sender()->objectName() == "Alt" ) {
        //Validate Az and Alt coordinates
        bool ok( false );
        dms alt;
        dms az = Az->createDms( true, &ok );
        if ( ok ) alt = Alt->createDms( true, &ok );
        if ( ok ) {
            SkyPoint sp;
            sp.setAz( az );
            sp.setAlt( alt );
            sp.HorizontalToEquatorial( &LST, geoPlace->lat() );
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
            sp.EquatorialToHorizontal( &LST, geoPlace->lat() );
            Az->showInDegrees( sp.az() );
            Alt->showInDegrees( sp.alt() );
        }
    }
}

void modCalcAltAz::slotUtChecked(){
    utBoxBatch->setEnabled( !utCheckBatch->isChecked() );
}

void modCalcAltAz::slotDateChecked(){
    dateBoxBatch->setEnabled( !dateCheckBatch->isChecked() );
}

void modCalcAltAz::slotRaChecked(){
    if ( raCheckBatch->isChecked() ) {
        raBoxBatch->setEnabled( false );
        horNoCheck();
    }
    else {
        raBoxBatch->setEnabled( true );
    }
}

void modCalcAltAz::slotDecChecked(){
    if ( decCheckBatch->isChecked() ) {
        decBoxBatch->setEnabled( false );
        horNoCheck();
    }
    else {
        decBoxBatch->setEnabled( true );
    }
}

void modCalcAltAz::slotEpochChecked(){
    epochBoxBatch->setEnabled( !epochCheckBatch->isChecked() );
}

void modCalcAltAz::slotLongChecked(){
    longBoxBatch->setEnabled( !longCheckBatch->isChecked());
}

void modCalcAltAz::slotLatChecked(){
    latBoxBatch->setEnabled( !latCheckBatch->isChecked() );
}

void modCalcAltAz::slotAzChecked(){
    if ( azCheckBatch->isChecked() ) {
        azBoxBatch->setEnabled( false );
        equNoCheck();
    }
    else {
        azBoxBatch->setEnabled( true );
    }
}

void modCalcAltAz::slotElChecked(){
    if ( elCheckBatch->isChecked() ) {
        elBoxBatch->setEnabled( false );
        equNoCheck();
    }
    else {
        elBoxBatch->setEnabled( true );
    }
}

void modCalcAltAz::horNoCheck() {
    azCheckBatch->setChecked(false);
    azBoxBatch->setEnabled(false);
    elCheckBatch->setChecked(false);
    elBoxBatch->setEnabled(false);
    horInputCoords = false;

}

void modCalcAltAz::equNoCheck() {
    raCheckBatch->setChecked(false);
    raBoxBatch->setEnabled(false);
    decCheckBatch->setChecked(false);
    decBoxBatch->setEnabled(false);
    horInputCoords = true;
}


void modCalcAltAz::slotRunBatch() {

    QString inputFileName = InputLineEditBatch->url().toLocalFile();

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
        InputLineEditBatch->setText( inputFileName );
        return;
    }
}

void modCalcAltAz::processLines( QTextStream &istream ) {

    // we open the output file

    //	QTextStream istream(&fIn);
    QString outputFileName;
    outputFileName = OutputLineEditBatch->text();
    QFile fOut( outputFileName );
    fOut.open(QIODevice::WriteOnly);
    QTextStream ostream(&fOut);

    QString line;
    QChar space = ' ';
    int i = 0;
    long double jd0, jdf;
    dms LST;
    SkyPoint sp;
    dms raB, decB, latB, longB, azB, elB;
    QString epoch0B;
    QTime utB;
    QDate dtB;

    while ( ! istream.atEnd() ) {
        line = istream.readLine();
        line.trimmed();

        //Go through the line, looking for parameters

        QStringList fields = line.split( ' ' );

        i = 0;

        // Read Ut and write in ostream if corresponds

        if(utCheckBatch->isChecked() ) {
            utB = QTime::fromString( fields[i] );
            i++;
        } else
            utB = utBoxBatch->time();

        if ( allRadioBatch->isChecked() )
            ostream << KGlobal::locale()->formatTime( utB ) << space;
        else
            if(utCheckBatch->isChecked() )
                ostream << KGlobal::locale()->formatTime( utB ) << space;

        // Read date and write in ostream if corresponds

        if(dateCheckBatch->isChecked() ) {
            dtB = QDate::fromString( fields[i] );
            i++;
        } else
            dtB = dateBoxBatch->date();
        if ( allRadioBatch->isChecked() )
            ostream << KGlobal::locale()->formatDate( dtB, KLocale::LongDate ).append(space);
        else
            if(dateCheckBatch->isChecked() )
                ostream << KGlobal::locale()->formatDate( dtB, KLocale::LongDate ).append(space);

        // Read Longitude and write in ostream if corresponds

        if (longCheckBatch->isChecked() ) {
            longB = dms::fromString( fields[i],true);
            i++;
        } else
            longB = longBoxBatch->createDms(true);

        if ( allRadioBatch->isChecked() )
            ostream << longB.toDMSString() << space;
        else
            if (longCheckBatch->isChecked() )
                ostream << longB.toDMSString() << space;

        // Read Latitude


        if (latCheckBatch->isChecked() ) {
            latB = dms::fromString( fields[i], true);
            i++;
        } else
            latB = latBoxBatch->createDms(true);
        if ( allRadioBatch->isChecked() )
            ostream << latB.toDMSString() << space;
        else
            if (latCheckBatch->isChecked() )
                ostream << latB.toDMSString() << space;

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

        // We make the first calculations
        KStarsDateTime dt;
        dt.setFromEpoch( epoch0B );
        jdf = KStarsDateTime(dtB,utB).djd();
        jd0 = dt.djd();

        LST = KStarsDateTime(dtB,utB).gst() + longB;

        // Equatorial coordinates are the input coords.
        if (!horInputCoords) {
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

            sp = SkyPoint (raB, decB);
            sp.apparentCoord(jd0, jdf);
            sp.EquatorialToHorizontal( &LST, &latB );
            ostream << sp.az().toDMSString() << space << sp.alt().toDMSString() << endl;

            // Input coords are horizontal coordinates

        } else {
            if(azCheckBatch->isChecked() ) {
                azB = dms::fromString( fields[i],false);
                i++;
            } else
                azB = azBoxBatch->createDms();

            if ( allRadioBatch->isChecked() )
                ostream << azB.toHMSString() << space;
            else
                if(raCheckBatch->isChecked() )
                    ostream << azB.toHMSString() << space;

            // Read DEC and write in ostream if corresponds

            if(elCheckBatch->isChecked() ) {
                elB = dms::fromString( fields[i], true);
                i++;
            } else
                elB = decBoxBatch->createDms();

            if ( allRadioBatch->isChecked() )
                ostream << elB.toDMSString() << space;
            else
                if(elCheckBatch->isChecked() )
                    ostream << elB.toDMSString() << space;

            sp.setAz(azB);
            sp.setAlt(elB);
            sp.HorizontalToEquatorial( &LST, &latB );
            ostream << sp.ra().toHMSString() << space << sp.dec().toDMSString() << endl;
        }

    }


    fOut.close();
}

#include "modcalcaltaz.moc"
