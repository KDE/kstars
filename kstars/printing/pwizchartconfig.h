/***************************************************************************
                          pwizchartconfig.h  -  K Desktop Planetarium
                             -------------------
    begin                : Wed Aug 10 2011
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

#ifndef PWIZCHARTCONFIG_H
#define PWIZCHARTCONFIG_H

#include "ui_pwizchartconfig.h"

class PrintingWizard;

class PWizChartConfigUI : public QFrame, public Ui::PWizChartConfig
{
    Q_OBJECT
public:
    PWizChartConfigUI(PrintingWizard *wizard, QWidget *parent = 0);

    QString getChartTitle();
    QString getChartSubtitle();
    QString getChartDescription();

private:
    PrintingWizard *m_ParentWizard;
};

#endif // PWIZCHARTCONFIG_H
