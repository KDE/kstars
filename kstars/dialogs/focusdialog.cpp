/***************************************************************************
                          focusdialog.cpp  -  description
                             -------------------
    begin                : Sat Mar 23 2002
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

#include "focusdialog.h"

#include <QVBoxLayout>
#include <QDebug>
#include <KLocalizedString>
#include <kmessagebox.h>
#include <knumvalidator.h>

#include "kstars.h"
#include "kstarsdata.h"
#include "dms.h"
#include "skyobjects/skypoint.h"
#include "skymap.h"

FocusDialogUI::FocusDialogUI( QWidget *parent ) : QFrame( parent ) {
    setupUi( this );
}

FocusDialog::FocusDialog( KStars *_ks )
        : QDialog( _ks ), ks( _ks )
{
    //initialize point to the current focus position
    Point = *ks->map()->focus();

    UsedAltAz = false; //assume RA/Dec by default

    fd = new FocusDialogUI(this);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(fd);
    setLayout(mainLayout);

    setWindowTitle( xi18n( "Set Coordinates Manually" ) );

    //FIXME Need porting to KF5
    //setMainWidget(fd);
    //setButtons( QDialog::Ok|QDialog::Cancel );

    fd->epochBox->setValidator( new QDoubleValidator( fd->epochBox ) );
    fd->raBox->setMinimumWidth( fd->raBox->fontMetrics().boundingRect("00h 00m 00s").width() );
    fd->azBox->setMinimumWidth( fd->raBox->fontMetrics().boundingRect("00h 00m 00s").width() );

    fd->raBox->setDegType(false); //RA box should be HMS-style
    fd->raBox->setFocus(); //set input focus

    //FIXME Need porting to KF5
    //enableButtonOk( false ); //disable until both lineedits are filled

    connect( fd->raBox, SIGNAL(textChanged( const QString & ) ), this, SLOT( checkLineEdits() ) );
    connect( fd->decBox, SIGNAL(textChanged( const QString & ) ), this, SLOT( checkLineEdits() ) );
    connect( fd->azBox, SIGNAL(textChanged( const QString & ) ), this, SLOT( checkLineEdits() ) );
    connect( fd->altBox, SIGNAL(textChanged( const QString & ) ), this, SLOT( checkLineEdits() ) );
    connect( this, SIGNAL( okClicked() ), this, SLOT( validatePoint() ) );
}

FocusDialog::~FocusDialog(){
}

void FocusDialog::checkLineEdits() {
    bool raOk(false), decOk(false), azOk(false), altOk(false);
    fd->raBox->createDms( false, &raOk );
    fd->decBox->createDms( true, &decOk );
    fd->azBox->createDms( true, &azOk );
    fd->altBox->createDms( true, &altOk );

    //FIXME Need porting to KF5
    /*
    if ( ( raOk && decOk ) || ( azOk && altOk ) )
        enableButtonOk( true );
    else
        enableButtonOk( false );
        */
}

void FocusDialog::validatePoint() {
    bool raOk(false), decOk(false), azOk(false), altOk(false);
    dms ra( fd->raBox->createDms( false, &raOk ) ); //false means expressed in hours
    dms dec( fd->decBox->createDms( true, &decOk ) );
    QString message;

    if ( raOk && decOk ) {
        //make sure values are in valid range
        if ( ra.Hours() < 0.0 || ra.Hours() > 24.0 )
            message = xi18n( "The Right Ascension value must be between 0.0 and 24.0." );
        if ( dec.Degrees() < -90.0 || dec.Degrees() > 90.0 )
            message += '\n' + xi18n( "The Declination value must be between -90.0 and 90.0." );
        if ( ! message.isEmpty() ) {
            KMessageBox::sorry( 0, message, xi18n( "Invalid Coordinate Data" ) );
            return;
        }

        Point.set( ra, dec );
        double epoch0 = getEpoch( fd->epochBox->text() );
        long double jd0 = epochToJd ( epoch0 );
        Point.apparentCoord(jd0, ks->data()->ut().djd() );
        Point.EquatorialToHorizontal( ks->data()->lst(), ks->data()->geo()->lat() );

        QDialog::accept();
    } else {
        dms az(  fd->azBox->createDms( true, &azOk ) );
        dms alt( fd->altBox->createDms( true, &altOk ) );

        if ( azOk && altOk ) {
            //make sure values are in valid range
            if ( az.Degrees() < 0.0 || az.Degrees() > 360.0 )
                message = xi18n( "The Azimuth value must be between 0.0 and 360.0." );
            if ( alt.Degrees() < -90.0 || alt.Degrees() > 90.0 )
                message += '\n' + xi18n( "The Altitude value must be between -90.0 and 90.0." );
            if ( ! message.isEmpty() ) {
                KMessageBox::sorry( 0, message, xi18n( "Invalid Coordinate Data" ) );
                return;
            }

            Point.setAz( az );
            Point.setAlt( alt );
            Point.HorizontalToEquatorial( ks->data()->lst(), ks->data()->geo()->lat() );

            UsedAltAz = true;

            QDialog::accept();
        } else {
            QDialog::reject();
        }
    }
}

double FocusDialog::getEpoch (const QString &eName) {
    //If eName is empty (or not a number) assume 2000.0
    bool ok(false);
    double epoch = eName.toDouble( &ok );
    if ( eName.isEmpty() || ! ok )
        return 2000.0;

    return epoch;
}

long double FocusDialog::epochToJd (double epoch) {

    double yearsTo2000 = 2000.0 - epoch;

    if (epoch == 1950.0) {
        return 2433282.4235;
    } else if ( epoch == 2000.0 ) {
        return J2000;
    } else {
        return ( J2000 - yearsTo2000 * 365.2425 );
    }

}


QSize FocusDialog::sizeHint() const
{
    return QSize(240,210);
}

void FocusDialog::activateAzAltPage() const {
    fd->fdTab->setCurrentWidget( fd->aaTab );
    fd->azBox->setFocus();
}
#include "focusdialog.moc"
