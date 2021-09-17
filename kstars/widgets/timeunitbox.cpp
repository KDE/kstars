/*
    SPDX-FileCopyrightText: 2002 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "timeunitbox.h"

#include <QToolButton>
#include <QVBoxLayout>

#include <KLocalizedString>

#define TUB_DAYUNITS 5

TimeUnitBox::TimeUnitBox(QWidget *parent, bool daysonly) : QWidget(parent)
{
    QVBoxLayout *vlay = new QVBoxLayout(this);

    vlay->setContentsMargins(0, 0, 0, 0);
    vlay->setSpacing(0);

    UpButton = new QToolButton(this);
    UpButton->setArrowType(Qt::UpArrow);
    UpButton->setMaximumWidth(26);
    UpButton->setMaximumHeight(13);

    IncreaseAction = new QAction(QIcon::fromTheme("go-next-skip"),
                                 i18n("Increase Time Scale"));
    IncreaseAction->setToolTip(i18n("Increase time scale to the next largest unit"));
    connect(IncreaseAction, SIGNAL(triggered()), this, SLOT(increase()));
    UpButton->setDefaultAction(IncreaseAction);

    DownButton = new QToolButton(this);
    DownButton->setArrowType(Qt::DownArrow);
    DownButton->setMaximumWidth(26);
    DownButton->setMaximumHeight(13);

    DecreaseAction = new QAction(QIcon::fromTheme("go-previous-skip"),
                                 i18n("Decrease Time Scale"));
    DecreaseAction->setToolTip(i18n("Decrease time scale to the next smallest unit"));
    connect(DecreaseAction, SIGNAL(triggered()), this, SLOT(decrease()));
    DownButton->setDefaultAction(DecreaseAction);

    vlay->addWidget(UpButton);
    vlay->addWidget(DownButton);

    for (int &item : UnitStep)
        item = 0;

    setDaysOnly(daysonly);
}

void TimeUnitBox::setDaysOnly(bool daysonly)
{
    if (daysonly)
    {
        setMinimum(1 - TUB_DAYUNITS);
        setMaximum(TUB_DAYUNITS - 1);
        setValue(1); // Start out with days units

        UnitStep[0] = 0;
        UnitStep[1] = 1;
        UnitStep[2] = 5;
        UnitStep[3] = 8;
        UnitStep[4] = 14;
    }
    else
    {
        setMinimum(1 - TUB_ALLUNITS);
        setMaximum(TUB_ALLUNITS - 1);
        setValue(1); // Start out with seconds units

        UnitStep[0] = 0;
        UnitStep[1] = 4;
        UnitStep[2] = 10;
        UnitStep[3] = 16;
        UnitStep[4] = 21;
        UnitStep[5] = 25;
        UnitStep[6] = 28;
        UnitStep[7] = 34;
    }
}

void TimeUnitBox::increase()
{
    if (value() < maxValue())
    {
        setValue(value() + 1);
        emit valueChanged(value());
        DecreaseAction->setEnabled(true);
    }
    else
    {
        IncreaseAction->setEnabled(false);
    }
}

void TimeUnitBox::decrease()
{
    if (value() > minValue())
    {
        setValue(value() - 1);
        emit valueChanged(value());
        IncreaseAction->setEnabled(true);
    }
    else
    {
        DecreaseAction->setEnabled(false);
    }
}

int TimeUnitBox::unitValue()
{
    int uval;
    if (value() >= 0)
        uval = UnitStep[value()];
    else
        uval = -1 * UnitStep[abs(value())];
    return uval;
}

int TimeUnitBox::getUnitValue(int val)
{
    if (val >= 0)
        return UnitStep[val];
    else
        return -1 * UnitStep[abs(val)];
}
