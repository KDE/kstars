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
#include <QFileDialog>

#include <KLocalizedString>
#include <KMessageBox>
#include <KUrlRequester>

#include "dms.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skyobjects/skypoint.h"
#include "ksnumbers.h"
#include "dialogs/finddialog.h"
#include "widgets/dmsbox.h"

modCalcEclCoords::modCalcEclCoords(QWidget *parentSplit)
        : QFrame(parentSplit)
{
    setupUi(this);
    RA->setDegType(false);

    //Initialize Date/Time and Location data
    DateTime->setDateTime( KStarsData::Instance()->lt() );
    kdt = DateTime->dateTime();

    connect(NowButton, SIGNAL(clicked()), this, SLOT(slotNow()));
    connect(ObjectButton, SIGNAL(clicked()), this, SLOT(slotObject()));
    connect(DateTime, SIGNAL(dateTimeChanged(const QDateTime&)), this, SLOT(slotDateTimeChanged(const QDateTime&)));

    connect(RA,     SIGNAL(editingFinished()), this, SLOT(slotCompute()));
    connect(Dec,    SIGNAL(editingFinished()), this, SLOT(slotCompute()));
    connect(EcLong, SIGNAL(editingFinished()), this, SLOT(slotCompute()));
    connect(EcLat,  SIGNAL(editingFinished()), this, SLOT(slotCompute()));

    this->show();
}

modCalcEclCoords::~modCalcEclCoords() {
}

void modCalcEclCoords::slotNow() {
    DateTime->setDateTime( KStarsDateTime::currentDateTime() );
    slotCompute();
}

void modCalcEclCoords::slotObject() {
    FindDialog fd(KStars::Instance());
    if ( fd.exec() == QDialog::Accepted ) {
        SkyObject *o = fd.targetObject();
        RA->showInHours( o->ra() );
        Dec->showInDegrees( o->dec() );
        slotCompute();
    }
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

