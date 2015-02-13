/***************************************************************************
                          modcalcequinox.cpp  -  description
                             -------------------
    begin                : dom may 2 2004
    copyright            : (C) 2004-2005 by Pablo de Vicente
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

#include "modcalcplanets.h"

#include <KLocalizedString>
#include <KMessageBox>

#include <QFileDialog>

#include "geolocation.h"
#include "dialogs/locationdialog.h"
#include "dms.h"
#include "kstarsdata.h"
#include "ksnumbers.h"
#include "skyobjects/kssun.h"
#include "skyobjects/ksplanet.h"
#include "skyobjects/ksmoon.h"
//#include "skyobjects/kspluto.h"
#include "widgets/dmsbox.h"

modCalcPlanets::modCalcPlanets(QWidget *parentSplit) :
    QFrame(parentSplit)
{
    setupUi(this);

    KStarsDateTime dt( KStarsDateTime::currentDateTime() );

    DateTimeBox->setDateTime( dt );
    DateBoxBatch->setDate( dt.date() );
    UTBoxBatch->setTime( dt.time() );

    geoPlace = KStarsData::Instance()->geo();
    LocationButton->setText( geoPlace->fullName() );

    RABox->setDegType(false);

    // signals and slots connections
    connect(PlanetComboBox, SIGNAL(activated(int)), this, SLOT(slotComputePosition()));
    connect(DateTimeBox, SIGNAL(dateTimeChanged( QDateTime )), this, SLOT(slotComputePosition()));
    connect(LocationButton, SIGNAL(clicked()), this, SLOT(slotLocation()));

    connect(UTCheckBatch, SIGNAL(clicked()), this, SLOT(slotUtCheckedBatch()));
    connect(DateCheckBatch, SIGNAL(clicked()), this, SLOT(slotDateCheckedBatch()));
    connect(LatCheckBatch, SIGNAL(clicked()), this, SLOT(slotLatCheckedBatch()));
    connect(LongCheckBatch, SIGNAL(clicked()), this, SLOT(slotLongCheckedBatch()));
    connect(PlanetCheckBatch, SIGNAL(clicked()), this, SLOT(slotPlanetsCheckedBatch()));

    slotComputePosition();
    show();
}

modCalcPlanets::~modCalcPlanets(){
}

void modCalcPlanets::slotLocation()
{
    QPointer<LocationDialog> ld = new LocationDialog( this );

    if ( ld->exec() == QDialog::Accepted ) {
        geoPlace = ld->selectedCity();
        LocationButton->setText( geoPlace->fullName() );
        slotComputePosition();
    }
    delete ld;
}

void modCalcPlanets::slotComputePosition (void)
{
    KStarsDateTime dt = DateTimeBox->dateTime();
    long double julianDay = dt.djd();
    KSNumbers num( julianDay );
    dms LST( geoPlace->GSTtoLST( dt.gst() ) );

    // Earth
    KSPlanet Earth( I18N_NOOP( "Earth" ));
    Earth.findPosition( &num );

    // Earth is special case!
    if( PlanetComboBox->currentIndex() == 2 ) {
        showCoordinates( Earth );
        return;
    }

    // Pointer to hold planet data. Pointer is used since it has to
    // hold objects of different type. It's safe to use new/delete
    // because exceptions are disallowed.
    KSPlanetBase* p = 0;    

    switch( PlanetComboBox->currentIndex() ) {
    case 0 : 
        p = new KSPlanet(KSPlanetBase::MERCURY); break;
    case 1:
        p = new KSPlanet(KSPlanetBase::VENUS);   break;
    case 3:
        p = new KSPlanet(KSPlanetBase::MARS);    break;
    case 4:
        p = new KSPlanet(KSPlanetBase::JUPITER); break;
    case 5:
        p = new KSPlanet(KSPlanetBase::SATURN);  break;
    case 6:
        p = new KSPlanet(KSPlanetBase::URANUS);  break;
    case 7:
        p = new KSPlanet(KSPlanetBase::NEPTUNE); break;
    /*case 8:
        p = new KSPluto(); break;*/
    case 8:
        p = new KSMoon();  break;
    case 9:
        p = new KSSun();
        p->setRsun(0.0);
        break;
    }

    // Show data.
    p->findPosition( &num, geoPlace->lat(), &LST, &Earth);        
    p->EquatorialToHorizontal( &LST, geoPlace->lat());            
    showCoordinates( *p );
    // Cleanup. 
    delete p;
}

void modCalcPlanets::showCoordinates( const KSPlanetBase &ksp)
{
    showHeliocentricEclipticCoords(ksp.helEcLong(), ksp.helEcLat(), ksp.rsun() );
    showGeocentricEclipticCoords(ksp.ecLong(), ksp.ecLat(), ksp.rearth() );
    showEquatorialCoords(ksp.ra(), ksp.dec() );
    showTopocentricCoords(ksp.az(), ksp.alt() );
}

