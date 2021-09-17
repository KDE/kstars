/*
    SPDX-FileCopyrightText: 2011 Rafał Kułaga <rl.kulaga@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "pwizfovbrowse.h"

#include <QPointer>
#include "foveditordialog.h"

PWizFovBrowseUI::PWizFovBrowseUI(PrintingWizard *wizard, QWidget *parent) : QFrame(parent), m_ParentWizard(wizard)
{
    setupUi(this);

    connect(browseButton, SIGNAL(clicked()), this, SLOT(slotOpenFovEditor()));
}

void PWizFovBrowseUI::slotOpenFovEditor()
{
    QPointer<FovEditorDialog> dialog(new FovEditorDialog(m_ParentWizard, this));
    dialog->exec();
    delete dialog;
}
