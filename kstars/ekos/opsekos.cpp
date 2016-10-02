/*  INDI Options
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

 */

#include <QPushButton>
#include <QFileDialog>
#include <QCheckBox>
#include <QStringList>
#include <QComboBox>

#include <KConfigDialog>

#include "opsekos.h"
#include "Options.h"
#include "kstarsdata.h"
#include "ekosmanager.h"
#include "guide.h"
#include "fov.h"

OpsEkos::OpsEkos()
        : QTabWidget( KStars::Instance() )
{
    setupUi(this);

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists( "settings" );

    selectPHD2B->setIcon(QIcon::fromTheme("document-open", QIcon(":/icons/breeze/default/document-open.svg")));

    connect( m_ConfigDialog->button(QDialogButtonBox::Apply), SIGNAL( clicked() ), SLOT( slotApply() ) );
    connect( m_ConfigDialog->button(QDialogButtonBox::Ok), SIGNAL( clicked() ), SLOT( slotApply() ) );
    connect( m_ConfigDialog->button(QDialogButtonBox::Cancel), SIGNAL( clicked() ), SLOT( slotCancel() ) );
    connect( selectPHD2B, SIGNAL(clicked()), this, SLOT(slotSelectPHD2Exec()));
    connect( kcfg_UseEkosGuider, SIGNAL(toggled(bool)), this, SLOT(slotCheckGuideModule()));

}


OpsEkos::~OpsEkos() {}

void OpsEkos::slotApply()
{
    EkosManager *ekosManager = KStars::Instance()->ekosManager();

    if (ekosManager)
    {
        //ekosManager->refreshRemoteDrivers();

        Ekos::Guide *guideModule = ekosManager->guideModule();

        if (guideModule)
            guideModule->setGuiderProcess(kcfg_UseEkosGuider->isChecked() ? Ekos::Guide::GUIDE_INTERNAL : Ekos::Guide::GUIDE_PHD2);

        Ekos::Align *alignModule = ekosManager->alignModule();

        if (alignModule && alignModule->fov())
            alignModule->fov()->setImageDisplay(kcfg_SolverWCS->isChecked());
    }
}

void OpsEkos::slotCancel()
{
}

void OpsEkos::slotSelectPHD2Exec()
{
    QUrl phd2URL = QFileDialog::getOpenFileUrl(this, i18n("Select PHD2 Executable"));
    if (phd2URL.isEmpty())
        return;

    kcfg_PHD2Exec->setText(phd2URL.toLocalFile());
}

void OpsEkos::slotCheckGuideModule()
{
    EkosManager *ekosManager = KStars::Instance()->ekosManager();

    if (ekosManager)
    {
        Ekos::Guide *guideModule = ekosManager->guideModule();

        if (guideModule == NULL)
            return;

        if (guideModule->isCalibrating() || guideModule->isGuiding())
        {
           kcfg_UseEkosGuider->disconnect();
           kcfg_UseEkosGuider->setChecked(!kcfg_UseEkosGuider->isChecked());
           connect( kcfg_UseEkosGuider, SIGNAL(toggled(bool)), this, SLOT(slotCheckGuideModule()));

           KMessageBox::error(KStars::Instance(), i18n("Cannot change guider while guiding process is busy."));
        }
    }
}
