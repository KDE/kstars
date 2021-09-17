/*
    SPDX-FileCopyrightText: 2019 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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
    QUrl executable = QFileDialog::getOpenFileUrl(this, i18nc("@title:window", "Select ASTAP executable"), QUrl(), "(astap astap.exe)");
    if (executable.isEmpty())
        return;

    kcfg_ASTAPExecutable->setText(executable.toLocalFile());
}

}
