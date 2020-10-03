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

#include "dms.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksnotification.h"
#include "skymap.h"
#include "skyobjects/skypoint.h"

#include <KLocalizedString>
#include <QDoubleValidator>
#include <KMessageBox>
#include <QPushButton>

#include <QVBoxLayout>

FocusDialogUI::FocusDialogUI(QWidget *parent) : QFrame(parent)
{
    setupUi(this);
}

FocusDialog::FocusDialog() : QDialog(KStars::Instance())
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
    //initialize point to the current focus position
    Point = SkyMap::Instance()->focus();

    fd = new FocusDialogUI(this);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(fd);
    setLayout(mainLayout);

    setWindowTitle(i18n("Set Coordinates Manually"));

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(validatePoint()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    okB = buttonBox->button(QDialogButtonBox::Ok);
    okB->setEnabled(false);

    // When editing epoch, set JNow to false.
    connect(fd->epochBox, &QLineEdit::editingFinished, [this]()
    {
        UseJNow = false;
    });
    fd->epochBox->setText(QString::number(KStarsData::Instance()->lt().epoch(), 'f', 3));
    fd->epochBox->setValidator(new QDoubleValidator(fd->epochBox));
    fd->raBox->setMinimumWidth(fd->raBox->fontMetrics().boundingRect("00h 00m 00s").width());
    fd->azBox->setMinimumWidth(fd->raBox->fontMetrics().boundingRect("00h 00m 00s").width());

    fd->raBox->setDegType(false); //RA box should be HMS-style
    fd->raBox->setFocus();        //set input focus

    SkyPoint *center {nullptr};
    if (SkyMap::Instance()->focusObject())
        center = dynamic_cast<SkyPoint*>(SkyMap::Instance()->focusObject());
    else
        center = SkyMap::Instance()->focusPoint();

    if (center)
    {
        //center->deprecess(KStarsData::Instance()->updateNum());
        center->catalogueCoord(KStarsData::Instance()->updateNum()->julianDay());
        fd->raBox->setDMS(center->ra().toHMSString());
        fd->decBox->setDMS(center->dec().toDMSString());

        fd->azBox->setDMS(center->az().toDMSString());
        fd->altBox->setDMS(center->alt().toDMSString());

        checkLineEdits();
    }

    connect(fd->raBox, SIGNAL(textChanged(QString)), this, SLOT(checkLineEdits()));
    connect(fd->decBox, SIGNAL(textChanged(QString)), this, SLOT(checkLineEdits()));
    connect(fd->azBox, SIGNAL(textChanged(QString)), this, SLOT(checkLineEdits()));
    connect(fd->altBox, SIGNAL(textChanged(QString)), this, SLOT(checkLineEdits()));

    connect(fd->J2000B, &QPushButton::clicked, [this]()
    {
        fd->epochBox->setText("2000.0");
        UseJNow = false;
    });
    connect(fd->JNowB, &QPushButton::clicked, [this]()
    {
        fd->epochBox->setText(QString::number(KStarsData::Instance()->lt().epoch(), 'f', 3));
        UseJNow = true;
    });

}

void FocusDialog::checkLineEdits()
{
    bool raOk(false), decOk(false), azOk(false), altOk(false);

    fd->raBox->createDms(false, &raOk);
    fd->decBox->createDms(true, &decOk);
    fd->azBox->createDms(true, &azOk);
    fd->altBox->createDms(true, &altOk);

    if ((raOk && decOk) || (azOk && altOk))
        okB->setEnabled(true);
    else
        okB->setEnabled(false);
}

void FocusDialog::validatePoint()
{
    bool raOk(false), decOk(false), azOk(false), altOk(false);

    //false means expressed in hours
    dms ra(fd->raBox->createDms(false, &raOk));
    dms dec(fd->decBox->createDms(true, &decOk));

    QString message;

    if (raOk && decOk)
    {
        //make sure values are in valid range
        if (ra.Hours() < 0.0 || ra.Hours() > 24.0)
            message = i18n("The Right Ascension value must be between 0.0 and 24.0.");
        if (dec.Degrees() < -90.0 || dec.Degrees() > 90.0)
            message += '\n' + i18n("The Declination value must be between -90.0 and 90.0.");
        if (!message.isEmpty())
        {
            KSNotification::sorry(message, i18n("Invalid Coordinate Data"));
            return;
        }

        // JNow
        if (UseJNow)
        {
            Point->setRA(ra);
            Point->setDec(dec);
            //Point->deprecess(KStarsData::Instance()->updateNum());
            Point->catalogueCoord(KStarsData::Instance()->updateNum()->julianDay());
            // N.B. At this point (ra, dec) and (ra0, dec0) are both J2000.0 values
            // Therefore, we precess again to get the JNow values in (ra, dec)
            Point->apparentCoord(static_cast<long double>(J2000), KStarsData::Instance()->updateNum()->julianDay());
        }
        else
        {
            bool ok { false };
            double epoch0   = KStarsDateTime::stringToEpoch(fd->epochBox->text(), ok);
            if (!ok)
            {
                KSNotification::sorry(message, i18n("Invalid Epoch format"));
                return;
            }
            long double jd0 = KStarsDateTime::epochToJd(epoch0);

            Point->setRA0(ra);
            Point->setDec0(dec);
            Point->apparentCoord(jd0, KStarsData::Instance()->updateNum()->julianDay());
        }

        Point->EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
        // At this point, both (RA, Dec) and (Alt, Az) should correspond to current time
        // (RA0, Dec0) may either be J2000.0 or some other epoch -- asimha

        QDialog::accept();
    }
    else
    {
        dms az(fd->azBox->createDms(true, &azOk));
        dms alt(fd->altBox->createDms(true, &altOk));

        if (azOk && altOk)
        {
            //make sure values are in valid range
            if (az.Degrees() < 0.0 || az.Degrees() > 360.0)
                message = i18n("The Azimuth value must be between 0.0 and 360.0.");
            if (alt.Degrees() < -90.0 || alt.Degrees() > 90.0)
                message += '\n' + i18n("The Altitude value must be between -90.0 and 90.0.");
            if (!message.isEmpty())
            {
                KSNotification::sorry(message, i18n("Invalid Coordinate Data"));
                return;
            }

            Point->setAz(az);
            Point->setAltRefracted(alt);
            Point->HorizontalToEquatorial(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());

            UsedAltAz = true;

            QDialog::accept();
        }
        else
        {
            QDialog::reject();
        }
    }
}

QSize FocusDialog::sizeHint() const
{
    return QSize(350, 210);
}

void FocusDialog::activateAzAltPage() const
{
    fd->fdTab->setCurrentWidget(fd->aaTab);
    fd->azBox->setFocus();
}
