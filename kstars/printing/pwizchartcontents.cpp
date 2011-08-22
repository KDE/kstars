/***************************************************************************
                          pwizchartcontents.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Aug 7 2011
    copyright            : (C) 2011 by Rafał Kułaga
    email                : rl.kulaga@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "pwizchartcontents.h"
#include "printingwizard.h"
#include "skyobject.h"

PWizChartContentsUI::PWizChartContentsUI(PrintingWizard *wizard, QWidget *parent) : QFrame(parent),
    m_ParentWizard(wizard)
{
    setupUi(this);

    astComCheckBox->setChecked(false);
    astComCheckBox->setEnabled(false);
}

void PWizChartContentsUI::entered()
{
    if(!m_ParentWizard->getSkyObject())
    {
        return;
    }

    if(m_ParentWizard->getSkyObject()->type() == 9 ||
       m_ParentWizard->getSkyObject()->type() == 10)
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
