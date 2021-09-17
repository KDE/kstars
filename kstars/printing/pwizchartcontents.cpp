/*
    SPDX-FileCopyrightText: 2011 Rafał Kułaga <rl.kulaga@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "pwizchartcontents.h"
#include "printingwizard.h"
#include "skyobject.h"

PWizChartContentsUI::PWizChartContentsUI(PrintingWizard *wizard, QWidget *parent)
    : QFrame(parent), m_ParentWizard(wizard)
{
    setupUi(this);

    astComCheckBox->setChecked(false);
    astComCheckBox->setEnabled(false);
}

void PWizChartContentsUI::entered()
{
    if (!m_ParentWizard->getSkyObject())
    {
        return;
    }

    if (m_ParentWizard->getSkyObject()->type() == 9 || m_ParentWizard->getSkyObject()->type() == 10)
    {
        astComCheckBox->setChecked(true);
        astComCheckBox->setEnabled(true);
    }
}

bool PWizChartContentsUI::isGeneralTableChecked()
{
    return generalCheckBox->isChecked();
}

bool PWizChartContentsUI::isPositionTableChecked()
{
    return posCheckBox->isChecked();
}

bool PWizChartContentsUI::isRSTTableChecked()
{
    return rstCheckBox->isChecked();
}

bool PWizChartContentsUI::isAstComTableChecked()
{
    return astComCheckBox->isChecked();
}

bool PWizChartContentsUI::isLoggingFormChecked()
{
    return loggingFormBox->isChecked();
}
