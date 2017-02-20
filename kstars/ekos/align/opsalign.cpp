/*  Astrometry.net Options Editor
    Copyright (C) 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>
    Copyright (C) 2017 Robert Lancaster <rlancaste@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include <KConfigDialog>

#include "fov.h"
#include "opsalign.h"
#include "kstars.h"
#include "align.h"
#include "Options.h"

namespace Ekos
{

OpsAlign::OpsAlign(Align *parent)  : QWidget( KStars::Instance() )
{
    setupUi(this);

    alignModule = parent;

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists( "alignsettings" );

    connect( m_ConfigDialog->button(QDialogButtonBox::Apply), SIGNAL( clicked() ), SLOT( slotApply() ) );
    connect( m_ConfigDialog->button(QDialogButtonBox::Ok), SIGNAL( clicked() ), SLOT( slotApply() ) );

#ifdef Q_OS_OSX
connect(kcfg_AstrometrySolverIsInternal, SIGNAL(clicked()), this, SLOT(toggleSolverInternal()));
kcfg_AstrometrySolverIsInternal->setToolTip(i18n("Internal or External Plate Solver?"));
if(Options::astrometrySolverIsInternal())
    kcfg_AstrometrySolverBinary->setEnabled(false);

connect(kcfg_AstrometryConfFileIsInternal, SIGNAL(clicked()), this, SLOT(toggleConfigInternal()));
kcfg_AstrometryConfFileIsInternal->setToolTip(i18n("Internal or External astrometry.cfg?"));
if(Options::astrometryConfFileIsInternal())
    kcfg_AstrometryConfFile->setEnabled(false);

connect(kcfg_AstrometryWCSIsInternal, SIGNAL(clicked()), this, SLOT(toggleWCSInternal()));
kcfg_AstrometryWCSIsInternal->setToolTip(i18n("Internal or External wcsinfo?"));
if(Options::wcsIsInternal())
    kcfg_AstrometryWCSInfo->setEnabled(false);
#else
kcfg_AstrometrySolverIsInternal->setVisible(false);
kcfg_AstrometryConfFileIsInternal->setVisible(false);
kcfg_AstrometryWCSIsInternal->setVisible(false);
#endif
}

OpsAlign::~OpsAlign() {}

void OpsAlign::toggleSolverInternal()
{
    kcfg_AstrometrySolverBinary->setEnabled(!kcfg_AstrometrySolverIsInternal->isChecked());
    if(kcfg_AstrometrySolverIsInternal->isChecked())
        kcfg_AstrometrySolverBinary->setText("*Internal Solver*");
    else
        kcfg_AstrometrySolverBinary->setText("/usr/local/bin/solve-field");
}

void OpsAlign::toggleConfigInternal()
{
    kcfg_AstrometryConfFile->setEnabled(!kcfg_AstrometryConfFileIsInternal->isChecked());
    if(kcfg_AstrometryConfFileIsInternal->isChecked())
        kcfg_AstrometryConfFile->setText("*Internal astrometry.cfg*");
    else
        kcfg_AstrometryConfFile->setText("/etc/astrometry.cfg");
}

void OpsAlign::toggleWCSInternal()
{
    kcfg_AstrometryWCSInfo->setEnabled(!kcfg_AstrometryWCSIsInternal->isChecked());
    if(kcfg_AstrometryWCSIsInternal->isChecked())
        kcfg_AstrometryWCSInfo->setText("*Internal wcsinfo*");
    else
        kcfg_AstrometryWCSInfo->setText("/usr/local/bin/wcsinfo");
}

void OpsAlign::slotApply()
{
    if (alignModule->fov())
            alignModule->fov()->setImageDisplay(kcfg_AstrometrySolverWCS->isChecked());
}

}
