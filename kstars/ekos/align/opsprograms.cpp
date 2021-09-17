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

    connect(kcfg_AstrometryConfFileIsInternal, SIGNAL(clicked()), this, SLOT(toggleConfigInternal()));
    kcfg_AstrometryConfFileIsInternal->setToolTip(i18n("Internal or External astrometry.cfg?"));
    if (Options::astrometryConfFileIsInternal())
        kcfg_AstrometryConfFile->setEnabled(false);

#ifdef Q_OS_OSX
    connect(kcfg_AstrometrySolverIsInternal, SIGNAL(clicked()), this, SLOT(toggleSolverInternal()));
    kcfg_AstrometrySolverIsInternal->setToolTip(i18n("Internal or External Plate Solver?"));
    if (Options::astrometrySolverIsInternal())
        kcfg_AstrometrySolverBinary->setEnabled(false);

    connect(kcfg_AstrometryWCSIsInternal, SIGNAL(clicked()), this, SLOT(toggleWCSInternal()));
    kcfg_AstrometryWCSIsInternal->setToolTip(i18n("Internal or External wcsinfo?"));
    if (Options::astrometryWCSIsInternal())
        kcfg_AstrometryWCSInfo->setEnabled(false);

    connect(kcfg_SextractorIsInternal, SIGNAL(clicked()), this, SLOT(toggleSextractorInternal()));
    kcfg_SextractorIsInternal->setToolTip(i18n("Internal or External sextractor?"));
    if (Options::sextractorIsInternal())
        kcfg_SextractorBinary->setEnabled(false);

#else
    kcfg_AstrometrySolverIsInternal->setVisible(false);
    kcfg_AstrometryWCSIsInternal->setVisible(false);
    kcfg_SextractorIsInternal->setVisible(false);
#endif

}

void OpsPrograms::toggleSolverInternal()
{
    kcfg_AstrometrySolverBinary->setEnabled(!kcfg_AstrometrySolverIsInternal->isChecked());
    if (kcfg_AstrometrySolverIsInternal->isChecked())
        kcfg_AstrometrySolverBinary->setText("*Internal Solver*");
    else
        kcfg_AstrometrySolverBinary->setText(KSUtils::getDefaultPath("AstrometrySolverBinary"));
}

void OpsPrograms::toggleConfigInternal()
{
    kcfg_AstrometryConfFile->setEnabled(!kcfg_AstrometryConfFileIsInternal->isChecked());
    if (kcfg_AstrometryConfFileIsInternal->isChecked())
        kcfg_AstrometryConfFile->setText("*Internal astrometry.cfg*");
    else
        kcfg_AstrometryConfFile->setText(KSUtils::getDefaultPath("AstrometryConfFile"));
}

void OpsPrograms::toggleWCSInternal()
{
    kcfg_AstrometryWCSInfo->setEnabled(!kcfg_AstrometryWCSIsInternal->isChecked());
    if (kcfg_AstrometryWCSIsInternal->isChecked())
        kcfg_AstrometryWCSInfo->setText("*Internal wcsinfo*");
    else
        kcfg_AstrometryWCSInfo->setText(KSUtils::getDefaultPath("AstrometryWCSInfo"));
}

void OpsPrograms::toggleSextractorInternal()
{
    kcfg_SextractorBinary->setEnabled(!kcfg_SextractorIsInternal->isChecked());
    if (kcfg_SextractorIsInternal->isChecked())
        kcfg_SextractorBinary->setText("*Internal Sextractor*");
    else
        kcfg_SextractorBinary->setText(KSUtils::getDefaultPath("SextractorBinary"));
}

void OpsPrograms::slotApply()
{
    emit settingsUpdated();
}

void OpsPrograms::loadDefaultPaths(int option)
{
    ExternalProgramPaths paths;

    switch(option)
    {
        case 0:
            return;
            break;
        case 1:
            paths = StellarSolver::getLinuxDefaultPaths();
            break;
        case 2:
            paths = StellarSolver::getLinuxInternalPaths();
            break;
        case 3:
            paths = StellarSolver::getMacHomebrewPaths();
            break;
        case 4:
            paths = StellarSolver::getMacInternalPaths();
            break;
        case 5:
            paths = StellarSolver::getWinANSVRPaths();
            break;
        case 6:
            paths = StellarSolver::getWinCygwinPaths();
            break;
        default:
            paths = StellarSolver::getLinuxDefaultPaths();
            break;
    }

    if( ! kcfg_SextractorIsInternal->isChecked())
        kcfg_SextractorBinary->setText(paths.sextractorBinaryPath);
    if( ! kcfg_AstrometryConfFileIsInternal->isChecked())
        kcfg_AstrometryConfFile->setText(paths.confPath);
    if( ! kcfg_AstrometrySolverIsInternal->isChecked())
        kcfg_AstrometrySolverBinary->setText(paths.solverPath);
    kcfg_ASTAPExecutable->setText(paths.astapBinaryPath);
    if( ! kcfg_AstrometryWCSIsInternal->isChecked())
        kcfg_AstrometryWCSInfo->setText(paths.wcsPath);

    defaultPathSelector->setCurrentIndex(0);
}
}