void modCalcPlanets::showHeliocentricEclipticCoords(const dms& hLong, const dms& hLat, double dist)
{
    HelioLongBox->show( hLong );
    HelioLatBox->show( hLat );
    HelioDistBox->setText( QLocale().toString( dist,6));
}

void modCalcPlanets::showGeocentricEclipticCoords(const dms& eLong, const dms& eLat, double dist)
{
    GeoLongBox->show( eLong );
    GeoLatBox->show( eLat );
    GeoDistBox->setText( QLocale().toString( dist,6));
}

void modCalcPlanets::showEquatorialCoords(const dms& ra, const dms& dec)
{
    RABox->show( ra, false );
    DecBox->show( dec );
}

void modCalcPlanets::showTopocentricCoords(const dms& az, const dms& el)
{
    AzBox->show( az );
    AltBox->show( el );
}

void modCalcPlanets::slotPlanetsCheckedBatch()
{
    PlanetComboBoxBatch->setEnabled( ! PlanetCheckBatch->isChecked() );
}

void modCalcPlanets::slotUtCheckedBatch()
{
    UTBoxBatch->setEnabled( ! UTCheckBatch->isChecked() );
}

void modCalcPlanets::slotDateCheckedBatch()
{
    DateBoxBatch->setEnabled( ! DateCheckBatch->isChecked() );
}

void modCalcPlanets::slotLongCheckedBatch()
{
    LongBoxBatch->setEnabled( ! LongCheckBatch->isChecked() );
}

void modCalcPlanets::slotLatCheckedBatch()
{
    LatBoxBatch->setEnabled( ! LatCheckBatch->isChecked() );
}

void modCalcPlanets::slotRunBatch() {

    QString inputFileName;

    inputFileName = InputFileBoxBatch->url().toLocalFile();

    // We open the input file and read its content

    if ( QFile::exists(inputFileName) ) {
        QFile f( inputFileName );
        if ( !f.open( QIODevice::ReadOnly) ) {
            QString message = xi18n( "Could not open file %1.", f.fileName() );
            KMessageBox::sorry( 0, message, xi18n( "Could Not Open File" ) );
            inputFileName.clear();
            return;
        }

        QTextStream istream(&f);
        processLines(istream);
        f.close();
    } else  {
        QString message = xi18n( "Invalid file: %1", inputFileName );
        KMessageBox::sorry( 0, message, xi18n( "Invalid file" ) );
        inputFileName.clear();
        InputFileBoxBatch->setUrl( inputFileName );
        return;
    }
}

unsigned int modCalcPlanets::requiredBatchFields() {
    unsigned int i = 0;

    if(PlanetCheckBatch->isChecked() )
        i++;
    if(UTCheckBatch->isChecked() )
        i++;
    if(DateCheckBatch->isChecked() )
        i++;
    if (LongCheckBatch->isChecked() )
        i++;
    if (LatCheckBatch->isChecked() )
        i++;

    return i;
}

