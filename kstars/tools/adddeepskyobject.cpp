/***************************************************************************
                adddeepskyobject.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Wed 17 Aug 2016 20:22:58 CDT
    copyright            : (c) 2016 by Akarsh Simha
    email                : akarsh.simha@kdemail.net
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


/* Project Includes */
#include "adddeepskyobject.h"

/* KDE Includes */
#include <KMessageBox>

/* Qt Includes */
#include <QInputDialog>

AddDeepSkyObject::AddDeepSkyObject( QWidget *parent, SyncedCatalogComponent *catalog ) :
    QDialog( parent ), m_catalog( catalog ), ui( new Ui::AddDeepSkyObject ) {
    Q_ASSERT( catalog );
    ui->setupUi( this );

    // Set up various things in the dialog so it is ready to be shown
    for( int k = 0; k < SkyObject::NUMBER_OF_KNOWN_TYPES; ++k ) {
        ui->typeComboBox->addItem( SkyObject::typeName( k ) );
    }

    ui->typeComboBox->addItem( SkyObject::typeName( SkyObject::TYPE_UNKNOWN ) );
    ui->catalogNameEdit->setEnabled( false );
    ui->catalogNameEdit->setText( catalog->name() );
    ui->raInput->setDegType( false );

    resetView();

    // Connections
    //    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(slotOk()));
    QAbstractButton *resetButton = ui->buttonBox->button( QDialogButtonBox::Reset );
    connect( resetButton, SIGNAL( clicked() ), this, SLOT( resetView() ) );
    connect( ui->fillFromTextButton,  SIGNAL( clicked() ), this, SLOT( slotFillFromText() ) );

    // Show the UI
    show();
}

AddDeepSkyObject::~AddDeepSkyObject() {
    delete ui;
}

