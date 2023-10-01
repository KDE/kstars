/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2017 Robert Lancaster <rlancaste@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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
#include <ekos_align_debug.h>

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
        emit needToLoadProfile(kcfg_SolveOptionsProfile->currentText());
    });

    reloadOptionsProfiles();   

    connect(m_ConfigDialog->button(QDialogButtonBox::Apply), SIGNAL(clicked()), SLOT(slotApply()));
    connect(m_ConfigDialog->button(QDialogButtonBox::Cancel), SIGNAL(clicked()), SLOT(slotApply()));
    connect(m_ConfigDialog->button(QDialogButtonBox::Ok), SIGNAL(clicked()), SLOT(slotApply()));
}

void OpsAlign::setFlipPolicy(const Ekos::OpsAlign::FlipPriority Priority)
{
    if (Priority == Ekos::OpsAlign::ROTATOR_ANGLE)
        kcfg_AstrometryFlipRotationAllowed->setChecked(true);
    else if (Priority == Ekos::OpsAlign::POSITION_ANGLE)
        FlipRotationNotAllowed->setChecked(true);
    OpsAlign::update();
    emit m_ConfigDialog->button(QDialogButtonBox::Apply)->click();
}

void OpsAlign::reloadOptionsProfiles()
{
    QString savedOptionsProfiles = QDir(KSPaths::writableLocation(
                                            QStandardPaths::AppLocalDataLocation)).filePath("SavedAlignProfiles.ini");

    if(QFile(savedOptionsProfiles).exists())
        optionsList = StellarSolver::loadSavedOptionsProfiles(savedOptionsProfiles);
    else
        optionsList = getDefaultAlignOptionsProfiles();
    int currentIndex = kcfg_SolveOptionsProfile->currentIndex();
    kcfg_SolveOptionsProfile->clear();
    for(auto &param : optionsList)
        kcfg_SolveOptionsProfile->addItem(param.listName);

    if (currentIndex >= 0)
    {
        kcfg_SolveOptionsProfile->setCurrentIndex(currentIndex);
        Options::setSolveOptionsProfile(currentIndex);
    }
    else
        kcfg_SolveOptionsProfile->setCurrentIndex(Options::solveOptionsProfile());
}

void OpsAlign::slotApply()
{
    emit settingsUpdated();
}
}
