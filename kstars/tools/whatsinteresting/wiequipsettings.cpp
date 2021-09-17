/*
    SPDX-FileCopyrightText: 2013 Samikshan Bairagya <samikshan@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "wiequipsettings.h"
#include "oal/equipmentwriter.h"
#include "kstars.h"
#include "Options.h"

#include <QListWidgetItem>

WIEquipSettings::WIEquipSettings() : QFrame(KStars::Instance())
{
    setupUi(this);

    ScopeListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    binoDetailsFrame->setEnabled(kcfg_BinocularsCheck->isChecked());
    scopeFrame->setEnabled(kcfg_TelescopeCheck->isChecked());

    connect(kcfg_TelescopeCheck, SIGNAL(toggled(bool)), this, SLOT(slotTelescopeCheck(bool)));
    connect(kcfg_BinocularsCheck, SIGNAL(toggled(bool)), this, SLOT(slotBinocularsCheck(bool)));
    connect(ScopeListWidget, SIGNAL(currentRowChanged(int)), this, SLOT(slotScopeSelected(int)));
    connect(saveNewScopeButton, SIGNAL(clicked()), this, SLOT(slotAddNewScope()));

    populateScopeListWidget();
}

void WIEquipSettings::populateScopeListWidget()
{
    ScopeListWidget->clear();
    ///Get telescope list from KStars user database.
    KStars::Instance()->data()->userdb()->GetAllScopes(m_ScopeList);
    foreach (OAL::Scope *scope, m_ScopeList)
    {
        QListWidgetItem *scopeItem = new QListWidgetItem;
        scopeItem->setText(scope->vendor());
        scopeItem->setData(Vendor, scope->vendor());
        scopeItem->setData(Model, scope->model());
        scopeItem->setData(Aperture, scope->aperture());
        scopeItem->setData(FocalLength, scope->focalLength());
        scopeItem->setData(Type, scope->type());

        ScopeListWidget->addItem(scopeItem);
    }
    if (ScopeListWidget->count() == 0)
        return;

    vendorText->setText(ScopeListWidget->item(0)->data(Vendor).toString());
    modelText->setText(ScopeListWidget->item(0)->data(Model).toString());
    apertureText->setText(ScopeListWidget->item(0)->data(Aperture).toString().append(" mm"));

    ScopeListWidget->setCurrentRow(Options::scopeListIndex());
}

void WIEquipSettings::slotTelescopeCheck(bool on)
{
    scopeFrame->setEnabled(on);
    Options::setTelescopeCheck(on);
}

void WIEquipSettings::slotBinocularsCheck(bool on)
{
    binoDetailsFrame->setEnabled(on);
    Options::setBinocularsCheck(on);
}

void WIEquipSettings::slotScopeSelected(int row)
{
    if (row == -1)
        return;

    QListWidgetItem *item = ScopeListWidget->item(row);

    if (item == nullptr)
        return;

    vendorText->setText(item->data(Vendor).toString());
    modelText->setText(item->data(Model).toString());
    apertureText->setText(item->data(Aperture).toString().append(" mm"));

    if (item->data(Type).toString() == "Reflector")
        m_TelType = ObsConditions::Reflector;
    else if (item->data(Type).toString() == "Refractor")
        m_TelType = ObsConditions::Refractor;

    Options::setScopeListIndex(row);
}

void WIEquipSettings::slotAddNewScope()
{
    EquipmentWriter equipmentdlg;
    equipmentdlg.loadEquipment();
    equipmentdlg.exec();

    populateScopeListWidget(); //Reload scope list widget
}

void WIEquipSettings::setAperture()
{
    double telAperture  = INVALID_APERTURE;
    double binoAperture = INVALID_APERTURE;

    if (kcfg_TelescopeCheck->isChecked() && ScopeListWidget->selectedItems().isEmpty() == false)
        telAperture = ScopeListWidget->currentItem()->data(Aperture).toDouble();
    if (kcfg_BinocularsCheck->isChecked())
        binoAperture = kcfg_BinocularsAperture->value();
    m_Aperture = telAperture > binoAperture ? telAperture : binoAperture;

    // JM 2016-05-11: This is way over-complicated
    /*
    if (ScopeListWidget->count() == 0)
    {
        if (Options::binocularsCheck()z)
        {
            m_Aperture = kcfg_BinocularsAperture->value();
            return;
        }
        else
        {
            m_Aperture = INVALID_APERTURE;
            return;
        }
    }

    if (!Options::telescopeCheck() && !Options::binocularsCheck())
    {
        m_Aperture = INVALID_APERTURE;
    }
    else if (!Options::telescopeCheck())    //No telescope available, but binoculars available
    {
        m_Aperture = kcfg_BinocularsAperture->value();
    }
    else if (!Options::binocularsCheck())   //No binoculars available, but telescope available
    {
        if (ScopeListWidget->count() == 0)
        {
                m_Aperture = INVALID_APERTURE;
                return;
        }
        else
            m_Aperture = ScopeListWidget->currentItem()->data(Aperture).toDouble();
    }
    else                                    //Both Telescope and Binoculars available
    {
        if (ScopeListWidget->count() == 0)
        {
            m_Aperture = kcfg_BinocularsAperture->value();
            return;
        }
        //If both Binoculars and Telescope available then select bigger aperture
        double telAperture = ScopeListWidget->currentItem()->data(Aperture).toDouble();
        double binoAperture = kcfg_BinocularsAperture->value();
        m_Aperture = telAperture > binoAperture ? telAperture : binoAperture;
    }*/
}
