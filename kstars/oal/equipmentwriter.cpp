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
    connect( m_Ui.NewLens, SIGNAL( clicked() ), this, SLOT( slotAddLens() ) );
    connect( m_Ui.NewFilter, SIGNAL( clicked() ), this, SLOT( slotAddFilter() ) );

    connect(m_Ui.saveScopeButton, SIGNAL(clicked()), this, SLOT(slotSaveScope()));
    connect(m_Ui.saveEyepieceButton, SIGNAL(clicked()), this, SLOT(slotSaveEyepiece()));
    connect( m_Ui.SaveLens, SIGNAL( clicked() ), this, SLOT( slotSaveLens() ) );
    connect( m_Ui.SaveFilter, SIGNAL( clicked() ), this, SLOT( slotSaveFilter() ) );

    connect(m_Ui.scopeListWidget, SIGNAL(currentRowChanged(int)),
            this, SLOT(slotSetScope(int)));
    connect(m_Ui.eyepieceListWidget, SIGNAL(currentRowChanged(int)),
            this, SLOT(slotSetEyepiece(int)));
    connect( m_Ui.LensList, SIGNAL( currentTextChanged(const QString) ),
             this, SLOT( slotSetLens(QString) ) );
    connect( m_Ui.FilterList, SIGNAL( currentTextChanged(const QString) ),
             this, SLOT( slotSetFilter(QString) ) );

    connect(m_Ui.removeScopeButton, SIGNAL(clicked()), this, SLOT(slotRemoveScope()));
    connect(m_Ui.removeEyepieceButton, SIGNAL(clicked()), this, SLOT(slotRemoveEyepiece()));
    connect( m_Ui.RemoveLens, SIGNAL( clicked() ), this, SLOT( slotRemoveLens() ) );
    connect( m_Ui.RemoveFilter, SIGNAL( clicked() ), this, SLOT( slotRemoveFilter() ) );

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
    }

    saveEquipment(); //Save the new list.
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
    }

    saveEquipment(); //Save the new list.
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
    while ( m_Ks->data()->logObject()->findLensById( "lens" + '_' + QString::number( nextLens ) ) )
        nextLens++;
    OAL::Lens *l = new OAL::Lens( "lens" + '_' + QString::number( nextLens++ ), m_Ui.l_Model->text(), m_Ui.l_Vendor->text(), m_Ui.l_Factor->value() ) ;
    m_Ks->data()->logObject()->lensList()->append( l );

    m_Ui.l_Id->clear();
    m_Ui.l_Model->clear();
    m_Ui.l_Vendor->clear();
    m_Ui.l_Factor->setValue( 0 );

    saveEquipment(); //Save the new list.
}

void EquipmentWriter::slotRemoveLens()
{
    OAL::Lens *l = m_Ks->data()->logObject()->findLensById( m_Ui.l_Id->text() );
    int idx = m_Ks->data()->logObject()->lensList()->indexOf( l );
    if ( idx < 0 )
        return;
    m_Ks->data()->logObject()->lensList()->removeAt( idx );
    if( l )
        delete l;

    m_Ui.l_Id->clear();
    m_Ui.l_Model->clear();
    m_Ui.l_Vendor->clear();
    m_Ui.l_Factor->setValue( 0 );
    m_Ui.f_Color->setCurrentIndex( 0 );

    QListWidgetItem *item = m_Ui.LensList->takeItem( m_Ui.LensList->currentRow() );
    if( item )
        delete item;

    saveEquipment(); //Save the new list.
}

void EquipmentWriter::slotSaveLens()
{
    OAL::Lens *l = m_Ks->data()->logObject()->findLensByName( m_Ui.l_Id->text() );
    if( l ){
        l->setLens( m_Ui.l_Id->text(), m_Ui.l_Model->text(), m_Ui.l_Vendor->text(), m_Ui.l_Factor->value() );
    }
    saveEquipment(); //Save the new list.
}

