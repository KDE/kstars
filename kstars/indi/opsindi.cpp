/*  INDI Options
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
    
 */

#include "opsindi.h"

#include <QPushButton>
#include <QFileDialog>
//#include <KLineEdit>
#include <KConfigDialog>

#include <QCheckBox>
#include <QStringList>
#include <QComboBox>

#include "Options.h"

#include "kstars.h"

OpsINDI::OpsINDI()
        : QFrame(KStars::Instance())
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

    selectFITSDirB->setIcon( QIcon::fromTheme( "document-open-folder" ) );
    selectDriversDirB->setIcon( QIcon::fromTheme( "document-open-folder" ) );

    connect(selectFITSDirB, SIGNAL(clicked()), this, SLOT(saveFITSDirectory()));
    connect(selectDriversDirB, SIGNAL(clicked()), this, SLOT(saveDriversDirectory()));    

    #ifdef Q_OS_WIN
    kcfg_indiServer->setEnabled(false);
    #endif
}


OpsINDI::~OpsINDI() {}

void OpsINDI::saveFITSDirectory()
{
    QString dir = QFileDialog::getExistingDirectory(KStars::Instance(), i18n("FITS Default Directory"), kcfg_fitsDir->text());

    if (!dir.isEmpty())
        kcfg_fitsDir->setText(dir);
}

void OpsINDI::saveDriversDirectory()
{
    QString dir = QFileDialog::getExistingDirectory(KStars::Instance(), i18n("INDI Drivers Directory"), kcfg_indiDriversDir->text());

    if (!dir.isEmpty())
        kcfg_indiDriversDir->setText(dir);
}