void AddDeepSkyObject::fillFromText( const QString &text ) {
    // Parse text to fill in the options

    // TODO: Add code to match and guess object type, to match blue magnitude.

    QRegularExpression matchJ2000Line( "^(.*)(?:J2000|ICRS|FK5|\\(2000(?:\\.0)?\\))(.*)$" );
    matchJ2000Line.setPatternOptions( QRegularExpression::MultilineOption );
    QRegularExpression matchCoords( "(?:^|[^-\\d])([-+]?\\d\\d?)(?:h ?|[^\\d]?° ?|:| +)(\\d\\d)(?:m ?|\' ?|:| +)(\\d\\d(?:\\.\\d+)?)(?:s|\"|\'\')?\\b" );
    QRegularExpression findMag1( "(?:[mM]ag(?:nitudes?)?|V(?=\\b))(?:\\s*=|:)?\\s*(-?\\d{1,2}(?:\\.\\d{1,3})?)" );
    QRegularExpression findMag2( "\\b-?\\d{1,2}(\\.\\d{1,3})?\\s*[mM]ag\\b");
    QRegularExpression findSize1( "\\b(\\d{1,3}(?:\\.\\d{1,2})?)\\s*(°|\'|\"|\'\')?\\s*[xX×]\\s*(\\d{1,3}(?:\\.\\d{1,2})?)\\s*(°|\'|\"|\'\')?\\b" );
    QRegularExpression findSize2( "\\b(?:[Ss]ize|[Dd]imensions?|[Dd]iameter)[: ](\\d{1,3}(?:\\.\\d{1,2})?)\\s*(°|\'|\"|\'\')?\\b" );
    QRegularExpression findMajorAxis( "\\b[Mm]ajor\\s*[Aa]xis:?\\s*(\\d{1,3}(?:\\.\\d{1,2})?)\\s*(°|\'|\"|\'\')?\\b" );
    QRegularExpression findMinorAxis( "\\b[Mm]inor\\s*[Aa]xis:?\\s*(\\d{1,3}(?:\\.\\d{1,2})?)\\s*(°|\'|\"|\'\')?\\b" );
    QRegularExpression findPA( "\\b(?:[Pp]osition *[Aa]ngle|PA|[pP]\\.[aA]\\.):?\\s*(\\d{1,3}(\\.\\d{1,2})?)(?:°|[Ddeg])?\\b" );
    QRegularExpression findName1( "\\b(?:[nN]ames?[: ]|[iI]dent(?:ifier)?:|[dD]esignation:)\\s*\"?([-+\'A-Za-z0-9 ]*)\"?\\b" );
    QStringList catalogNames;
    catalogNames << "NGC" << "IC" << "M" << "PGC" << "UGC" << "MCG" << "ESO" << "SDSS" << "LEDA"
                 << "IRAS" << "PNG" << "Abell" << "ACO" << "HCG" << "CGCG" << "[IV]+Zw" << "Hickson"
                 << "AGC" << "2MASS" << "RCS2" << "Terzan" << "PN [A-Z0-9]" << "VV" << "PK" << "GSC2"
                 << "LBN" << "LDN" << "Caldwell" << "HIP" << "AM" << "vdB" << "B" << "Shk";
    QRegularExpression findName2( "\\b(" + catalogNames.join( "|" ) + ")\\s+(J?[-+0-9\\.]+[A-Da-h]?)\\b" );
    QRegularExpression findName3( "\\b([A-Za-z]+[0-9]?)\\s+(J?[-+0-9]+[A-Da-h]?)\\b" );

    QString coordText = QString();
    bool coordsFound = false, magFound = false, sizeFound = false, nameFound = false,  positionAngleFound = false;
    dms RA, Dec;
    float mag = NaN::f;
    float majorAxis = NaN::f;
    float minorAxis = NaN::f;
    float positionAngle = 0;
    QString name;
    QRegularExpressionMatch rmatch;
    if ( countNonOverlappingMatches( text, matchCoords ) == 2 ) {
        coordText = text;
    }
    else if ( countNonOverlappingMatches( text, matchCoords ) > 2 ) {
        qDebug() << "Found more than 2 coordinate matches. Trying to match J2000 line.";
        if ( text.indexOf( matchJ2000Line, 0, &rmatch ) >= 0 ) {
            coordText = rmatch.captured( 1 ) + rmatch.captured( 2 );
            qDebug() << "Found a J2000 line match: " << coordText;
            if ( countNonOverlappingMatches( coordText, matchCoords ) != 2 )
                coordText = QString(); // Give up
        }
    }
    if ( !coordText.isEmpty() ) {
        int coord1 = coordText.indexOf( matchCoords, 0, &rmatch );
        Q_ASSERT( coord1 >= 0 );
        RA = dms( rmatch.captured( 1 ) + ' ' + rmatch.captured( 2 ) + ' ' + rmatch.captured( 3 ), false );
        int coord2 = coordText.indexOf( matchCoords, coord1 + rmatch.captured( 0 ).length(), &rmatch );
        Q_ASSERT( coord2 >= 0 );
        Dec = dms( rmatch.captured( 1 ) + ' ' + rmatch.captured( 2 ) + ' ' + rmatch.captured( 3 ), true );
        qDebug() << "Extracted coordinates: " << RA.toHMSString() << " " << Dec.toDMSString();
        coordsFound = true;
    }
    else {
        QStringList matches;
        qDebug() << "Could not extract RA/Dec. Found " << countNonOverlappingMatches( text, matchCoords, &matches ) << " coordinate matches:";
        qDebug() << matches;
    }

    nameFound = true;
    if ( text.contains( findName1, &rmatch ) ) { // Explicit name search
        qDebug() << "Found explicit name field: " << rmatch.captured( 1 ) << " in text " << rmatch.captured( 0 );
        name = rmatch.captured( 1 );
    }
    else if ( text.contains( findName2, &rmatch ) ) {
        qDebug() << "Found known catalog field: " << ( name = rmatch.captured( 1 ) + ' ' + rmatch.captured( 2 ) ) << " in text " << rmatch.captured( 0 );
    }
    else if ( text.contains( findName3, &rmatch ) ) {
        qDebug() << "Found something that looks like a catalog designation: " << ( name = rmatch.captured( 1 ) + ' ' + rmatch.captured( 2 ) ) << " in text " << rmatch.captured( 0 );
    }
    else {
        qDebug() << "Could not find name.";
        nameFound = false;
    }

    magFound = true;
    if ( text.contains( findMag1, &rmatch ) ) {
        qDebug() << "Found magnitude: " << rmatch.captured( 1 ) << " in text " << rmatch.captured( 0 );
        mag = rmatch.captured( 1 ).toFloat();
    }
    else if ( text.contains( findMag2, &rmatch ) ) {
        qDebug() << "Found magnitude: " << rmatch.captured( 1 ) << " in text " << rmatch.captured( 0 );
        mag = rmatch.captured( 1 ).toFloat();
    }
    else {
        qDebug() << "Could not find magnitude.";
        magFound = false;
    }

    sizeFound = true;
    if ( text.contains( findSize1, &rmatch ) ) {
        qDebug() << "Found size: " << rmatch.captured( 1 ) << " x " << rmatch.captured( 3 ) << " with units " << rmatch.captured( 4 ) << " in text " << rmatch.captured( 0 );
        majorAxis = rmatch.captured( 1 ).toFloat();
        QString unitText2;
        if ( rmatch.captured( 2 ).isEmpty() ) {
            unitText2 = rmatch.captured( 4 );
        }
        else {
            unitText2 = rmatch.captured( 2 );
        }

        if ( unitText2.contains( "°" ) )
            majorAxis *= 60;
        else if ( unitText2.contains( "\"" ) || unitText2.contains( "\'\'" ) )
            majorAxis /= 60;

        minorAxis = rmatch.captured( 3 ).toFloat();
        if ( rmatch.captured( 4 ).contains( "°" ) )
            minorAxis *= 60;
        else if ( rmatch.captured( 4 ).contains( "\"" ) || rmatch.captured( 4 ).contains( "\'\'" ) )
            minorAxis /= 60;
        qDebug() << "Major axis = " << majorAxis << "; minor axis = " << minorAxis << " in arcmin";
    }
    else if ( text.contains( findSize2, &rmatch ) ) {
        majorAxis = rmatch.captured( 1 ).toFloat();
        if ( rmatch.captured( 2 ).contains( "°" ) )
            majorAxis *= 60;
        else if ( rmatch.captured( 2 ).contains( "\"" ) || rmatch.captured( 2 ).contains( "\'\'" ) )
            majorAxis /= 60;
        minorAxis = majorAxis;
    }
    else if ( text.contains( findMajorAxis, &rmatch ) ) {
        majorAxis = rmatch.captured( 1 ).toFloat();
        if ( rmatch.captured( 2 ).contains( "°" ) )
            majorAxis *= 60;
        else if ( rmatch.captured( 2 ).contains( "\"" ) || rmatch.captured( 2 ).contains( "\'\'" ) )
            majorAxis /= 60;
        minorAxis = majorAxis;
        if ( text.contains( findMinorAxis, &rmatch ) ) {
            minorAxis = rmatch.captured( 1 ).toFloat();
            if ( rmatch.captured( 2 ).contains( "°" ) )
                minorAxis *= 60;
            else if ( rmatch.captured( 2 ).contains( "\"" ) || rmatch.captured( 2 ).contains( "\'\'" ) )
                minorAxis /= 60;
        }
    }


    else {
        qDebug() << "Could not find size."; // FIXME: Improve to include separate major and minor axis matches, and size matches for round objects.
        sizeFound = false;
    }

    positionAngleFound = true;
    if ( text.contains( findPA, &rmatch ) ) {
        qDebug() << "Found position angle: " << rmatch.captured( 1 ) << " in text " << rmatch.captured( 0 );
        positionAngle = rmatch.captured( 1 ).toFloat();
    }
    else {
        qDebug() << "Could not find position angle.";
        positionAngleFound = false;
    }

    if ( nameFound )
        ui->longNameEdit->setText( name );
    if ( magFound )
        ui->visualMagnitudeInput->setValue( mag ); // Improve band identification (V vs. B)
    if ( coordsFound ) {
        ui->raInput->setDMS( RA.toHMSString() );
        ui->decInput->setDMS( Dec.toDMSString() );
    }
    if ( positionAngleFound )
        ui->positionAngleInput->setValue( positionAngle );
    if ( sizeFound ) {
        ui->majorAxisInput->setValue( majorAxis );
        ui->minorAxisInput->setValue( minorAxis );
    }
}

