/***************************************************************************
                          equipmentwriter.cpp  -  description

                             -------------------
    begin                : Friday July 19, 2009
    copyright            : (C) 2009 by Prakash Mohan
    email                : prakash.mohan@kdemail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "equipmentwriter.h"
#include "ui_equipmentwriter.h"

#include <QFile>

#include "kstarsdata.h"
#include "oal.h"
#include "scope.h"
#include "eyepiece.h"
#include "lens.h"
#include "filter.h"

#include <config-kstars.h>

#ifdef HAVE_INDI_H
#include "indi/indidriver.h"
#endif

using namespace OAL;

EquipmentWriter::EquipmentWriter()
{
    QWidget *widget = new QWidget;
    m_Ui.setupUi(widget);
    m_Ui.tabWidget->setCurrentIndex(0);
    setMainWidget(widget);
    setCaption(i18n("Define Equipment"));
    setButtons(KDialog::Close);
    setupFilterTab();

    m_Ks = KStars::Instance();
    m_LogObject = m_Ks->data()->logObject();
    nextFilter = 0;
    nextLens = 0;

    loadEquipment();

    #ifdef HAVE_INDI_H
    ui.driverComboBox->insertItems(1, KStars::Instance()->indiDriver()->driversList.keys());
    #endif

    // Create signal-slot connections
    connect(this, SIGNAL(closeClicked()), this, SLOT(slotClose()));
    connect(m_Ui.addNewScopeButton, SIGNAL(clicked()), this, SLOT(slotAddScope()));
    connect(m_Ui.addNewEyepieceButton, SIGNAL(clicked()), this, SLOT(slotAddEyepiece()));
    connect(m_Ui.addNewLensButton, SIGNAL(clicked()), this, SLOT(slotAddLens()));
    connect(m_Ui.addNewFilterButton, SIGNAL(clicked()), this, SLOT(slotAddFilter()));

    connect(m_Ui.saveScopeButton, SIGNAL(clicked()), this, SLOT(slotSaveScope()));
    connect(m_Ui.saveEyepieceButton, SIGNAL(clicked()), this, SLOT(slotSaveEyepiece()));
    connect(m_Ui.saveLensButton, SIGNAL(clicked()), this, SLOT(slotSaveLens()));
    connect(m_Ui.saveFilterButton, SIGNAL(clicked()), this, SLOT(slotSaveFilter()));

    connect(m_Ui.scopeListWidget, SIGNAL(currentRowChanged(int)),
            this, SLOT(slotSetScope(int)));
    connect(m_Ui.eyepieceListWidget, SIGNAL(currentRowChanged(int)),
            this, SLOT(slotSetEyepiece(int)));
    connect(m_Ui.lensListWidget, SIGNAL(currentRowChanged(int)),
            this, SLOT(slotSetLens(int)));
    connect(m_Ui.filterListWidget, SIGNAL(currentRowChanged(int)),
            this, SLOT(slotSetFilter(int)));

    connect(m_Ui.removeScopeButton, SIGNAL(clicked()), this, SLOT(slotRemoveScope()));
    connect(m_Ui.removeEyepieceButton, SIGNAL(clicked()), this, SLOT(slotRemoveEyepiece()));
    connect(m_Ui.removeLensButton, SIGNAL(clicked()), this, SLOT(slotRemoveLens()));
    connect(m_Ui.removeFilterButton, SIGNAL(clicked()), this, SLOT(slotRemoveFilter()));

    connect(m_Ui.scopeGraspDefinedCheckBox, SIGNAL(toggled(bool)), this, SLOT(slotLightGraspDefined(bool)));
    connect(m_Ui.scopeOrientationDefinedCheckBox, SIGNAL(toggled(bool)), this, SLOT(slotOrientationDefined(bool)));
}

void EquipmentWriter::slotAddScope()
{
    Scope *scope = new Scope("telescope_" + QString::number(m_LogObject->scopeList()->size()), m_Ui.scopeModelLineEdit->text(),
                             m_Ui.scopeVendorLineEdit->text(), m_Ui.scopeTypeComboBox->currentText(),
                             m_Ui.scopeFocalLengthSpinBox->value(), m_Ui.scopeApertureSpinBox->value(),
                             m_Ui.scopeLightGraspSpinBox->value() / 100, m_Ui.scopeGraspDefinedCheckBox->isChecked(),
                             m_Ui.scopeErectedCheckBox->isChecked(), m_Ui.scopeTruesidedCheckBox->isChecked(),
                             m_Ui.scopeOrientationDefinedCheckBox->isChecked());

    m_LogObject->scopeList()->append(scope);
    scope->setINDIDriver(m_Ui.scopeDriverComboBox->currentText());

    m_Ui.scopeListWidget->addItem(scope->name());

    saveEquipment(); //Save the new list.
}

void EquipmentWriter::slotRemoveScope()
{
    int idx = m_Ui.scopeListWidget->currentRow();
    if(idx < 0 || idx >= m_LogObject->scopeList()->size())
        return;

    m_LogObject->removeScope(idx);

    QListWidgetItem *item = m_Ui.scopeListWidget->takeItem(idx);
    if(item)
        delete item;

    clearScopePage();

    saveEquipment(); //Save the new list.
}

void EquipmentWriter::slotSaveScope()
{
    int idx = m_Ui.scopeListWidget->currentRow();
    if(idx < 0 || idx >= m_LogObject->scopeList()->size())
        return;

    Scope *scope = m_LogObject->scopeList()->at(idx);
    if(scope) {
        scope->setScope(m_Ui.scopeIdLineEdit->text(), m_Ui.scopeModelLineEdit->text(),
                        m_Ui.scopeVendorLineEdit->text(), m_Ui.scopeTypeComboBox->currentText(),
                        m_Ui.scopeFocalLengthSpinBox->value(), m_Ui.scopeApertureSpinBox->value(),
                        m_Ui.scopeLightGraspSpinBox->value() / 100, m_Ui.scopeGraspDefinedCheckBox->isChecked(),
                        m_Ui.scopeErectedCheckBox->isChecked(), m_Ui.scopeTruesidedCheckBox->isChecked(),
                        m_Ui.scopeOrientationDefinedCheckBox->isChecked());

        scope->setINDIDriver(m_Ui.scopeDriverComboBox->currentText());

        m_Ui.scopeListWidget->item(idx)->setText(scope->name());

        saveEquipment(); //Save the new list.
    }
}
 
void EquipmentWriter::slotSetScope(int idx)
{
    clearScopePage();

    if(idx < 0 || idx >= m_LogObject->scopeList()->size())
        return;

    OAL::Scope *scope = m_LogObject->scopeList()->at(idx);
    if(scope) {
        m_Ui.scopeIdLineEdit->setText(scope->id());
        m_Ui.scopeModelLineEdit->setText(scope->model());
        m_Ui.scopeVendorLineEdit->setText(scope->vendor());
        int typeIdx = m_Ui.scopeTypeComboBox->findText(scope->type());
        if(typeIdx == -1) {
            m_Ui.scopeTypeComboBox->lineEdit()->setText(scope->type());
        } else {
            m_Ui.scopeTypeComboBox->setEditText(scope->type());
        }
        m_Ui.scopeTypeComboBox->setCurrentIndex(m_Ui.scopeTypeComboBox->findText(scope->type()));
        m_Ui.scopeApertureSpinBox->setValue(scope->aperture());
        m_Ui.scopeFocalLengthSpinBox->setValue(scope->focalLength());
        m_Ui.scopeDriverComboBox->setCurrentIndex(m_Ui.scopeDriverComboBox->findText(scope->driver()));

        if(scope->isLightGraspDefined()) {
            m_Ui.scopeGraspDefinedCheckBox->setChecked(true);
            m_Ui.scopeLightGraspSpinBox->setValue(scope->lightGrasp() * 100);
        }

        if(scope->isOrientationDefined()) {
            m_Ui.scopeOrientationDefinedCheckBox->setChecked(true);
            m_Ui.scopeErectedCheckBox->setChecked(scope->orientationErect());
            m_Ui.scopeTruesidedCheckBox->setChecked(scope->orientationTruesided());
        }
    }
}

void EquipmentWriter::slotAddEyepiece()
{
    Eyepiece *eyepiece = new Eyepiece("eyepiece_" + QString::number(m_LogObject->eyepieceList()->size()),
                                      m_Ui.eyepieceModelLineEdit->text(), m_Ui.eyepieceVendorLineEdit->text(), m_Ui.eyepieceFovSpinBox->value(),
                                      m_Ui.eyepieceFovUnitComboBox->currentText(), m_Ui.eyepieceFovDefinedCheckBox->isChecked(),
                                      m_Ui.eyepieceFocalLengthSpinBox->value(), m_Ui.eyepieceMaxFocalLengthSpinBox->value(),
                                      m_Ui.eyepieceIsZoomCheckBox->isChecked());

    m_LogObject->eyepieceList()->append(eyepiece);

    m_Ui.eyepieceListWidget->addItem(eyepiece->name());

    saveEquipment(); //Save the new list.
}

void EquipmentWriter::slotRemoveEyepiece()
{
    int idx = m_Ui.eyepieceListWidget->currentRow();
    if(idx < 0 || idx >= m_LogObject->eyepieceList()->size())
        return;

    m_LogObject->removeEyepiece(idx);

    QListWidgetItem *item = m_Ui.eyepieceListWidget->takeItem(idx);
    if(item)
        delete item;

    clearEyepiecePage();

    saveEquipment(); //Save the new list.
}
void EquipmentWriter::slotSaveEyepiece()
{
    int idx = m_Ui.eyepieceListWidget->currentRow();
    if(idx < 0 || idx >= m_LogObject->eyepieceList()->size())
        return;

    Eyepiece *eyepiece = m_LogObject->eyepieceList()->at(idx);
    if(eyepiece) {
        eyepiece->setEyepiece(m_Ui.eyepieceIdLineEdit->text(), m_Ui.eyepieceModelLineEdit->text(), m_Ui.eyepieceVendorLineEdit->text(),
                              m_Ui.eyepieceFovSpinBox->value(), m_Ui.eyepieceFovUnitComboBox->currentText(), m_Ui.eyepieceFovDefinedCheckBox->isChecked(),
                              m_Ui.eyepieceFocalLengthSpinBox->value(), m_Ui.eyepieceMaxFocalLengthSpinBox->value(), m_Ui.eyepieceIsZoomCheckBox->isEnabled());

        m_Ui.eyepieceListWidget->item(idx)->setText(eyepiece->name());

        saveEquipment(); //Save the new list.
    }
}

void EquipmentWriter::slotSetEyepiece(int idx)
{
    clearEyepiecePage();

    if(idx < 0 || idx >= m_LogObject->eyepieceList()->size())
        return;

    Eyepiece *eyepiece = m_LogObject->eyepieceList()->at(idx);
    if(eyepiece) {
        m_Ui.eyepieceIdLineEdit->setText(eyepiece->id());
        m_Ui.eyepieceModelLineEdit->setText(eyepiece->model());
        m_Ui.eyepieceVendorLineEdit->setText(eyepiece->vendor());
        m_Ui.eyepieceFocalLengthSpinBox->setValue(eyepiece->focalLength());

        if(eyepiece->isMaxFocalLengthDefined()) {
            m_Ui.eyepieceIsZoomCheckBox->setChecked(true);
            m_Ui.eyepieceMaxFocalLengthSpinBox->setEnabled(true);
            m_Ui.eyepieceMaxFocalLengthSpinBox->setValue(eyepiece->maxFocalLength());
        }

        if(eyepiece->isFovDefined()) {
            m_Ui.eyepieceFovDefinedCheckBox->setChecked(true);
            m_Ui.eyepieceFovSpinBox->setEnabled(true);
            m_Ui.eyepieceFovUnitComboBox->setEnabled(true);
            m_Ui.eyepieceFovSpinBox->setValue(eyepiece->appFov());
            m_Ui.eyepieceFovUnitComboBox->setCurrentIndex(m_Ui.eyepieceFovUnitComboBox->findText(eyepiece->fovUnit()));
        }
    }
}

void EquipmentWriter::slotAddLens()
{
    Lens *lens = new Lens("lens_" + QString::number(m_LogObject->lensList()->size()), m_Ui.lensModelLineEdit->text(),
                          m_Ui.lensVendorLineEdit->text(), m_Ui.lensFactorSpinBox->value());

    m_LogObject->lensList()->append(lens);

    m_Ui.lensListWidget->addItem(lens->name());

    clearLensPage();

    saveEquipment(); //Save the new list.
}

void EquipmentWriter::slotRemoveLens()
{
    int idx = m_Ui.lensListWidget->currentRow();
    if(idx < 0 || idx >= m_LogObject->lensList()->size())
        return;

    m_LogObject->removeLens(idx);

    QListWidgetItem *item = m_Ui.lensListWidget->takeItem(idx);
    if(item)
        delete item;

    clearLensPage();

    saveEquipment(); //Save the new list.
}

void EquipmentWriter::slotSaveLens()
{
    int idx = m_Ui.lensListWidget->currentRow();
    if(idx < 0 || idx >= m_LogObject->lensList()->size())
        return;

    Lens *lens = m_LogObject->lensList()->at(idx);
    if(lens) {
        lens->setLens(m_Ui.lensIdLineEdit->text(), m_Ui.lensModelLineEdit->text(), m_Ui.lensVendorLineEdit->text(),
                      m_Ui.lensFactorSpinBox->value());

        m_Ui.lensListWidget->item(idx)->setText(lens->name());

        saveEquipment(); //Save the new list.
    }
}

void EquipmentWriter::slotSetLens(int idx)
{
    clearLensPage();

    if(idx < 0 || idx >= m_LogObject->lensList()->size())
        return;

    Lens *lens = m_LogObject->lensList()->at(idx);
    if(lens) {
        m_Ui.lensIdLineEdit->setText(lens->id());
        m_Ui.lensVendorLineEdit->setText(lens->vendor());
        m_Ui.lensModelLineEdit->setText(lens->model());
        m_Ui.lensFactorSpinBox->setValue(lens->factor());
    }
}

void EquipmentWriter::slotAddFilter()
{
    Filter *filter = new Filter("filter_" + QString::number(m_LogObject->filterList()->size()),
                                m_Ui.filterModelLineEdit->text(), m_Ui.filterVendorLineEdit->text(),
                                static_cast<Filter::FILTER_TYPE>(m_Ui.filterTypeComboBox->currentIndex()),
                                static_cast<Filter::FILTER_COLOR>(m_Ui.filterColorComboBox->currentIndex()));

    m_LogObject->filterList()->append(filter);

    m_Ui.filterListWidget->addItem(filter->name());

    clearFilterPage();

    saveEquipment(); //Save the new list.
}

void EquipmentWriter::slotRemoveFilter()
{
    int idx = m_Ui.filterListWidget->currentRow();
    if(idx < 0 || idx >= m_LogObject->filterList()->size())
        return;

    m_LogObject->removeFilter(idx);

    QListWidgetItem *item = m_Ui.filterListWidget->takeItem(idx);
    if(item)
        delete item;

    clearFilterPage();

    saveEquipment(); //Save the new list.
}

void EquipmentWriter::slotSaveFilter()
{
    int idx = m_Ui.filterListWidget->currentRow();
    if(idx < 0 || idx >= m_LogObject->filterList()->size())
        return;

    Filter *filter = m_LogObject->filterList()->at(idx);
    if(filter) {
        filter->setFilter("filter_" + QString::number(m_LogObject->filterList()->size()),
                          m_Ui.filterModelLineEdit->text(), m_Ui.filterVendorLineEdit->text(),
                          static_cast<Filter::FILTER_TYPE>(m_Ui.filterTypeComboBox->currentIndex()),
                          static_cast<Filter::FILTER_COLOR>(m_Ui.filterColorComboBox->currentIndex()));

        m_Ui.filterListWidget->item(idx)->setText(filter->name());

        saveEquipment(); //Save the new list.
    }
}

void EquipmentWriter::slotSetFilter(int idx)
{
    if(idx < 0 || idx >= m_LogObject->filterList()->size())
        return;

    Filter *filter = m_LogObject->filterList()->at(idx);
    if(filter) {
        m_Ui.filterIdLineEdit->setText(filter->id());
        m_Ui.filterModelLineEdit->setText(filter->model());
        m_Ui.filterVendorLineEdit->setText(filter->vendor());
        m_Ui.filterTypeComboBox->setCurrentIndex(filter->type());
        m_Ui.filterColorComboBox->setCurrentIndex(filter->color());
    }
}

void EquipmentWriter::saveEquipment()
{
    m_Ks->data()->logObject()->saveEquipmentToFile();

#ifdef HAVE_INDI_H
    ks->indiDriver()->updateCustomDrivers();
#endif
}

void EquipmentWriter::loadEquipment()
{
    m_LogObject->loadEquipmentFromFile();

    m_Ui.scopeListWidget->clear();
    m_Ui.eyepieceListWidget->clear();
    m_Ui.lensListWidget->clear();
    m_Ui.filterListWidget->clear();

    foreach(Scope *scope, *(m_LogObject->scopeList())) {
        m_Ui.scopeListWidget->addItem(scope->name());
    }
    foreach(Eyepiece *eyepiece, *(m_LogObject->eyepieceList())) {
        m_Ui.eyepieceListWidget->addItem(eyepiece->name());
    }
    foreach(Lens *lens, *(m_LogObject->lensList())) {
        m_Ui.lensListWidget->addItem(lens->name());
    }
    foreach(Filter *filter, *(m_LogObject->filterList())) {
        m_Ui.filterListWidget->addItem(filter->name());
    }
}

void EquipmentWriter::slotClose()
{
   hide();
}

void EquipmentWriter::slotLightGraspDefined(bool enabled)
{
    m_Ui.scopeLightGraspSpinBox->setEnabled(enabled);
}

void EquipmentWriter::slotOrientationDefined(bool enabled)
{
    m_Ui.scopeErectedCheckBox->setEnabled(enabled);
    m_Ui.scopeTruesidedCheckBox->setEnabled(enabled);
}

void EquipmentWriter::slotFovDefined(bool enabled)
{
    m_Ui.eyepieceFovSpinBox->setEnabled(enabled);
    m_Ui.eyepieceFovUnitComboBox->setEnabled(enabled);
}

void EquipmentWriter::slotZoomEyepieceDefined(bool enabled)
{
    m_Ui.eyepieceIsZoomCheckBox->setEnabled(enabled);
}

void EquipmentWriter::setupFilterTab()
{
    m_Ui.filterTypeComboBox->addItems(Filter::filterTypes());
    m_Ui.filterColorComboBox->addItems(Filter::filterColors());
}

void EquipmentWriter::clearScopePage()
{
    m_Ui.scopeIdLineEdit->clear();
    m_Ui.scopeModelLineEdit->clear();
    m_Ui.scopeVendorLineEdit->clear();
    m_Ui.scopeTypeComboBox->setCurrentIndex(0);
    m_Ui.scopeDriverComboBox->setCurrentIndex(0);
    m_Ui.scopeApertureSpinBox->setValue(0);
    m_Ui.scopeFocalLengthSpinBox->setValue(0);
    m_Ui.scopeLightGraspSpinBox->setValue(0);
    m_Ui.scopeErectedCheckBox->setChecked(false);
    m_Ui.scopeTruesidedCheckBox->setChecked(false);

    m_Ui.scopeGraspDefinedCheckBox->setChecked(false);
    m_Ui.scopeLightGraspSpinBox->setEnabled(false);

    m_Ui.scopeOrientationDefinedCheckBox->setChecked(false);
    m_Ui.scopeErectedCheckBox->setEnabled(false);
    m_Ui.scopeTruesidedCheckBox->setEnabled(false);
}

void EquipmentWriter::clearEyepiecePage()
{
    m_Ui.eyepieceIdLineEdit->clear();
    m_Ui.eyepieceVendorLineEdit->clear();
    m_Ui.eyepieceModelLineEdit->clear();
    m_Ui.eyepieceFocalLengthSpinBox->setValue(0);

    m_Ui.eyepieceIsZoomCheckBox->setChecked(false);
    m_Ui.eyepieceMaxFocalLengthSpinBox->setEnabled(false);

    m_Ui.eyepieceFovDefinedCheckBox->setChecked(false);
    m_Ui.eyepieceFovSpinBox->setEnabled(false);
    m_Ui.eyepieceFovUnitComboBox->setEnabled(false);
}

void EquipmentWriter::clearLensPage()
{
    m_Ui.lensIdLineEdit->clear();
    m_Ui.lensVendorLineEdit->clear();
    m_Ui.lensModelLineEdit->clear();
    m_Ui.lensFactorSpinBox->setValue(0);
}

void EquipmentWriter::clearFilterPage()
{
    m_Ui.filterIdLineEdit->clear();
    m_Ui.filterVendorLineEdit->clear();
    m_Ui.filterModelLineEdit->clear();
    m_Ui.filterTypeComboBox->setCurrentIndex(0);
    m_Ui.filterColorComboBox->setCurrentIndex(0);
}

#include "equipmentwriter.moc"
