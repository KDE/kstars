/***************************************************************************
                          pwizfovtypeselection.h  -  K Desktop Planetarium
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

#ifndef PWIZFOVTYPESELECTION_H
#define PWIZFOVTYPESELECTION_H

#include "ui_pwizfovtypeselection.h"
#include "printingwizard.h"

class PWizFovTypeSelectionUI : public QFrame, public Ui::PWizFovTypeSelection
{
    Q_OBJECT
public:
    PWizFovTypeSelectionUI(PrintingWizard *wizard, QWidget *parent = 0);

    PrintingWizard::FOV_TYPE getFovExportType();

private:
    PrintingWizard *m_ParentWizard;
};

#endif // PWIZFOVTYPESELECTION_H
