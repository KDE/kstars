/***************************************************************************
                          modcalcdaylength.cpp  -  description
                             -------------------
    begin                : wed jun 12 2002
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

#include "modcalcdaylength.h"

#include <KGlobal>
#include <KLocale>
#include <kmessagebox.h>
#include <QLineEdit>

#include "skyobjects/skyobject.h"
#include "geolocation.h"
#include "kstarsdata.h"
#include "skyobjects/kssun.h"
#include "skyobjects/ksmoon.h"
#include "ksnumbers.h"
#include "kstarsdatetime.h"
#include "dialogs/locationdialog.h"


modCalcDayLength::modCalcDayLength(QWidget *parentSplit) :
    QFrame(parentSplit)
{
    setupUi(this);

    showCurrentDate();
    initGeo();
    slotComputeAlmanac();

    connect( Date, SIGNAL(dateChanged(const QDate&)), this, SLOT(slotComputeAlmanac() ) );
    connect( Location, SIGNAL( clicked() ), this, SLOT( slotLocation() ) );

    connect( LocationBatch, SIGNAL( clicked() ), this, SLOT( slotLocationBatch() ) );
    connect( InputFileBatch, SIGNAL(urlSelected(const KUrl&)), this, SLOT(slotCheckFiles()) );
    connect( OutputFileBatch, SIGNAL(urlSelected(const KUrl&)), this, SLOT(slotCheckFiles()) );
    connect( RunButtonBatch, SIGNAL(clicked()), this, SLOT(slotRunBatch()) );
    connect( ViewButtonBatch, SIGNAL(clicked()), this, SLOT(slotViewBatch()) );

    RunButtonBatch->setEnabled( false );
    ViewButtonBatch->setEnabled( false );

    show();
}

modCalcDayLength::~modCalcDayLength() {}

void modCalcDayLength::showCurrentDate (void)
{
    KStarsDateTime dt( KStarsDateTime::currentDateTime() );
    Date->setDate( dt.date() );
}

void modCalcDayLength::initGeo(void)
{
    KStarsData *data = KStarsData::Instance();
    geoPlace = data->geo();
    geoBatch = data->geo();
    Location->setText( geoPlace->fullName() );
    LocationBatch->setText( geoBatch->fullName() );
}

QTime modCalcDayLength::lengthOfDay(QTime setQTime, QTime riseQTime){
    QTime dL(0,0,0);
    int dds = riseQTime.secsTo(setQTime);
    QTime dLength = dL.addSecs( dds );

    return dLength;
}

void modCalcDayLength::slotLocation() {
    QPointer<LocationDialog> ld = new LocationDialog( this );
    if ( ld->exec() == QDialog::Accepted ) {
        GeoLocation *newGeo = ld->selectedCity();
        if ( newGeo ) {
            geoPlace = newGeo;
            Location->setText( geoPlace->fullName() );
        }
    }
    delete ld;

    slotComputeAlmanac();
}

void modCalcDayLength::slotLocationBatch() {
    QPointer<LocationDialog> ld = new LocationDialog( this );
    if ( ld->exec() == QDialog::Accepted ) {
        GeoLocation *newGeo = ld->selectedCity();
        if ( newGeo ) {
            geoBatch = newGeo;
            LocationBatch->setText( geoBatch->fullName() );
        }
    }
    delete ld;
}

void modCalcDayLength::updateAlmanac( const QDate &d, GeoLocation *geo ) {
    //Determine values needed for the Almanac
    long double jd0 = KStarsDateTime(d, QTime(8,0,0)).djd();
    KSNumbers num(jd0);

    //Sun
    KSSun Sun;
    Sun.findPosition(&num);

    QTime ssTime = Sun.riseSetTime(jd0 , geo, false );
    QTime srTime = Sun.riseSetTime(jd0 , geo, true );
    QTime stTime = Sun.transitTime(jd0 , geo);

    dms ssAz  = Sun.riseSetTimeAz(jd0, geo, false);
    dms srAz  = Sun.riseSetTimeAz(jd0, geo, true);
    dms stAlt = Sun.transitAltitude(jd0, geo);

    //In most cases, the Sun will rise and set:
    if ( ssTime.isValid() ) {
        ssAzString = ssAz.toDMSString();
        stAltString = stAlt.toDMSString();
        srAzString = srAz.toDMSString();

        ssTimeString = KLocale::global()->formatTime( ssTime );
        srTimeString = KLocale::global()->formatTime( srTime );
        stTimeString = KLocale::global()->formatTime( stTime );

        QTime daylength = lengthOfDay(ssTime,srTime);
        daylengthString = KLocale::global()->formatTime( daylength, false, true );

        //...but not always!
    } else if ( stAlt.Degrees() > 0. ) {
        ssAzString = i18n("Circumpolar");
        stAltString = stAlt.toDMSString();
        srAzString = i18n("Circumpolar");

        ssTimeString = "--:--";
        srTimeString = "--:--";
        stTimeString = KLocale::global()->formatTime( stTime );
        daylengthString = "24:00";

    } else if (stAlt.Degrees() < 0. ) {
        ssAzString = i18n("Does not rise");
        stAltString = stAlt.toDMSString();
        srAzString = i18n("Does not set");

        ssTimeString = "--:--";
        srTimeString = "--:--";
        stTimeString = KLocale::global()->formatTime( stTime );
        daylengthString = "00:00";
    }

    //Moon
    KSMoon Moon;

    QTime msTime = Moon.riseSetTime( jd0 , geo, false );
    QTime mrTime = Moon.riseSetTime( jd0 , geo, true );
    QTime mtTime = Moon.transitTime(jd0 , geo);

    dms msAz  = Moon.riseSetTimeAz(jd0, geo, false);
    dms mrAz  = Moon.riseSetTimeAz(jd0, geo, true);
    dms mtAlt = Moon.transitAltitude(jd0, geo);

    //In most cases, the Moon will rise and set:
    if ( msTime.isValid() ) {
        msAzString = msAz.toDMSString();
        mtAltString = mtAlt.toDMSString();
        mrAzString = mrAz.toDMSString();

        msTimeString = KLocale::global()->formatTime( msTime );
        mrTimeString = KLocale::global()->formatTime( mrTime );
        mtTimeString = KLocale::global()->formatTime( mtTime );

        //...but not always!
    } else if ( mtAlt.Degrees() > 0. ) {
        msAzString = i18n("Circumpolar");
        mtAltString = mtAlt.toDMSString();
        mrAzString = i18n("Circumpolar");

        msTimeString = "--:--";
        mrTimeString = "--:--";
        mtTimeString = KLocale::global()->formatTime( mtTime );

    } else if ( mtAlt.Degrees() < 0. ) {
        msAzString = i18n("Does not rise");
        mtAltString = mtAlt.toDMSString();
        mrAzString = i18n("Does not rise");

        msTimeString = "--:--";
        mrTimeString = "--:--";
        mtTimeString = KLocale::global()->formatTime( mtTime );
    }

    //after calling riseSetTime Phase needs to reset, setting it before causes Phase to set nan
    Moon.findPosition(&num);
    Moon.findPhase();
    lunarphaseString = Moon.phaseName()+" ("+QString::number( int( 100*Moon.illum() ) )+"%)";

    //Fix length of Az strings
    if ( srAz.Degrees() < 100.0 ) srAzString = ' '+srAzString;
    if ( ssAz.Degrees() < 100.0 ) ssAzString = ' '+ssAzString;
    if ( mrAz.Degrees() < 100.0 ) mrAzString = ' '+mrAzString;
    if ( msAz.Degrees() < 100.0 ) msAzString = ' '+msAzString;
}

void modCalcDayLength::slotComputeAlmanac() {
    updateAlmanac( Date->date(), geoPlace );

    SunSet->setText( ssTimeString );
    SunRise->setText( srTimeString );
    SunTransit->setText( stTimeString );
    SunSetAz->setText( ssAzString );
    SunRiseAz->setText( srAzString );
    SunTransitAlt->setText( stAltString );
    DayLength->setText( daylengthString );

    MoonSet->setText( msTimeString );
    MoonRise->setText( mrTimeString );
    MoonTransit->setText( mtTimeString );
    MoonSetAz->setText( msAzString );
    MoonRiseAz->setText( mrAzString );
    MoonTransitAlt->setText( mtAltString );
    LunarPhase->setText( lunarphaseString );
}

void modCalcDayLength::slotCheckFiles() {
    bool flag = !InputFileBatch->lineEdit()->text().isEmpty() && !OutputFileBatch->lineEdit()->text().isEmpty();
    RunButtonBatch->setEnabled( flag );
}

void modCalcDayLength::slotRunBatch() {
    QString inputFileName = InputFileBatch->url().toLocalFile();

    if ( QFile::exists(inputFileName) ) {
        QFile f( inputFileName );
        if ( !f.open( QIODevice::ReadOnly) ) {
            QString message = i18n( "Could not open file %1.", f.fileName() );
            KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
            return;
        }

        QTextStream istream(&f);
        processLines( istream );
        ViewButtonBatch->setEnabled( true );

        f.close();
    } else  {
        QString message = i18n( "Invalid file: %1", inputFileName );
        KMessageBox::sorry( 0, message, i18n( "Invalid file" ) );
        return;
    }
}

void modCalcDayLength::processLines( QTextStream &istream ) {
    QFile fOut( OutputFileBatch->url().toLocalFile() );
    fOut.open(QIODevice::WriteOnly);
    QTextStream ostream(&fOut);

    //Write header
    ostream << "# " << i18nc("%1 is a location on earth", "Almanac for %1", geoBatch->fullName())
    << QString("  [%1, %2]").arg(geoBatch->lng()->toDMSString()).arg(geoBatch->lat()->toDMSString()) << endl
    << "# " << i18n("computed by KStars") << endl
    << "#" << endl
    << "# Date      SRise  STran  SSet     SRiseAz      STranAlt      SSetAz     DayLen    MRise  MTran  MSet      MRiseAz      MTranAlt      MSetAz     LunarPhase" << endl
    << "#" << endl;

    QString line;
    QDate d;

    while ( ! istream.atEnd() ) {
        line = istream.readLine();
        line = line.trimmed();

        //Parse the line as a date, then compute Almanac values
        d = QDate::fromString( line );
        if ( d.isValid() ) {
            updateAlmanac( d, geoBatch );
            ostream << d.toString( Qt::ISODate ) << "  "
            << srTimeString << "  " << stTimeString << "  " << ssTimeString << "  "
            << srAzString << "  " << stAltString << "  " << ssAzString << "  "
            << daylengthString << "    "
            << mrTimeString << "  " << mtTimeString << "  " << msTimeString << "  "
            << mrAzString << "  " << mtAltString << "  " << msAzString << "  "
            << lunarphaseString << endl;
        }
    }
}

void modCalcDayLength::slotViewBatch() {
    QFile fOut( OutputFileBatch->url().toLocalFile() );
    fOut.open(QIODevice::ReadOnly);
    QTextStream istream(&fOut);
    QStringList text;

    while ( ! istream.atEnd() )
        text.append( istream.readLine() );

    fOut.close();

    KMessageBox::informationList( 0, i18n("Results of Almanac calculation"), text, OutputFileBatch->url().toLocalFile() );
}

#include "modcalcdaylength.moc"
