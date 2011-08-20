/***************************************************************************
                          pwizfovsh.h  -  K Desktop Planetarium
                             -------------------
    begin                : Mon Aug 15 2011
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

#ifndef PWIZFOVSH_H
#define PWIZFOVSH_H

#include "ui_pwizfovsh.h"

class PrintingWizard;
class SkyObject;

class PWizFovShUI : public QFrame, public Ui::PWizFovSh
{
    Q_OBJECT
public:
    PWizFovShUI(PrintingWizard *wizard, QWidget *parent = 0);

    double getMaglim() { return maglimSpinBox->value(); }
    QString getFovName() { return fovCombo->currentText(); }
    void setBeginObject(SkyObject *obj);

private slots:
    void slotSelectFromList();
    void slotPointObject();
    void slotBeginCapture();

private:
    void setupWidgets();
    void setupConnections();

    PrintingWizard *m_ParentWizard;
};

#endif // PWIZFOVSH_H
