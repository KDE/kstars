/***************************************************************************
                          pwiztypeselection.h  -  K Desktop Planetarium
                             -------------------
    begin                : Thu Aug 4 2011
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

#ifndef PWIZTYPESELECTION_H
#define PWIZTYPESELECTION_H

#include "ui_pwiztypeselection.h"
#include "printingwizard.h"

// Printing Wizard printout type selection screen
class PWizTypeSelectionUI : public QFrame, public Ui::PWizTypeSelection
{
    Q_OBJECT
public:
    PWizTypeSelectionUI(QWidget *parent = 0);

    PrintingWizard::PRINTOUT_TYPE getSelectedPrintoutType();
};


#endif // PWIZTYPESELECTION_H