void modCalcPlanets::processLines( QTextStream &istream )
{
    // we open the output file

    QString outputFileName;
    outputFileName = OutputFileBoxBatch->url().toLocalFile();
    QFile fOut( outputFileName );
    fOut.open(QIODevice::WriteOnly);
    QTextStream ostream(&fOut);
    bool lineIsValid = true;
    QString message;

    QChar space = ' ';
    QString planetB;
    unsigned int i = 0, nline = 0;
    QTime utB;
    QDate dtB;
    dms longB, latB, hlongB, hlatB, glongB, glatB, raB, decB, azmB, altB;
    double rSunB(0.0), rEarthB(0.0);

    //Initialize planet names
    QString pn;
    QStringList pNames, pNamesi18n;
    pNames << "Mercury" << "Venus" << "Earth" << "Mars" << "Jupiter"
    << "Saturn" << "Uranus" << "Neptune" /* << "Pluto" */
    << "Sun" << "Moon";
    pNamesi18n << xi18n("Mercury") << xi18n("Venus") << xi18n("Earth")
    << xi18n("Mars") << xi18n("Jupiter") << xi18n("Saturn")
    << xi18n("Uranus") << xi18n("Neptune") /* << xi18n("Pluto") */
    << xi18n("Sun") << xi18n("Moon");

    ///Parse the input file
    int numberOfRequiredFields = requiredBatchFields();
    while ( ! istream.atEnd() ) {
        QString lineToWrite;
        QString line = istream.readLine();
        line.trimmed();

        //Go through the line, looking for parameters

        QStringList fields = line.split( ' ' );

        if (fields.count() != numberOfRequiredFields ) {
            lineIsValid = false;
            qWarning() << xi18n( "Incorrect number of fields in line %1: " , nline)
            << xi18n( "Present fields %1. " , fields.count())
            << xi18n( "Required fields %1. " , numberOfRequiredFields) << endl;
            nline++;
            continue;
        }

        i = 0;
        if(PlanetCheckBatch->isChecked() ) {
            planetB = fields[i];
            int j = pNamesi18n.indexOf( planetB );
            if (j == -1) {
                qWarning() << xi18n("Unknown planet ")
                << fields[i]
                << xi18n(" in line %1: ", nline) << endl;
                continue;
            }
            pn = pNames.at(j); //untranslated planet name
            i++;
        } else {
            planetB = PlanetComboBoxBatch->currentText( );
        }
        if ( AllRadioBatch->isChecked() || PlanetCheckBatch->isChecked() ) {
            lineToWrite = planetB;
            lineToWrite += space;
        }

        // Read Ut and write in ostream if corresponds
        if(UTCheckBatch->isChecked() ) {
            utB = QTime::fromString( fields[i] );
            if ( !utB.isValid() ) {
                qWarning() << xi18n( "Line %1 contains an invalid time" , nline) ;
                lineIsValid=false;
                nline++;
                continue;
            }
            i++;
        } else {
            utB = UTBoxBatch->time();
        }
        if ( AllRadioBatch->isChecked() || UTCheckBatch->isChecked() )
            lineToWrite += QLocale().toString( utB).append(space);

        // Read date and write in ostream if corresponds
        if(DateCheckBatch->isChecked() ) {
            dtB = QDate::fromString( fields[i], Qt::ISODate );
            if ( !dtB.isValid() ) {
                qWarning() << xi18n( "Line %1 contains an invalid date: " , nline) <<
                fields[i] << endl ;
                lineIsValid=false;
                nline++;
                continue;
            }
            i++;
        } else {
            dtB = DateBoxBatch->date();
        }
        if ( AllRadioBatch->isChecked() || DateCheckBatch->isChecked() )
            lineToWrite += QLocale().toString( dtB, QLocale::LongFormat ).append(space);


        // Read Longitude and write in ostream if corresponds

        if (LongCheckBatch->isChecked() ) {
            longB = dms::fromString( fields[i],true);
            i++;
        } else {
            longB = LongBoxBatch->createDms(true);
        }
        if ( AllRadioBatch->isChecked() || LongCheckBatch->isChecked() )
            lineToWrite += longB.toDMSString() + space;

        // Read Latitude
        if (LatCheckBatch->isChecked() ) {
            latB = dms::fromString( fields[i], true);
            i++;
        } else {
            latB = LatBoxBatch->createDms(true);
        }
        if ( AllRadioBatch->isChecked() || LatCheckBatch->isChecked() ) 
            lineToWrite += latB.toDMSString() + space;


        KStarsDateTime edt( dtB, utB );
        dms LST = edt.gst() + longB;

        KSNumbers num( edt.djd() );
        KSPlanet Earth( I18N_NOOP( "Earth" ));
        Earth.findPosition( &num );

        // FIXME: allocate new object for every iteration is probably not wisest idea.
        KSPlanetBase *kspb = 0 ;
        /*if ( pn == "Pluto" ) {
            kspb = new KSPluto();
        } else*/
        if ( pn == "Sun" ) {
            kspb = new KSSun();
        } else if ( pn == "Moon" ) {
            kspb = new KSMoon();
        } else {
            kspb = new KSPlanet(i18n( pn.toLocal8Bit() ), QString(), Qt::white, 1.0 );
        }
        kspb->findPosition( &num, &latB, &LST, &Earth );
        kspb->EquatorialToHorizontal( &LST, &latB );

        // Heliocentric Ecl. coords.
        hlongB  = kspb->helEcLong();
        hlatB   = kspb->helEcLat();
        rSunB   = kspb->rsun();
        // Geocentric Ecl. coords.
        glongB  = kspb->ecLong();
        glatB   = kspb->ecLat();
        rEarthB = kspb->rearth();
        // Equatorial coords.
        decB    = kspb->dec();
        raB     = kspb->ra();
        // Topocentric Coords.
        azmB    = kspb->az();
        altB    = kspb->alt();

        ostream << lineToWrite;

        if ( HelioEclCheckBatch->isChecked() )
            ostream << hlongB.toDMSString() << space << hlatB.toDMSString() << space << rSunB << space ;
        if ( GeoEclCheckBatch->isChecked() )
            ostream << glongB.toDMSString() << space << glatB.toDMSString() << space << rEarthB << space ;
        if ( EquatorialCheckBatch->isChecked() )
            ostream << raB.toHMSString() << space << decB.toDMSString() << space ;
        if ( HorizontalCheckBatch->isChecked() )
            ostream << azmB.toDMSString() << space << altB.toDMSString() << space ;
        ostream << endl;

        // Delete object
        delete kspb;
   
        nline++;
    }

    if (!lineIsValid) {
        QString message = xi18n("Errors found while parsing some lines in the input file");
        KMessageBox::sorry( 0, message, xi18n( "Errors in lines" ) );
    }

    fOut.close();
}

