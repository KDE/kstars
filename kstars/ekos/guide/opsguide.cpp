/*
    SPDX-FileCopyrightText: 2003 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "opsguide.h"

#include "Options.h"
#include "kstars.h"
#include "auxiliary/ksnotification.h"
#include "internalguide/internalguider.h"
#include "ekos/auxiliary/stellarsolverprofileeditor.h"
#include "kspaths.h"

#include <KConfigDialog>

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QPushButton>
#include <QStringList>

namespace Ekos
{
OpsGuide::OpsGuide() : QFrame(KStars::Instance())
{
    setupUi(this);

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists("guidesettings");

    editGuideProfile->setIcon(QIcon::fromTheme("document-edit"));
    editGuideProfile->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    if (Options::gPGEnabled())
    {
        // this option is an old one. Allowing users who have this set to keep their setting.
        Options::setGPGEnabled(false);
        Options::setRAGuidePulseAlgorithm(GPG_ALGORITHM);
        kcfg_RAGuidePulseAlgorithm->setCurrentIndex(static_cast<int>(GPG_ALGORITHM));
    }


    connect(editGuideProfile, &QAbstractButton::clicked, this, [this]()
    {
        KConfigDialog *optionsEditor = new KConfigDialog(this, "OptionsProfileEditor", Options::self());
        optionsProfileEditor = new StellarSolverProfileEditor(this, Ekos::GuideProfiles, optionsEditor);
#ifdef Q_OS_MACOS
        optionsEditor->setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
        KPageWidgetItem *mainPage = optionsEditor->addPage(optionsProfileEditor, i18n("Guide Options Profile Editor"));
        mainPage->setIcon(QIcon::fromTheme("configure"));
        connect(optionsProfileEditor, &StellarSolverProfileEditor::optionsProfilesUpdated, this, &OpsGuide::loadOptionsProfiles);
        optionsProfileEditor->loadProfile(kcfg_GuideOptionsProfile->currentText());
        optionsEditor->show();
    });

    loadOptionsProfiles();

    connect(m_ConfigDialog, &KConfigDialog::settingsChanged, this, &OpsGuide::settingsUpdated);
}

void OpsGuide::setRAGuidePulseAlg(int index)
{

    switch (index)
    {
        case STANDARD_ALGORITHM:
            kcfg_RAHysteresis->setDisabled(true);
            kcfg_RAIntegralGain->setDisabled(false);
            break;
        case HYSTERESIS_ALGORITHM:
            kcfg_RAHysteresis->setDisabled(false);
            kcfg_RAIntegralGain->setDisabled(true);
            break;
        case LINEAR_ALGORITHM:
            kcfg_RAHysteresis->setDisabled(true);
            kcfg_RAIntegralGain->setDisabled(true);
            break;
        case GPG_ALGORITHM:
            kcfg_RAHysteresis->setDisabled(true);
            kcfg_RAIntegralGain->setDisabled(true);
            break;
        default:
            kcfg_RAHysteresis->setDisabled(false);
            kcfg_RAIntegralGain->setDisabled(false);
            break;
    }
}

void OpsGuide::setDECGuidePulseAlg(int index)
{
    switch (index)
    {
        case STANDARD_ALGORITHM:
            kcfg_DECHysteresis->setDisabled(true);
            kcfg_DECIntegralGain->setDisabled(false);
            break;
        case HYSTERESIS_ALGORITHM:
            kcfg_DECHysteresis->setDisabled(false);
            kcfg_DECIntegralGain->setDisabled(true);
            break;
        case LINEAR_ALGORITHM:
            kcfg_DECHysteresis->setDisabled(true);
            kcfg_DECIntegralGain->setDisabled(true);
            break;
        default:
            kcfg_DECHysteresis->setDisabled(false);
            kcfg_DECIntegralGain->setDisabled(false);
            break;
    }
}

void OpsGuide::loadOptionsProfiles()
{
    QString savedOptionsProfiles = QDir(KSPaths::writableLocation(
                                            QStandardPaths::AppLocalDataLocation)).filePath("SavedGuideProfiles.ini");
    if(QFile(savedOptionsProfiles).exists())
        optionsList = StellarSolver::loadSavedOptionsProfiles(savedOptionsProfiles);
    else
        optionsList = getDefaultGuideOptionsProfiles();
    kcfg_GuideOptionsProfile->clear();
    for(SSolver::Parameters param : optionsList)
        kcfg_GuideOptionsProfile->addItem(param.listName);
    kcfg_GuideOptionsProfile->setCurrentIndex(Options::guideOptionsProfile());
}
}
