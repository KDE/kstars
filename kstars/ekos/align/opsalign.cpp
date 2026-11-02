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
#include "ekos/auxiliary/rotatorutils.h"

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
        Q_EMIT needToLoadProfile(kcfg_SolveOptionsProfile->currentText());
    });

    reloadOptionsProfiles();

    connect(m_ConfigDialog->button(QDialogButtonBox::Apply), SIGNAL(clicked()), SLOT(slotApply()));
    connect(m_ConfigDialog->button(QDialogButtonBox::Cancel), SIGNAL(clicked()), SLOT(slotApply()));
    connect(m_ConfigDialog->button(QDialogButtonBox::Ok), SIGNAL(clicked()), SLOT(slotApply()));

    // An Alt-Az mount does not flip, so there is no flip policy
    auto showFlipPolicy = [this](bool altAz)
    {
        label_3->setHidden(altAz);
        kcfg_AstrometryFlipRotationAllowed->setHidden(altAz);
        FlipRotationNotAllowed->setHidden(altAz);
    };
    connect(RotatorUtils::Instance(), &RotatorUtils::altAzChanged, this, showFlipPolicy);
    showFlipPolicy(RotatorUtils::Instance()->isAltAz());
}

void OpsAlign::setFlipPolicy(const Ekos::OpsAlign::FlipPriority Priority)
{
    if (Priority == Ekos::OpsAlign::ROTATOR_ANGLE)
        kcfg_AstrometryFlipRotationAllowed->setChecked(true);
    else if (Priority == Ekos::OpsAlign::POSITION_ANGLE)
        FlipRotationNotAllowed->setChecked(true);
    OpsAlign::update();
    Q_EMIT m_ConfigDialog->button(QDialogButtonBox::Apply)->click();
}

void OpsAlign::reloadOptionsProfiles()
{
    const int selectedIndex = Options::solveOptionsProfile();
    optionsList = loadAlignOptionsProfiles();
    // Loading may migrate the saved profiles, and the selected profile with them
    int currentIndex = static_cast<int>(Options::solveOptionsProfile()) != selectedIndex ?
                       static_cast<int>(Options::solveOptionsProfile()) : kcfg_SolveOptionsProfile->currentIndex();
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
    Q_EMIT settingsUpdated();
}
}
