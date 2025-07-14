/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2017 Robert Lancaster <rlancaste@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "opsprograms.h"

#include "align.h"
#include "fov.h"
#include "kstars.h"
#include "ksnotification.h"
#include "Options.h"

#include <stellarsolver.h>
#undef Const
#include <KConfigDialog>
#include <QProcess>

namespace Ekos
{
OpsPrograms::OpsPrograms(Align *parent) : QWidget(KStars::Instance())
{
    setupUi(this);

    alignModule = parent;

    connect(defaultPathSelector, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &OpsPrograms::loadDefaultPaths);

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists("alignsettings");

    connect(m_ConfigDialog->button(QDialogButtonBox::Apply), SIGNAL(clicked()), SLOT(slotApply()));
    connect(m_ConfigDialog->button(QDialogButtonBox::Ok), SIGNAL(clicked()), SLOT(slotApply()));

}

void OpsPrograms::slotApply()
{
    emit settingsUpdated();
}

void OpsPrograms::loadDefaultPaths(int option)
{
    if(option == 0)
        return;
    ExternalProgramPaths paths = StellarSolver::getDefaultExternalPaths((SSolver::ComputerSystemType) (option - 1));

    kcfg_SextractorBinary->setText(paths.sextractorBinaryPath);
    kcfg_AstrometryConfFile->setText(paths.confPath);
    kcfg_AstrometrySolverBinary->setText(paths.solverPath);
    kcfg_ASTAPExecutable->setText(paths.astapBinaryPath);
    kcfg_WatneyBinary->setText(paths.watneyBinaryPath);
    kcfg_AstrometryWCSInfo->setText(paths.wcsPath);

    defaultPathSelector->setCurrentIndex(0);
}
}
