/*  INDI Options
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
    
 */

#include "opsindi.h"

#include <kpushbutton.h>
#include <kfiledialog.h>
#include <klineedit.h>
#include <kconfigdialog.h>

#include <QCheckBox>
#include <QStringList>
#include <QComboBox>

#include "Options.h"

#include "kstars.h"

OpsINDI::OpsINDI( KStars *_ks )
        : QFrame( _ks )
{
    setupUi(this);
    
    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists( "settings" );

    if (Options::fitsDir().isEmpty())
    {
        kcfg_fitsDir->setText (QDir:: homePath());
        Options::setFitsDir( kcfg_fitsDir->text());
    }
    else
        kcfg_fitsDir->setText ( Options::fitsDir());

    if (Options::filterAlias().empty())
        m_filterList << "1" << "2" << "3" << "4" << "5" << "6" << "7" << "8" << "9" << "10";
    else
        m_filterList = Options::filterAlias();

    filterSlotCombo->setCurrentIndex(0);
    kcfg_filterAlias->setText(m_filterList[0]);

    selectFITSDirB->setIcon( KIcon( "document-open" ) );
    selectDriversDirB->setIcon( KIcon( "document-open" ) );

    connect(selectFITSDirB, SIGNAL(clicked()), this, SLOT(saveFITSDirectory()));
    connect(selectDriversDirB, SIGNAL(clicked()), this, SLOT(saveDriversDirectory()));
    connect(kcfg_filterAlias, SIGNAL(editingFinished()), this, SLOT(saveFilterAlias()));
    connect(filterSlotCombo, SIGNAL(activated(int)), this, SLOT(updateFilterAlias(int)));

    connect( m_ConfigDialog, SIGNAL( applyClicked() ), SLOT( slotApply() ) );
    connect( m_ConfigDialog, SIGNAL( okClicked() ), SLOT( slotApply() ) );
    connect( m_ConfigDialog, SIGNAL( cancelClicked() ), SLOT( slotCancel() ) );
}


OpsINDI::~OpsINDI() {}

void OpsINDI::saveFITSDirectory()
{
    QString dir = KFileDialog::getExistingDirectory(kcfg_fitsDir->text());

    if (!dir.isEmpty())
        kcfg_fitsDir->setText(dir);
}

void OpsINDI::saveDriversDirectory()
{
    QString dir = KFileDialog::getExistingDirectory(kcfg_indiDriversDir->text());

    if (!dir.isEmpty())
        kcfg_indiDriversDir->setText(dir);
}

void OpsINDI::saveFilterAlias()
{
    m_filterList[filterSlotCombo->currentIndex()] = kcfg_filterAlias->text();

    //Options::setfilterAlias(m_filterList);
}

void OpsINDI::updateFilterAlias(int index)
{
    if (index < 0) return;

    kcfg_filterAlias->setText(m_filterList[index]);
}

void OpsINDI::slotApply()
{
  Options::setFilterAlias(m_filterList);
}

void OpsINDI::slotCancel()
{
  m_filterList = Options::filterAlias();
}

#include "opsindi.moc"
