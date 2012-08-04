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
#include "wiview.h"
#include "obsconditions.h"
#include "kdebug.h"

WIUserSettings::WIUserSettings(QWidget* parent, Qt::WindowFlags flags): QWizard(parent, flags)
{
    setupUi(this);
    makeConnections();
}

void WIUserSettings::makeConnections()
{
    connect(this, SIGNAL(finished(int)), this, SLOT(slotFinished(int)));
    connect(telescopeCheck, SIGNAL( toggled(bool)), this, SLOT(slotTelescopeCheck(bool)));
    connect(binocularsCheck, SIGNAL( toggled(bool)), this, SLOT(slotBinocularsCheck(bool)));
}

void WIUserSettings::slotFinished( int )
{

    eq = noEquipCheck->isEnabled()
            ? (ObsConditions::None)
            : (telescopeCheck->isChecked()
            ? (binocularsCheck->isChecked() ? ObsConditions::Both : ObsConditions::Telescope)
            : (binocularsCheck->isChecked() ? ObsConditions::Binoculars : ObsConditions::None));


    type = (equipmentType->currentText()=="Reflector") ? ObsConditions::Reflector : ObsConditions::Refractor;

    kDebug()<<bortleClass->value()<<eq<<aperture->value()<<type;
    WIView *wi = new WIView(0, new ObsConditions(bortleClass->value(), aperture->value(), eq, type));
}

void WIUserSettings::slotTelescopeCheck(bool on)
{
    if (on)
    {
        equipmentType->setEnabled(true);
        noEquipCheck->setEnabled(false);
    }
}

void WIUserSettings::slotBinocularsCheck(bool on)
{
    if (on && !telescopeCheck->isChecked())
    {
        equipmentType->setEnabled(false);
        noEquipCheck->setEnabled(false);
    }
}

#include "wiusersettings.moc"
