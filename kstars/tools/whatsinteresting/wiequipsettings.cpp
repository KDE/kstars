/***************************************************************************
                          wiequipsettings.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2013/09/01
    copyright            : (C) 2013 by Samikshan Bairagya
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

#include "wiequipsettings.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"

WIEquipSettings::WIEquipSettings(KStars* ks): QFrame(ks), m_Ks(ks)
{
    setupUi(this);
    populateScopeListWidget();
    scopeListWidget->setSelectionMode(QAbstractItemView::SingleSelection);

    connect(kcfg_TelescopeCheck, SIGNAL(toggled(bool)), this, SLOT(slotTelescopeCheck(bool)));
    connect(kcfg_BinocularsCheck, SIGNAL(toggled(bool)), this, SLOT(slotBinocularsCheck(bool)));
    connect(kcfg_NoEquipCheck, SIGNAL(toggled(bool)), this, SLOT(slotNoEquipCheck(bool)));
    connect(scopeListWidget, SIGNAL(itemClicked(QListWidgetItem *)), this,
            SLOT(slotScopeSelected(QListWidgetItem *)));
}

void WIEquipSettings::populateScopeListWidget()
{
    ///Get telescope list from KStars user database.
    KStars::Instance()->data()->userdb()->GetAllScopes(m_ScopeList);
    foreach(OAL::Scope *scope, m_ScopeList)
    {
        QListWidgetItem *scopeItem = new QListWidgetItem;
        scopeItem->setText(scope->vendor());
        scopeItem->setData(Vendor, scope->vendor());
        scopeItem->setData(Model, scope->model());
        scopeItem->setData(Aperture, scope->aperture());
        scopeItem->setData(FocalLength, scope->focalLength());
        scopeItem->setData(Type, scope->type());

        scopeListWidget->addItem(scopeItem);
    }
}

void WIEquipSettings::slotTelescopeCheck(bool on)
{
    if (on)
    {
        kcfg_NoEquipCheck->setEnabled(false);
    }
    else
    {
        if (!kcfg_BinocularsCheck->isChecked())
            kcfg_NoEquipCheck->setEnabled(true);
    }
}

void WIEquipSettings::slotBinocularsCheck(bool on)
{
    if (on)
        kcfg_NoEquipCheck->setEnabled(false);
    else
    {
        if (!kcfg_TelescopeCheck->isChecked())
            kcfg_NoEquipCheck->setEnabled(true);
    }
}

void WIEquipSettings::slotNoEquipCheck(bool on)
{
    if (on)
    {
        kcfg_TelescopeCheck->setEnabled(false);
        kcfg_BinocularsCheck->setEnabled(false);
    }
    else
    {
        kcfg_TelescopeCheck->setEnabled(true);
        kcfg_BinocularsCheck->setEnabled(true);
    }
}

void WIEquipSettings::slotScopeSelected(QListWidgetItem* scopeItem)
{
    vendorText->setText(scopeItem->data(Vendor).toString());
    modelText->setText(scopeItem->data(Model).toString());
    apertureText->setText(scopeItem->data(Aperture).toString());

    double telAperture = scopeItem->data(Aperture).toDouble();
    double binoAperture = kcfg_BinocularsAperture->value();
    if (kcfg_BinocularsCheck->checkState() == Qt::Unchecked)
        Options::setAperture(telAperture);
    else             //If both Binoculars and Telescope available then select bigger aperture
        Options::setAperture(telAperture > binoAperture ? telAperture : binoAperture);
}
