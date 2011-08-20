/***************************************************************************
                          pwizfovbrowse.h  -  K Desktop Planetarium
                             -------------------
    begin                : Fri Aug 12 2011
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

#ifndef PWIZFOVBROWSER_H
#define PWIZFOVBROWSER_H

#include "ui_pwizfovbrowse.h"

class PrintingWizard;

class PWizFovBrowseUI : public QFrame, Ui::PWizFovBrowse
{
    Q_OBJECT
public:
    PWizFovBrowseUI(PrintingWizard *wizard, QWidget *parent = 0);

private slots:
    void slotOpenFovEditor();

private:
    PrintingWizard *m_ParentWizard;
};

#endif // PWIZFOVBROWSER_H
