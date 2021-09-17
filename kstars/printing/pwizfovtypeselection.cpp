/*
    SPDX-FileCopyrightText: 2011 Rafał Kułaga <rl.kulaga@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "pwizfovtypeselection.h"

PWizFovTypeSelectionUI::PWizFovTypeSelectionUI(PrintingWizard *wizard, QWidget *parent)
    : QFrame(parent), m_ParentWizard(wizard)
{
    setupUi(this);
}

PrintingWizard::FOV_TYPE PWizFovTypeSelectionUI::getFovExportType()
{
    if (manualRadio->isChecked())
    {
        return PrintingWizard::FT_MANUAL;
    }

    else if (hopperRadio->isChecked())
    {
        return PrintingWizard::FT_STARHOPPER;
    }

    else
    {
        return PrintingWizard::FT_UNDEFINED;
    }
}
