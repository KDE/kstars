/***************************************************************************
                          pwizfovmanual.cpp  -  K Desktop Planetarium
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

#include "pwizfovmanual.h"
#include "printingwizard.h"

PWizFovManualUI::PWizFovManualUI(PrintingWizard *wizard, QWidget *parent) : QFrame(parent),
    m_ParentWizard(wizard)
{
    setupUi(this);

    connect(pushButton, SIGNAL(clicked()), this, SLOT(slotExportFov()));
}

void PWizFovManualUI::slotExportFov()
{
    m_ParentWizard->beginFovCapture();
}
