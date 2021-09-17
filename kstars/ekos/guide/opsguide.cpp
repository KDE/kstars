/*
    SPDX-FileCopyrightText: 2003 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "opsguide.h"

#include "Options.h"
#include "kstars.h"
#include "auxiliary/ksnotification.h"
#include "internalguide/internalguider.h"

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

    connect(editGuideProfile, &QAbstractButton::clicked, this, [this]()
    {
        KConfigDialog *optionsEditor = new KConfigDialog(this, "OptionsProfileEditor", Options::self());
        optionsProfileEditor = new StellarSolverProfileEditor(this, Ekos::GuideProfiles, optionsEditor);
#ifdef Q_OS_OSX
        optionsEditor->setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
        KPageWidgetItem *mainPage = optionsEditor->addPage(optionsProfileEditor, i18n("Guide Options Profile Editor"));
        mainPage->setIcon(QIcon::fromTheme("configure"));
        connect(optionsProfileEditor, &StellarSolverProfileEditor::optionsProfilesUpdated, this, &OpsGuide::loadOptionsProfiles);
        optionsProfileEditor->loadProfile(kcfg_GuideOptionsProfile->currentIndex());
        optionsEditor->show();
    });

    loadOptionsProfiles();

    connect(kcfg_GuideOptionsProfile, QOverload<int>::of(&QComboBox::activated), this, [](int index)
    {
        Options::setGuideOptionsProfile(index);
    });

    connect(m_ConfigDialog, SIGNAL(settingsChanged(QString)), this, SIGNAL(settingsUpdated()));

}

void OpsGuide::loadOptionsProfiles()
{
    QString savedOptionsProfiles = QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("SavedGuideProfiles.ini");
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
