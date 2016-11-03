/*  INDI Options
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

 */

#include <QPushButton>
#include <QFileDialog>
#include <QCheckBox>
#include <QStringList>
#include <QComboBox>

#include <KConfigDialog>

#include "opsekos.h"
#include "Options.h"
#include "kstarsdata.h"
#include "ekosmanager.h"
#include "guide/guide.h"
#include "fov.h"

OpsEkos::OpsEkos()
        : QTabWidget( KStars::Instance() )
{
    setupUi(this);

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists( "settings" );

#ifdef Q_OS_OSX
connect(solverInternal, SIGNAL(clicked()), this, SLOT(toggleSolverInternal()));
solverInternal->setToolTip(i18n("Internal or External Plate Solver."));
if(Options::astrometrySolverIsInternal()){
    solverInternal->setChecked(true);
    kcfg_astrometrySolver->setEnabled(false);
}

connect(configInternal, SIGNAL(clicked()), this, SLOT(toggleConfigInternal()));
configInternal->setToolTip(i18n("Internal or External Astrometry.cfg."));
if(Options::astrometryConfFileIsInternal()){
    configInternal->setChecked(true);
    kcfg_astrometryConfFile->setEnabled(false);
}

connect(wcsInternal, SIGNAL(clicked()), this, SLOT(toggleWCSInternal()));
wcsInternal->setToolTip(i18n("Internal or External WCS Info."));
if(Options::wcsIsInternal()){
    wcsInternal->setChecked(true);
    kcfg_astrometryWCSInfo->setEnabled(false);
}
#else
solverInternal->setVisible(false);
configInternal->setVisible(false);
wcsInternal->setVisible(false);
#endif

    connect( m_ConfigDialog->button(QDialogButtonBox::Apply), SIGNAL( clicked() ), SLOT( slotApply() ) );
    connect( m_ConfigDialog->button(QDialogButtonBox::Ok), SIGNAL( clicked() ), SLOT( slotApply() ) );
    connect( m_ConfigDialog->button(QDialogButtonBox::Cancel), SIGNAL( clicked() ), SLOT( slotCancel() ) );
}


OpsEkos::~OpsEkos() {}

void OpsEkos::toggleSolverInternal()
{
    kcfg_astrometrySolver->setEnabled(!solverInternal->isChecked());
    if(solverInternal->isChecked())
        kcfg_astrometrySolver->setText("*Internal Solver*");
    else
        kcfg_astrometrySolver->setText("/usr/local/bin/solve-field");
}

void OpsEkos::toggleConfigInternal()
{
    kcfg_astrometryConfFile->setEnabled(!configInternal->isChecked());
    if(configInternal->isChecked())
        kcfg_astrometryConfFile->setText("*Internal astrometry.cfg*");
    else
        kcfg_astrometryConfFile->setText("/etc/astrometry.cfg");
}

void OpsEkos::toggleWCSInternal()
{
    kcfg_astrometryWCSInfo->setEnabled(!wcsInternal->isChecked());
    if(wcsInternal->isChecked())
        kcfg_astrometryWCSInfo->setText("*Internal wcsinfo*");
    else
        kcfg_astrometryWCSInfo->setText("/usr/local/bin/wcsinfo");
}

void OpsEkos::slotApply()
{
    EkosManager *ekosManager = KStars::Instance()->ekosManager();

    if (ekosManager)
    {
        Ekos::Align *alignModule = ekosManager->alignModule();

        if (alignModule && alignModule->fov())
            alignModule->fov()->setImageDisplay(kcfg_SolverWCS->isChecked());
    }
}