void EquipmentWriter::slotSetLens( QString name )
{
    OAL::Lens *l;
    l = m_Ks->data()->logObject()->findLensByName( name );
    if( l ) {
        m_Ui.l_Id->setText( l->id() );
        m_Ui.l_Model->setText( l->model() );
        m_Ui.l_Vendor->setText( l->vendor() );
        m_Ui.l_Factor->setValue( l->factor() );
    }
}

void EquipmentWriter::slotAddFilter()
{
    OAL::Filter *f = new OAL::Filter( "filter" + '_' + QString::number( m_Ks->data()->logObject()->filterList()->size() ),
                                      m_Ui.f_Model->text(), m_Ui.f_Vendor->text(), static_cast<OAL::Filter::FILTER_TYPE>( m_Ui.f_Type->currentIndex() ),
                                      static_cast<OAL::Filter::FILTER_COLOR>( m_Ui.f_Color->currentIndex() ) );
    m_Ks->data()->logObject()->filterList()->append( f );

    m_Ui.f_Id->clear();
    m_Ui.f_Model->clear();
    m_Ui.f_Vendor->clear();
    m_Ui.f_Type->setCurrentIndex( 0 );
    m_Ui.f_Color->setCurrentIndex( 0 );

    m_Ui.FilterList->addItem( f->name() );

    saveEquipment(); //Save the new list.
}

void EquipmentWriter::slotRemoveFilter()
{
    OAL::Filter *f = m_Ks->data()->logObject()->findFilterById( m_Ui.f_Id->text() );
    int idx = m_Ks->data()->logObject()->filterList()->indexOf( f );
    if ( idx < 0 )
        return;
    m_Ks->data()->logObject()->filterList()->removeAt( idx );
    if( f )
        delete f;

    m_Ui.f_Id->clear();
    m_Ui.f_Model->clear();
    m_Ui.f_Vendor->clear();
    m_Ui.f_Type->setCurrentIndex( 0 );
    m_Ui.f_Color->setCurrentIndex( 0 );

    QListWidgetItem *item = m_Ui.FilterList->takeItem( m_Ui.FilterList->currentRow() );
    if( item )
        delete item;

    saveEquipment(); //Save the new list.
}

void EquipmentWriter::slotSaveFilter()
{
    OAL::Filter *f = m_Ks->data()->logObject()->findFilterById( m_Ui.f_Id->text() );
    if( f ){
        f->setFilter( m_Ui.f_Id->text(), m_Ui.f_Model->text(), m_Ui.f_Vendor->text(),
                      static_cast<OAL::Filter::FILTER_TYPE>( m_Ui.f_Type->currentIndex() ), static_cast<OAL::Filter::FILTER_COLOR>( m_Ui.f_Color->currentIndex() ) );

        m_Ui.FilterList->currentItem()->setText( f->name() );
        saveEquipment(); //Save the new list.
    } 
}

void EquipmentWriter::slotSetFilter( QString name )
{
    OAL::Filter *f;
    f = m_Ks->data()->logObject()->findFilterByName( name );
    if( f ) {
        m_Ui.f_Id->setText( f->id() );
        m_Ui.f_Model->setText( f->model() );
        m_Ui.f_Vendor->setText( f->vendor() );
        m_Ui.f_Type->setCurrentIndex( f->type() );
        m_Ui.f_Color->setCurrentIndex( f->color() );
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
    m_Ui.LensList->clear();
    m_Ui.FilterList->clear();

    foreach(Scope *scope, *(m_LogObject->scopeList())) {
        m_Ui.scopeListWidget->addItem(scope->name());
    }
    foreach(Eyepiece *eyepiece, *(m_LogObject->eyepieceList())) {
        m_Ui.eyepieceListWidget->addItem(eyepiece->name());
    }
    foreach(Lens *lens, *(m_LogObject->lensList())) {
        m_Ui.LensList->addItem(lens->name());
    }
    foreach(Filter *filter, *(m_LogObject->filterList())) {
        m_Ui.FilterList->addItem(filter->name());
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
    m_Ui.f_Type->addItems( OAL::Filter::filterTypes() );
    m_Ui.f_Color->addItems( OAL::Filter::filterColors() );
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

#include "equipmentwriter.moc"
