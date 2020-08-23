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

    editFocusProfile->setIcon(QIcon::fromTheme("document-edit"));
    editFocusProfile->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    editGuidingProfile->setIcon(QIcon::fromTheme("document-edit"));
    editGuidingProfile->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    editSolverProfile->setIcon(QIcon::fromTheme("document-edit"));
    editSolverProfile->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    StellarSolver temp(SSolver::SOLVE, FITSImage::Statistic(), nullptr, this);
    optionsList = temp.getBuiltInProfiles();
    foreach(SSolver::Parameters param, optionsList)
    {
        focusProfile->addItem(param.listName);
        guidingProfile->addItem(param.listName);
        solverProfile->addItem(param.listName);
    }

    connect(m_ConfigDialog->button(QDialogButtonBox::Apply), SIGNAL(clicked()), SLOT(slotApply()));
    connect(m_ConfigDialog->button(QDialogButtonBox::Ok), SIGNAL(clicked()), SLOT(slotApply()));

}



void OpsAlign::slotApply()
{
    emit settingsUpdated();
}
}
