/*
    SPDX-FileCopyrightText: 2002 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "modcalcaltaz.h"

#include "geolocation.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "kstarsdatetime.h"
#include "dialogs/finddialog.h"
#include "dialogs/locationdialog.h"
#include "skyobjects/skypoint.h"
#include "widgets/dmsbox.h"

#include <KLocalizedString>
#include <KMessageBox>

#include <QTextStream>
#include <QFileDialog>

modCalcAltAz::modCalcAltAz(QWidget *parentSplit) : QFrame(parentSplit)
{
    setupUi(this);

    KStarsData *data = KStarsData::Instance();
    RA->setDegType(false);

    //Initialize Date/Time and Location data
    geoPlace = data->geo();
    LocationButton->setText(geoPlace->fullName());

    //Make sure slotDateTime() gets called, so that LST will be set
    connect(DateTime, SIGNAL(dateTimeChanged(QDateTime)), this, SLOT(slotDateTimeChanged(QDateTime)));
    DateTime->setDateTime(data->lt());

    connect(NowButton, SIGNAL(clicked()), this, SLOT(slotNow()));
    connect(LocationButton, SIGNAL(clicked()), this, SLOT(slotLocation()));
    connect(ObjectButton, SIGNAL(clicked()), this, SLOT(slotObject()));

    connect(RA, SIGNAL(editingFinished()), this, SLOT(slotCompute()));
    connect(Dec, SIGNAL(editingFinished()), this, SLOT(slotCompute()));
    connect(Az, SIGNAL(editingFinished()), this, SLOT(slotCompute()));
    connect(Alt, SIGNAL(editingFinished()), this, SLOT(slotCompute()));

    show();
}

void modCalcAltAz::slotNow()
{
    DateTime->setDateTime(KStarsDateTime::currentDateTime());
    slotCompute();
}

void modCalcAltAz::slotLocation()
{
    QPointer<LocationDialog> ld = new LocationDialog(this);
    if (ld->exec() == QDialog::Accepted)
    {
        GeoLocation *newGeo = ld->selectedCity();
        if (newGeo)
        {
            geoPlace = newGeo;
            LocationButton->setText(geoPlace->fullName());
            slotCompute();
        }
    }
    delete ld;
}

void modCalcAltAz::slotObject()
{
    if (FindDialog::Instance()->exec() == QDialog::Accepted)
    {
        SkyObject *o = FindDialog::Instance()->targetObject();
        RA->showInHours(o->ra());
        Dec->showInDegrees(o->dec());
        slotCompute();
    }
}

void modCalcAltAz::slotDateTimeChanged(const QDateTime &dt)
{
    KStarsDateTime ut = geoPlace->LTtoUT(KStarsDateTime(dt));
    LST               = geoPlace->GSTtoLST(ut.gst());
}

void modCalcAltAz::slotCompute()
{
    //Determine whether we are calculating Alt/Az coordinates from RA/Dec,
    //or vice versa.  We calculate Alt/Az by default, unless the signal
    //was sent by changing the Az or Alt value.
    if (sender()->objectName() == "Az" || sender()->objectName() == "Alt")
    {
        //Validate Az and Alt coordinates
        bool ok(false);
        dms alt;
        dms az = Az->createDms(true, &ok);
        if (ok)
            alt = Alt->createDms(true, &ok);
        if (ok)
        {
            SkyPoint sp;
            sp.setAz(az);
            sp.setAlt(alt);
            sp.HorizontalToEquatorial(&LST, geoPlace->lat());
            RA->showInHours(sp.ra());
            Dec->showInDegrees(sp.dec());
        }
    }
    else
    {
        //Validate RA and Dec coordinates
        bool ok(false);
        dms ra;
        dms dec = Dec->createDms(true, &ok);
        if (ok)
            ra = RA->createDms(false, &ok);
        if (ok)
        {
            SkyPoint sp(ra, dec);
            sp.EquatorialToHorizontal(&LST, geoPlace->lat());
            Az->showInDegrees(sp.az());
            Alt->showInDegrees(sp.alt());
        }
    }
}