void AddDeepSkyObject::resetView() {
    ui->actualTypeDisplay->setText( SkyObject::typeName( SkyObject::TYPE_UNKNOWN ) );
    ui->catalogIDInput->setValue( m_catalog->objectList().count() );
    ui->blueMagnitudeInput->setValue( 99.99 );
    ui->visualMagnitudeInput->setValue( 99.99 );
    ui->typeComboBox->setCurrentIndex( ui->typeComboBox->count() - 1 );
    ui->majorAxisInput->setValue( 0.0 );
    ui->minorAxisInput->setValue( 0.0 );
    ui->positionAngleInput->setValue( 0.0 );
    ui->longNameEdit->setText( QString() );
    ui->raInput->setDMS( QString() );
    ui->decInput->setDMS( QString() );
}

bool AddDeepSkyObject::slotOk() {
    // Formulate a CatalogEntryData object
    CatalogEntryData centry;
    bool ok;

    centry.magnitude = ui->visualMagnitudeInput->value();
    centry.flux = ui->blueMagnitudeInput->value();
    centry.ra = ui->raInput->createDms( false, &ok ).Degrees();
    centry.dec = ui->decInput->createDms( true, &ok ).Degrees();
    centry.major_axis = ui->majorAxisInput->value();
    centry.minor_axis = ui->minorAxisInput->value();
    centry.long_name = ui->longNameEdit->text();
    centry.type = ui->typeComboBox->currentIndex();
    centry.position_angle = ui->positionAngleInput->value();
    if( centry.type == ui->typeComboBox->count() - 1 )
        centry.type = SkyObject::TYPE_UNKNOWN;

    // Insert it into the catalog
    bool success = m_catalog->addObject( centry );

    if( !success ) {
        // Display error message
        KMessageBox::sorry( 0, i18n( "Could not add deep-sky object. See console for error message!" ), i18n( "Add deep-sky object" ) );
    }
    // Accept the dialog

    return success;
}

void AddDeepSkyObject::slotFillFromText() {
    bool ok = false;
    QString text = QInputDialog::getMultiLineText( this, i18n( "Add deep-sky object : enter text" ),
                                                   i18n( "Enter the data to guess parameters from:" ), QString(), &ok );
    if ( ok )
        fillFromText( text );
}

int AddDeepSkyObject::countNonOverlappingMatches( const QString &string, const QRegularExpression &regExp, QStringList *list ) {
     int count = 0;
     int matchIndex = -1;
     int lastMatchLength = 1;
     QRegularExpressionMatch rmatch;
     while ( ( matchIndex = string.indexOf( regExp, matchIndex + lastMatchLength, &rmatch ) ) >= 0 ) {
         ++count;
         lastMatchLength = rmatch.captured( 0 ).length();
         if ( list )
             list->append( rmatch.captured( 0 ) );
     }
     return count;
}
