/*  Astrometry.net Options Editor
    Copyright (C) 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>
    Copyright (C) 2017 Robert Lancaster <rlancaste@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "opsalign.h"

#include "align.h"
#include "fov.h"
#include "kstars.h"
#include "ksnotification.h"
#include "Options.h"
#include "kspaths.h"
#include "ekos/auxiliary/stellarsolverprofile.h"

#include <KConfigDialog>
#include <QProcess>

namespace Ekos
{
OpsAlign::OpsAlign(Align *parent) : QWidget(KStars::Instance())
{
    setupUi(this);

    alignModule = parent;

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists("alignsettings");

    editSolverProfile->setIcon(QIcon::fromTheme("document-edit"));
    editSolverProfile->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    connect(editSolverProfile, &QAbstractButton::clicked, this, [this]
    {
        emit needToLoadProfile(kcfg_SolveOptionsProfile->currentIndex());
    });

    reloadOptionsProfiles();

    connect(m_ConfigDialog->button(QDialogButtonBox::Apply), SIGNAL(clicked()), SLOT(slotApply()));
    connect(m_ConfigDialog->button(QDialogButtonBox::Ok), SIGNAL(clicked()), SLOT(slotApply()));
}

void OpsAlign::reloadOptionsProfiles()
{
    QString savedOptionsProfiles = QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("SavedAlignProfiles.ini");

    if(QFile(savedOptionsProfiles).exists())
        optionsList = StellarSolver::loadSavedOptionsProfiles(savedOptionsProfiles);
    else
        optionsList = getDefaultAlignOptionsProfiles();
    kcfg_SolveOptionsProfile->clear();
    foreach(SSolver::Parameters param, optionsList)
        kcfg_SolveOptionsProfile->addItem(param.listName);
    kcfg_SolveOptionsProfile->setCurrentIndex(Options::solveOptionsProfile());
}

void OpsAlign::slotApply()
{
    emit settingsUpdated();
}
}
