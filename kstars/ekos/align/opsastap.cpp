/*  ASTAP Options Editor
    Copyright (C) 2019 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "opsastap.h"

#include "align.h"
#include "fov.h"
#include "kstars.h"
#include "ksnotification.h"
#include "Options.h"

#include <KConfigDialog>
#include <QProcess>

namespace Ekos
{
OpsASTAP::OpsASTAP(Align *parent) : QWidget(KStars::Instance())
{
    setupUi(this);

    alignModule = parent;

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists("alignsettings");

    connect(m_ConfigDialog->button(QDialogButtonBox::Apply), SIGNAL(clicked()), SLOT(slotApply()));
    connect(m_ConfigDialog->button(QDialogButtonBox::Ok), SIGNAL(clicked()), SLOT(slotApply()));
    connect(selectASTAPExecB, &QPushButton::clicked, this, &OpsASTAP::slotSelectExecutable);
}

void OpsASTAP::slotApply()
{
    emit settingsUpdated();
}

void OpsASTAP::slotSelectExecutable()
{
    QUrl executable = QFileDialog::getOpenFileUrl(this, i18n("Select ASTAP executable"), QUrl(), "(astap astap.exe)");
    if (executable.isEmpty())
        return;

    kcfg_ASTAPExecutable->setText(executable.toLocalFile());
}

}
