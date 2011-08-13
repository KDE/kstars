/***************************************************************************
                          pwizfovmanual.h  -  K Desktop Planetarium
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

#ifndef PWIZFOVMANUAL_H
#define PWIZFOVMANUAL_H

#include "ui_pwizfovmanual.h"

class PrintingWizard;

class PWizFovManualUI : public QFrame, public Ui::PWizFovManual
{
    Q_OBJECT
public:
    PWizFovManualUI(PrintingWizard *wizard, QWidget *parent = 0);

private slots:
    void slotExportFov();

private:
    PrintingWizard *m_ParentWizard;
};

#endif // PWIZFOVMANUAL_H
