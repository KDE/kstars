/***************************************************************************
                          pwizchartcontents.h  -  K Desktop Planetarium
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

#ifndef PWIZCHARTCONTENTS_H
#define PWIZCHARTCONTENTS_H

#include "ui_pwizchartcontents.h"

class PrintingWizard;

class PWizChartContentsUI : public QFrame, public Ui::PWizChartContents
{
    Q_OBJECT
public:
    PWizChartContentsUI(PrintingWizard *wizard, QWidget *parent = 0);

    void entered();

    bool isGeneralTableChecked();
    bool isPositionTableChecked();
    bool isRSTTableChecked();
    bool isAstComTableChecked();
    bool isLoggingFormChecked();

private:
    PrintingWizard *m_ParentWizard;
};

#endif // PWIZCHARTCONTENTS_H
