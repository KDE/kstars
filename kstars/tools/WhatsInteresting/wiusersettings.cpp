/***************************************************************************
                          wiusersettings.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2012/09/07
    copyright            : (C) 2012 by Samikshan Bairagya
    email                : samikshan@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#include "wiusersettings.h"
#include "WIView.h"
#include "obsconditions.h"
#include "kdebug.h"

WIUserSettings::WIUserSettings(QWidget* parent, Qt::WindowFlags flags): QWizard(parent, flags)
{
    setupUi(this);
    makeConnections();
}

WIUserSettings::~WIUserSettings() {}

void WIUserSettings::slotFinished( int )
{

    eq = noEquipCheck->isEnabled() ? (ObsConditions::None) : (telescopeCheck->isChecked()
    ?(binocularsCheck->isChecked() ? ObsConditions::Both : ObsConditions::Telescope)
    :(binocularsCheck->isChecked() ? ObsConditions::Binoculars : ObsConditions::None));

    type = (equipmentType->itemText(0)=="Reflector") ? ObsConditions::Reflector : ObsConditions::Refractor;

    wi = new WIView(0, new ObsConditions(bortleClass->value(), eq, aperture->value(), type));
}

void WIUserSettings::makeConnections()
{
    connect(this, SIGNAL(finished(int)), this, SLOT(slotFinished(int)));
    connect(bortleClass, SIGNAL(valueChanged(int)), this, SLOT(slotSetBortleClass(int)));
    connect(telescopeCheck, SIGNAL( toggled(bool)), this, SLOT(slotTelescopeCheck(bool)));
    connect(binocularsCheck, SIGNAL( toggled(bool)), this, SLOT(slotBinocularsCheck(bool)));
    connect(noEquipCheck, SIGNAL( toggled(bool)), this, SLOT(slotNoEquipCheck(bool)));
    connect(aperture, SIGNAL(valueChanged(double)), this, SLOT(slotSetAperture(double)));
    connect(equipmentType, SIGNAL(currentIndexChanged(QString)), this, SLOT(slotSetEqType(QString)));
}

void WIUserSettings::slotSetBortleClass(int value)
{
    bortleClass->setValue(value);
}

void WIUserSettings::slotSetAperture(double value)
{
    aperture->setValue(value);
}


void WIUserSettings::slotTelescopeCheck(bool on)
{
    telescopeCheck->setChecked(on);
    if (on)
        noEquipCheck->setEnabled(false);
}

void WIUserSettings::slotBinocularsCheck(bool on)
{
    binocularsCheck->setChecked(on);
    if (on)
        noEquipCheck->setEnabled(false);
}

void WIUserSettings::slotNoEquipCheck(bool on)
{
    noEquipCheck->setChecked(on);
}

void WIUserSettings::slotSetEqType(QString type)
{
    equipmentType->setCurrentItem(type);
}


#include "wiusersettings.moc"