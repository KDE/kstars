/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "opsfits.h"

#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"

#ifdef HAVE_STELLARSOLVER
#include "kspaths.h"
#include "ekos/auxiliary/stellarsolverprofile.h"
#include <stellarsolver.h>
#endif

OpsFITS::OpsFITS() : QFrame(KStars::Instance())
{
    setupUi(this);

    connect(kcfg_LimitedResourcesMode, &QCheckBox::toggled, this, [this](bool toggled)
    {
        if (toggled)
        {
            kcfg_Auto3DCube->setChecked(false);
            kcfg_AutoDebayer->setChecked(false);
            kcfg_AutoWCS->setChecked(false);
        }
    });
    connect(kcfg_Auto3DCube, &QCheckBox::toggled, this, [this](bool toggled)
    {
        if (toggled)
            kcfg_LimitedResourcesMode->setChecked(false);
    });
    connect(kcfg_AutoDebayer, &QCheckBox::toggled, this, [this](bool toggled)
    {
        if (toggled)
            kcfg_LimitedResourcesMode->setChecked(false);
    });
    connect(kcfg_AutoWCS, &QCheckBox::toggled, this, [this](bool toggled)
    {
        if (toggled)
            kcfg_LimitedResourcesMode->setChecked(false);
    });
#ifdef HAVE_STELLARSOLVER
    setupHFROptions();
#else
    editHfrProfile->setEnabled(false);
    HfrOptionsProfiles->setEnabled(false);
#endif
}

#ifdef HAVE_STELLARSOLVER

void OpsFITS::loadStellarSolverProfiles()
{
    QString savedOptionsProfiles = QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("SavedHFRProfiles.ini");
    if(QFile(savedOptionsProfiles).exists())
        m_StellarSolverProfiles = StellarSolver::loadSavedOptionsProfiles(savedOptionsProfiles);
    else
        m_StellarSolverProfiles = Ekos::getDefaultHFROptionsProfiles();
    HfrOptionsProfiles->clear();
    for(auto param : m_StellarSolverProfiles)
        HfrOptionsProfiles->addItem(param.listName);
    HfrOptionsProfiles->setCurrentIndex(Options::hFROptionsProfile());
}

void OpsFITS::setupHFROptions()
{
    editHfrProfile->setEnabled(true);
    HfrOptionsProfiles->setEnabled(true);

    editHfrProfile->setIcon(QIcon::fromTheme("document-edit"));
    editHfrProfile->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    connect(editHfrProfile, &QAbstractButton::clicked, this, [this]()
    {
        KConfigDialog *optionsEditor = new KConfigDialog(this, "OptionsProfileEditor", Options::self());
        optionsProfileEditor = new Ekos::StellarSolverProfileEditor(this, Ekos::HFRProfiles, optionsEditor);
#ifdef Q_OS_OSX
        optionsEditor->setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
        KPageWidgetItem *mainPage = optionsEditor->addPage(optionsProfileEditor, i18n("HFR Options Profile Editor"));
        mainPage->setIcon(QIcon::fromTheme("configure"));
        connect(optionsProfileEditor, &Ekos::StellarSolverProfileEditor::optionsProfilesUpdated, this,
                &OpsFITS::loadStellarSolverProfiles);
        optionsProfileEditor->loadProfile(HfrOptionsProfiles->currentIndex());
        optionsEditor->show();
    });

    loadStellarSolverProfiles();

    connect(HfrOptionsProfiles, QOverload<int>::of(&QComboBox::activated), this, [](int index)
    {
        Options::setHFROptionsProfile(index);
    });
}

#endif
