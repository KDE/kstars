/*  INDI Options
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
    
 */

#include "opsekos.h"

#include <QPushButton>
#include <kfiledialog.h>
#include <klineedit.h>
#include <kconfigdialog.h>

#include <QCheckBox>
#include <QStringList>
#include <QComboBox>

#include "Options.h"

#include "kstars.h"
#include "ekosmanager.h"

OpsEkos::OpsEkos( KStars *_ks )
        : QFrame( _ks )
{
    setupUi(this);
    

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists( "settings" );

    connect( m_ConfigDialog, SIGNAL( applyClicked() ), SLOT( slotApply() ) );
    connect( m_ConfigDialog, SIGNAL( okClicked() ), SLOT( slotApply() ) );
    connect( m_ConfigDialog, SIGNAL( cancelClicked() ), SLOT( slotCancel() ) );

}


OpsEkos::~OpsEkos() {}

void OpsEkos::slotApply()
{
    EkosManager *ekosManager = KStars::Instance()->ekosManager();

    if (ekosManager)
        ekosManager->refreshRemoteDrivers();
}

void OpsEkos::slotCancel()
{
  }

#include "opsekos.moc"
