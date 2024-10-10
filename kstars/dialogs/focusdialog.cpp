/*
    SPDX-FileCopyrightText: 2002 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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
#ifdef Q_OS_MACOS
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
    //initialize point to the current focus position
    Point = SkyMap::Instance()->focus();

    constexpr const char* J2000EpochString = "2000.0";
    fd = new FocusDialogUI(this);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(fd);
    setLayout(mainLayout);

    setWindowTitle(i18nc("@title:window", "Set Coordinates Manually"));

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(validatePoint()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    okB = buttonBox->button(QDialogButtonBox::Ok);
    okB->setEnabled(false);

    fd->epochBox->setText(J2000EpochString);
    fd->epochBox->setValidator(new QDoubleValidator(fd->epochBox));
    fd->raBox->setMinimumWidth(fd->raBox->fontMetrics().boundingRect("00h 00m 00s").width());
    fd->azBox->setMinimumWidth(fd->raBox->fontMetrics().boundingRect("00h 00m 00s").width());

    fd->raBox->setUnits(dmsBox::HOURS); //RA box should be HMS-style
    fd->raBox->setFocus();        //set input focus

    const SkyPoint *center {nullptr};
    if (SkyMap::Instance()->focusObject())
        center = dynamic_cast<const SkyPoint*>(SkyMap::Instance()->focusObject());
    else
        center = const_cast<const SkyPoint*>(SkyMap::Instance()->focusPoint());

    if (center)
    {
        // Make a copy so as to not affect the existing center point / object
        SkyPoint centerCopy {*center};
        //center->deprecess(KStarsData::Instance()->updateNum());
        centerCopy.catalogueCoord(KStarsData::Instance()->updateNum()->julianDay());
        fd->raBox->show(centerCopy.ra());
        fd->decBox->show(centerCopy.dec());

        fd->azBox->show(centerCopy.az());
        fd->altBox->show(centerCopy.alt());

        checkLineEdits();
    }

    connect(fd->raBox, SIGNAL(textChanged(QString)), this, SLOT(checkLineEdits()));
    connect(fd->decBox, SIGNAL(textChanged(QString)), this, SLOT(checkLineEdits()));
    connect(fd->azBox, SIGNAL(textChanged(QString)), this, SLOT(checkLineEdits()));
    connect(fd->altBox, SIGNAL(textChanged(QString)), this, SLOT(checkLineEdits()));

    connect(fd->J2000B, &QPushButton::clicked, this, [&]()
    {
        fd->epochBox->setText(J2000EpochString);
    });
    connect(fd->JNowB, &QPushButton::clicked, this, [&]()
    {
        fd->epochBox->setText(QString::number(KStarsData::Instance()->lt().epoch(), 'f', 3));
    });

}

void FocusDialog::checkLineEdits()
{
    bool raOk(false), decOk(false), azOk(false), altOk(false);

    fd->raBox->createDms(&raOk);
    fd->decBox->createDms(&decOk);
    fd->azBox->createDms(&azOk);
    fd->altBox->createDms(&altOk);

    if ((raOk && decOk) || (azOk && altOk))
        okB->setEnabled(true);
    else
        okB->setEnabled(false);
}

void FocusDialog::validatePoint()
{
    bool raOk(false), decOk(false), azOk(false), altOk(false);
    QString message;

    if (fd->fdTab->currentWidget() == fd->rdTab)
    {
        //false means expressed in hours
        dms ra(fd->raBox->createDms(&raOk));
        dms dec(fd->decBox->createDms(&decOk));

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

            bool ok { false };
            double epoch0   = KStarsDateTime::stringToEpoch(fd->epochBox->text(), ok);
            if (!ok)
            {
                KSNotification::sorry(message, i18n("Invalid Epoch format"));
                return;
            }
            long double jd0 = KStarsDateTime::epochToJd(epoch0);

            // Set RA and Dec to whatever epoch we have been given (may be J2000, JNow or something completely different)
            Point->setRA(ra);
            Point->setDec(dec);

            if (jd0 != J2000)
            {
                // Compute and set the J2000 coordinates of Point
                Point->catalogueCoord(jd0);
            }
            else
            {
                Point->setRA0(ra);
                Point->setDec0(dec);
            }

            // N.B. At this point (ra, dec) and (ra0, dec0) are both
            // J2000.0 values Therefore, we precess again to get the
            // values for the present draw epoch into (ra, dec)
            Point->apparentCoord(static_cast<long double>(J2000), KStarsData::Instance()->updateNum()->julianDay());

            Point->EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
            // At this point, both (RA, Dec) and (Alt, Az) should correspond to current time
            // (RA0, Dec0) will be J2000.0 -- asimha

            QDialog::accept();
        }
        else
        {
            QDialog::reject();
        }
    }
    else  // Az/Alt tab is active
    {
        dms az(fd->azBox->createDms(&azOk));
        dms alt(fd->altBox->createDms(&altOk));

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
