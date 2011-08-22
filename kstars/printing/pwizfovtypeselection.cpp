/***************************************************************************
                          pwizfovtypeselection.cpp  -  K Desktop Planetarium
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

#include "pwizfovtypeselection.h"

PWizFovTypeSelectionUI::PWizFovTypeSelectionUI(PrintingWizard *wizard, QWidget *parent) : QFrame(parent),
    m_ParentWizard(wizard)
{
    setupUi(this);
}

PrintingWizard::FOV_TYPE PWizFovTypeSelectionUI::getFovExportType()
{
    if(manualRadio->isChecked())
    {
        return PrintingWizard::FT_MANUAL;
    }

    else if(hopperRadio->isChecked())
    {
        return PrintingWizard::FT_STARHOPPER;
    }

    else
    {
        return PrintingWizard::FT_UNDEFINED;
    }
}
