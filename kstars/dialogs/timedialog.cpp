/*
    SPDX-FileCopyrightText: 2001 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "timedialog.h"

#include "geolocation.h"
#include "kstarsdata.h"
#include "kstarsdatetime.h"
#include "simclock.h"

#include <KLocalizedString>
#include <KDatePicker>

#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QPushButton>
#include <QTimeEdit>
#include <QVBoxLayout>

TimeDialog::TimeDialog(const KStarsDateTime &now, GeoLocation *_geo, QWidget *parent, bool UTCFrame)
    : QDialog(parent), geo(_geo)
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
    UTCNow = UTCFrame;

    QFrame *page = new QFrame(this);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(page);
    setLayout(mainLayout);

    if (UTCNow)
        setWindowTitle(i18nc("@title:window set clock to a new time", "Set UTC Time"));
    else
        setWindowTitle(i18nc("@title:window set clock to a new time", "Set Time"));

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    vlay = new QVBoxLayout(page);
    vlay->setContentsMargins(2, 2, 2, 2);
    vlay->setSpacing(2);
    hlay = new QHBoxLayout(); //this layout will be added to the VLayout
    hlay->setSpacing(2);

    dPicker   = new KDatePicker(now.date(), page);
    tEdit     = new QTimeEdit(now.time(), page);
    NowButton = new QPushButton(page);
    NowButton->setObjectName("NowButton");
    NowButton->setText(UTCNow ? i18n("UTC Now") : i18n("Now"));

    vlay->addWidget(dPicker, 0);
    vlay->addLayout(hlay, 0);

    hlay->addWidget(tEdit);
    hlay->addWidget(NowButton);

    vlay->activate();

    QObject::connect(NowButton, SIGNAL(clicked()), this, SLOT(setNow()));
}

//Add handler for Escape key to close window
//Use keyReleaseEvent because keyPressEvents are already consumed
//by the KDatePicker.
void TimeDialog::keyReleaseEvent(QKeyEvent *kev)
{
    switch (kev->key())
    {
        case Qt::Key_Escape:
        {
            close();
            break;
        }

        default:
        {
            kev->ignore();
            break;
        }
    }
}

void TimeDialog::setNow(void)
{
    KStarsDateTime dt(KStarsDateTime::currentDateTimeUtc());
    if (!UTCNow)
        dt = geo->UTtoLT(dt);

    dPicker->setDate(dt.date());
    tEdit->setTime(dt.time());
}

QTime TimeDialog::selectedTime(void)
{
    return tEdit->time();
}

QDate TimeDialog::selectedDate(void)
{
    return dPicker->date();
}

KStarsDateTime TimeDialog::selectedDateTime(void)
{
    return KStarsDateTime(selectedDate(), selectedTime());
}
