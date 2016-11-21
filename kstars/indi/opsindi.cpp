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
#include <QDesktopServices>

#include <KConfigDialog>

#include <QCheckBox>
#include <QStringList>
#include <QComboBox>

#include "ksnotification.h"

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

    selectFITSDirB->setIcon( QIcon::fromTheme( "document-open-folder", QIcon(":/icons/breeze/default/document-open-folder.svg")) );
    selectFITSDirB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    selectDriversDirB->setIcon( QIcon::fromTheme( "document-open-folder", QIcon(":/icons/breeze/default/document-open-folder.svg")) );
    selectDriversDirB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    #ifdef Q_OS_OSX
    connect(kcfg_indiServerIsInternal, SIGNAL(clicked()), this, SLOT(toggleINDIInternal()));
    kcfg_indiServerIsInternal->setToolTip(i18n("Internal or External INDI Server?"));
    connect(kcfg_indiDriversAreInternal, SIGNAL(clicked()), this, SLOT(toggleDriversInternal()));
    kcfg_indiDriversAreInternal->setToolTip(i18n("Internal or External INDI Drivers?"));

    if(Options::indiServerIsInternal())
        kcfg_indiServer->setEnabled(false);
    if(Options::indiDriversAreInternal())
        kcfg_indiDriversDir->setEnabled(false);

    #else
    kcfg_indiServerIsInternal->setVisible(false);
    kcfg_indiDriversAreInternal->setVisible(false);
    #endif

    connect(selectFITSDirB, SIGNAL(clicked()), this, SLOT(saveFITSDirectory()));
    connect(selectDriversDirB, SIGNAL(clicked()), this, SLOT(saveDriversDirectory()));    
    connect(showLogsB, SIGNAL(clicked()), this, SLOT(slotShowLogFiles()));
    connect(kcfg_indiServer, SIGNAL(editingFinished()), this, SLOT(verifyINDIServer()));

    #ifdef Q_OS_WIN
    kcfg_indiServer->setEnabled(false);
    #endif
}


OpsINDI::~OpsINDI() {}

void OpsINDI::toggleINDIInternal()
{
    kcfg_indiServer->setEnabled(!kcfg_indiServerIsInternal->isChecked());
    if(kcfg_indiServerIsInternal->isChecked())
        kcfg_indiServer->setText("*Internal INDI Server*");
    else
        #ifdef Q_OS_OSX
        kcfg_indiServer->setText("/usr/local/bin/indiserver");
        #else
        kcfg_indiServer->setText("/usr/bin/indiserver");
        #endif
}

void OpsINDI::toggleDriversInternal()
{
    kcfg_indiDriversDir->setEnabled(!kcfg_indiDriversAreInternal->isChecked());
    if(kcfg_indiDriversAreInternal->isChecked())
        kcfg_indiDriversDir->setText("*Internal INDI Drivers*");
    else
        kcfg_indiDriversDir->setText("/usr/local/bin/");
}

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

void OpsINDI::slotShowLogFiles()
{
    QUrl path = QUrl::fromLocalFile(QDir::homePath() + "/.indi/logs");

    QDesktopServices::openUrl(path);
}

void OpsINDI::verifyINDIServer()
{
    // Do not verify internal
    if (kcfg_indiServerIsInternal->isChecked())
        return;

    QFileInfo indiserver(kcfg_indiServer->text());

    if (indiserver.exists() && indiserver.isFile() && indiserver.baseName() == "indiserver")
        return;

    KSNotification::error(i18n("%1 is not a valid INDI server binary!", kcfg_indiServer->text()));
}

