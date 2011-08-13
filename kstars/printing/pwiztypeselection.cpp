/***************************************************************************
                          pwiztypeselection.cpp  -  K Desktop Planetarium
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

#include "pwiztypeselection.h"

PWizTypeSelectionUI::PWizTypeSelectionUI(QWidget *parent) : QFrame(parent)
{
    setupUi(this);
}

PrintingWizard::PRINTOUT_TYPE PWizTypeSelectionUI::getSelectedPrintoutType()
{
    if(finderChartRadio->isChecked())
    {
        return PrintingWizard::PT_FINDER_CHART;
    }

    else if(loggingFormRadio->isChecked())
    {
        return PrintingWizard::PT_LOGGING_FORM;
    }

    else
    {
        return PrintingWizard::PT_UNDEFINED;
    }

}


