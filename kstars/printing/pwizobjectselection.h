/***************************************************************************
                          pwizobjectselection.h  -  K Desktop Planetarium
                             -------------------
    begin                : Wed Aug 3 2011
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

#ifndef PWIZOBJECTSELECTION_H
#define PWIZOBJECTSELECTION_H

#include "ui_pwizobjectselection.h"

class PrintingWizard;
class SkyObject;

class PWizObjectSelectionUI : public QFrame, public Ui::PWizObjectSelection
{
    Q_OBJECT

public:
    PWizObjectSelectionUI(PrintingWizard *wizard, QWidget *parent = 0);
    void setSkyObject(SkyObject *obj);

private slots:
    void slotSelectFromList();
    void slotPointObject();
    void slotShowDetails();

private:
    QString objectInfoString(SkyObject *obj);

    PrintingWizard *m_ParentWizard;
};

#endif // PWIZOBJECTSELECTION_H
