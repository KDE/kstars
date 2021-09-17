/*
    SPDX-FileCopyrightText: 2011 Rafał Kułaga <rl.kulaga@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "pwizfovmanual.h"
#include "printingwizard.h"

PWizFovManualUI::PWizFovManualUI(PrintingWizard *wizard, QWidget *parent) : QFrame(parent), m_ParentWizard(wizard)
{
    setupUi(this);

    connect(pushButton, SIGNAL(clicked()), this, SLOT(slotExportFov()));
}

void PWizFovManualUI::slotExportFov()
{
    m_ParentWizard->beginFovCapture();
}
