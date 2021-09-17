/*
    SPDX-FileCopyrightText: 2009 Prakash Mohan <prakash.mohan@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "oal/equipmentwriter.h"
#include "ui_equipmentwriter.h"

#include <QFile>

#include "kstarsdata.h"
#include "oal/oal.h"
#include "oal/scope.h"
#include "oal/eyepiece.h"
#include "oal/lens.h"
#include "oal/filter.h"

#include <config-kstars.h>

#ifdef HAVE_INDI
#include "indi/drivermanager.h"
#endif

EquipmentWriter::EquipmentWriter()
{
    QWidget *widget = new QWidget;
    ui.setupUi(widget);
    ui.tabWidget->setCurrentIndex(0);
    setWindowTitle(i18nc("@title:window", "Configure Equipment"));

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(widget);
    setLayout(mainLayout);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    mainLayout->addWidget(buttonBox);

    QObject::connect(buttonBox, SIGNAL(rejected()), this, SLOT(close()));

    nextScope    = 0;
    nextEyepiece = 0;
    nextFilter   = 0;
    nextLens     = 0;
    loadEquipment();
    newScope    = true;
    newEyepiece = true;
    newLens     = true;
    newFilter   = true;

#ifdef HAVE_INDI
    ui.driverComboBox->insertItems(1, DriverManager::Instance()->getDriversStringList());
#endif

    //make connections
    connect(ui.NewScope, SIGNAL(clicked()), this, SLOT(slotNewScope()));
    connect(ui.NewEyepiece, SIGNAL(clicked()), this, SLOT(slotNewEyepiece()));
    connect(ui.NewLens, SIGNAL(clicked()), this, SLOT(slotNewLens()));
    connect(ui.NewFilter, SIGNAL(clicked()), this, SLOT(slotNewFilter()));
    connect(ui.AddScope, SIGNAL(clicked()), this, SLOT(slotSave()));
    connect(ui.AddEyepiece, SIGNAL(clicked()), this, SLOT(slotSave()));
    connect(ui.AddLens, SIGNAL(clicked()), this, SLOT(slotSave()));
    connect(ui.AddFilter, SIGNAL(clicked()), this, SLOT(slotSave()));
    connect(ui.ScopeList, SIGNAL(currentTextChanged(QString)), this, SLOT(slotSetScope(QString)));
    connect(ui.EyepieceList, SIGNAL(currentTextChanged(QString)), this, SLOT(slotSetEyepiece(QString)));
    connect(ui.LensList, SIGNAL(currentTextChanged(QString)), this, SLOT(slotSetLens(QString)));
    connect(ui.FilterList, SIGNAL(currentTextChanged(QString)), this, SLOT(slotSetFilter(QString)));
    connect(ui.RemoveScope, SIGNAL(clicked()), this, SLOT(slotRemoveScope()));
    connect(ui.RemoveEyepiece, SIGNAL(clicked()), this, SLOT(slotRemoveEyepiece()));
    connect(ui.RemoveLens, SIGNAL(clicked()), this, SLOT(slotRemoveLens()));
    connect(ui.RemoveFilter, SIGNAL(clicked()), this, SLOT(slotRemoveFilter()));
}

void EquipmentWriter::slotAddScope()
{
    KStarsData::Instance()->userdb()->AddScope(ui.Model->text(), ui.Vendor->text(), ui.driverComboBox->currentText(),
                                               ui.Type->currentText(), ui.FocalLength->value(), ui.Aperture->value());
    loadEquipment();
    ui.Model->clear();
    ui.Vendor->clear();
    ui.FocalLength->setValue(0);
    ui.Aperture->setValue(0);
    ui.driverComboBox->setCurrentIndex(0);
}

void EquipmentWriter::slotRemoveScope()
{
    KStarsData::Instance()->userdb()->DeleteEquipment("telescope", ui.Id->text().toInt());
    ui.Model->clear();
    ui.Vendor->clear();
    ui.FocalLength->setValue(0);
    ui.Aperture->setValue(0);
    loadEquipment();
}

void EquipmentWriter::slotSaveScope()
{
    KStarsData::Instance()->userdb()->AddScope(ui.Model->text(), ui.Vendor->text(), ui.driverComboBox->currentText(),
                                               ui.Type->currentText(), ui.FocalLength->value(), ui.Aperture->value(),
                                               ui.Id->text());

    loadEquipment();
}

void EquipmentWriter::slotSetScope(QString name)
{
    //TODO: maybe this should also use the DB? ~~spacetime
    OAL::Scope *s = KStarsData::Instance()->logObject()->findScopeByName(name);
    if (s)
    {
        ui.Id->setText(s->id());
        ui.Model->setText(s->model());
        ui.Vendor->setText(s->vendor());
        ui.Type->setCurrentIndex(ui.Type->findText(s->type()));
        ui.FocalLength->setValue(s->focalLength());
        ui.Aperture->setValue(s->aperture());
        ui.driverComboBox->setCurrentIndex(ui.driverComboBox->findText(s->driver()));
        newScope = false;
    }
}
void EquipmentWriter::slotNewScope()
{
    ui.Id->clear();
    ui.Model->clear();
    ui.Vendor->clear();
    ui.FocalLength->setValue(0);
    ui.driverComboBox->setCurrentIndex(0);
    ui.ScopeList->selectionModel()->clear();
    newScope = true;
}

void EquipmentWriter::slotAddEyepiece()
{
    KStarsData::Instance()->userdb()->AddEyepiece(ui.e_Vendor->text(), ui.e_Model->text(), ui.e_focalLength->value(),
                                                  ui.Fov->value(), ui.FovUnit->currentText());
    loadEquipment();
    ui.e_Id->clear();
    ui.e_Model->clear();
    ui.e_Vendor->clear();
    ui.Fov->setValue(0);
    ui.e_focalLength->setValue(0);
}

void EquipmentWriter::slotRemoveEyepiece()
{
    KStarsData::Instance()->userdb()->DeleteEquipment("eyepiece", ui.e_Id->text().toInt());
    loadEquipment();
    ui.e_Id->clear();
    ui.e_Model->clear();
    ui.e_Vendor->clear();
    ui.Fov->setValue(0);
    ui.e_focalLength->setValue(0);
    ui.EyepieceList->clear();
    foreach (OAL::Eyepiece *e, *(KStarsData::Instance()->logObject()->eyepieceList()))
        ui.EyepieceList->addItem(e->name());
}
void EquipmentWriter::slotSaveEyepiece()
{
    KStarsData::Instance()->userdb()->AddEyepiece(ui.e_Vendor->text(), ui.e_Model->text(), ui.e_focalLength->value(),
                                                  ui.Fov->value(), ui.FovUnit->currentText(), ui.e_Id->text());
    loadEquipment();
}

void EquipmentWriter::slotSetEyepiece(QString name)
{
    //TODO: maybe this should also use the DB? ~~spacetime
    OAL::Eyepiece *e;
    e = KStarsData::Instance()->logObject()->findEyepieceByName(name);
    if (e)
    {
        ui.e_Id->setText(e->id());
        ui.e_Model->setText(e->model());
        ui.e_Vendor->setText(e->vendor());
        ui.Fov->setValue(e->appFov());
        ui.e_focalLength->setValue(e->focalLength());
        newEyepiece = false;
    }
}

void EquipmentWriter::slotNewEyepiece()
{
    ui.e_Id->clear();
    ui.e_Model->clear();
    ui.e_Vendor->clear();
    ui.Fov->setValue(0);
    ui.e_focalLength->setValue(0);
    ui.EyepieceList->selectionModel()->clear();
    newEyepiece = true;
}

void EquipmentWriter::slotAddLens()
{
    KStarsData::Instance()->userdb()->AddLens(ui.l_Vendor->text(), ui.l_Model->text(), ui.l_Factor->value());
    loadEquipment();
    ui.l_Id->clear();
    ui.l_Model->clear();
    ui.l_Vendor->clear();
    ui.l_Factor->setValue(0);
}

void EquipmentWriter::slotRemoveLens()
{
    KStarsData::Instance()->userdb()->DeleteEquipment("lens", ui.l_Id->text().toInt());
    loadEquipment();
    ui.l_Id->clear();
    ui.l_Model->clear();
    ui.l_Vendor->clear();
    ui.l_Factor->setValue(0);
    ui.LensList->clear();
    foreach (OAL::Lens *l, *(KStarsData::Instance()->logObject()->lensList()))
        ui.LensList->addItem(l->name());
}
void EquipmentWriter::slotSaveLens()
{
    KStarsData::Instance()->userdb()->AddLens(ui.l_Vendor->text(), ui.l_Model->text(), ui.l_Factor->value(),
                                              ui.l_Id->text());
    loadEquipment();
}

void EquipmentWriter::slotSetLens(QString name)
{
    OAL::Lens *l;
    l = KStarsData::Instance()->logObject()->findLensByName(name);
    if (l)
    {
        ui.l_Id->setText(l->id());
        ui.l_Model->setText(l->model());
        ui.l_Vendor->setText(l->vendor());
        ui.l_Factor->setValue(l->factor());
        newLens = false;
    }
}

void EquipmentWriter::slotNewLens()
{
    ui.l_Id->clear();
    ui.l_Model->clear();
    ui.l_Vendor->clear();
    ui.l_Factor->setValue(0);
    ui.LensList->selectionModel()->clear();
    newLens = true;
}

void EquipmentWriter::slotAddFilter()
{
    KStarsData::Instance()->userdb()->AddFilter(ui.f_Vendor->text(), ui.f_Model->text(), ui.f_Type->text(),
                                                ui.f_Color->text(), ui.f_Offset->value(), ui.f_Exposure->value(),
                                                ui.f_UseAutoFocus->isChecked(), ui.f_LockedFilter->text(), 0);
    loadEquipment();
    ui.f_Id->clear();
    ui.f_Model->clear();
    ui.f_Vendor->clear();
    ui.f_Type->clear();
    ui.f_Offset->setValue(0);
    ui.f_Color->clear();
    ui.f_Exposure->setValue(1);
    ui.f_LockedFilter->clear();
    ui.f_UseAutoFocus->setChecked(false);
}

void EquipmentWriter::slotRemoveFilter()
{
    KStarsData::Instance()->userdb()->DeleteEquipment("filter", ui.f_Id->text().toInt());
    loadEquipment();
    ui.f_Id->clear();
    ui.f_Model->clear();
    ui.f_Vendor->clear();
    ui.f_Type->clear();
    ui.f_Offset->setValue(0);
    ui.f_Color->clear();
    ui.f_Exposure->setValue(0);
    ui.f_LockedFilter->clear();
    ui.f_UseAutoFocus->setChecked(false);
    ui.FilterList->clear();
    foreach (OAL::Filter *f, *(KStarsData::Instance()->logObject()->filterList()))
        ui.FilterList->addItem(f->name());
}

void EquipmentWriter::slotSaveFilter()
{
    // Add
    if (ui.f_Id->text().isEmpty())
        KStarsData::Instance()->userdb()->AddFilter(ui.f_Vendor->text(), ui.f_Model->text(), ui.f_Type->text(),
                                                    ui.f_Color->text(), ui.f_Offset->value(), ui.f_Exposure->value(),
                                                    ui.f_UseAutoFocus->isChecked(), ui.f_LockedFilter->text(), ui.f_AbsoluteFocusPosition->value());
    // Update Existing
    else
        KStarsData::Instance()->userdb()->AddFilter(ui.f_Vendor->text(), ui.f_Model->text(), ui.f_Type->text(),
                                                    ui.f_Color->text(), ui.f_Offset->value(), ui.f_Exposure->value(),
                                                    ui.f_UseAutoFocus->isChecked(), ui.f_LockedFilter->text(), ui.f_AbsoluteFocusPosition->value(), ui.f_Id->text());

    loadEquipment();
}

void EquipmentWriter::slotSetFilter(QString name)
{
    OAL::Filter *f;
    f = KStarsData::Instance()->logObject()->findFilterByName(name);
    if (f)
    {
        ui.f_Id->setText(f->id());
        ui.f_Model->setText(f->model());
        ui.f_Vendor->setText(f->vendor());
        ui.f_Type->setText(f->type());
        ui.f_Color->setText(f->color());
        ui.f_Offset->setValue(f->offset());
        ui.f_Exposure->setValue(f->exposure());
        ui.f_LockedFilter->setText(f->lockedFilter());
        ui.f_UseAutoFocus->setChecked(f->useAutoFocus());
        ui.f_AbsoluteFocusPosition->setValue(f->absoluteFocusPosition());
        newFilter = false;
    }
}

void EquipmentWriter::slotNewFilter()
{
    ui.f_Id->clear();
    ui.f_Model->clear();
    ui.f_Vendor->clear();
    ui.f_Type->clear();
    ui.f_Model->clear();
    ui.f_Color->clear();
    ui.f_Offset->setValue(0);
    ui.f_Exposure->setValue(1);
    ui.f_LockedFilter->clear();
    ui.f_UseAutoFocus->setChecked(false);
    ui.f_AbsoluteFocusPosition->setValue(0);
    ui.FilterList->selectionModel()->clear();
    newFilter = true;
}

void EquipmentWriter::loadEquipment()
{
    KStarsData::Instance()->logObject()->readScopes();
    KStarsData::Instance()->logObject()->readEyepieces();
    KStarsData::Instance()->logObject()->readLenses();
    KStarsData::Instance()->logObject()->readFilters();
    ui.ScopeList->clear();
    ui.EyepieceList->clear();
    ui.LensList->clear();
    ui.FilterList->clear();
    foreach (OAL::Scope *s, *(KStarsData::Instance()->logObject()->scopeList()))
        ui.ScopeList->addItem(s->name());
    foreach (OAL::Eyepiece *e, *(KStarsData::Instance()->logObject()->eyepieceList()))
        ui.EyepieceList->addItem(e->name());
    foreach (OAL::Lens *l, *(KStarsData::Instance()->logObject()->lensList()))
        ui.LensList->addItem(l->name());
    foreach (OAL::Filter *f, *(KStarsData::Instance()->logObject()->filterList()))
        ui.FilterList->addItem(f->name());

#ifdef HAVE_INDI
    DriverManager::Instance()->updateCustomDrivers();
#endif
}

void EquipmentWriter::slotSave()
{
    switch (ui.tabWidget->currentIndex())
    {
        case 0:
        {
            if (newScope)
                slotAddScope();
            else
                slotSaveScope();
            ui.ScopeList->clear();
            foreach (OAL::Scope *s, *(KStarsData::Instance()->logObject()->scopeList()))
                ui.ScopeList->addItem(s->name());
            break;
        }
        case 1:
        {
            if (newEyepiece)
                slotAddEyepiece();
            else
                slotSaveEyepiece();
            ui.EyepieceList->clear();
            foreach (OAL::Eyepiece *e, *(KStarsData::Instance()->logObject()->eyepieceList()))
                ui.EyepieceList->addItem(e->name());
            break;
        }
        case 2:
        {
            if (newLens)
                slotAddLens();
            else
                slotSaveLens();
            ui.LensList->clear();
            foreach (OAL::Lens *l, *(KStarsData::Instance()->logObject()->lensList()))
                ui.LensList->addItem(l->name());
            break;
        }
        case 3:
        {
            if (newFilter)
                slotAddFilter();
            else
                slotSaveFilter();
            ui.FilterList->clear();
            foreach (OAL::Filter *f, *(KStarsData::Instance()->logObject()->filterList()))
                ui.FilterList->addItem(f->name());
            break;
        }
    }
}

void EquipmentWriter::slotClose()
{
    hide();
}
