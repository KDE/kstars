/***************************************************************************
                          wiusersettings.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2012/09/07
    copyright            : (C) 2012 by Samikshan Bairagya
    email                : samikshan@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef WI_USER_SETTINGS
#define WI_USER_SETTINGS

class WIView;

#include "obsconditions.h"
#include <QWizard>
#include "ui_wiusersettings.h"

class WIUserSettings : public QWizard , public Ui::WIUserSettingsUI
{
    Q_OBJECT
public:
    WIUserSettings(QWidget* parent = 0, Qt::WindowFlags flags = 0);
    ~WIUserSettings();

public slots:
    void slotFinished(int);
    void slotSetBortleClass(int);
    void slotSetAperture(double);
    void slotTelescopeCheck(bool);
    void slotBinocularsCheck(bool);
    void slotNoEquipCheck(bool);
    void slotSetEqType(QString);

private:
    void makeConnections();
    WIView *wi;
    ObsConditions::Equipment eq;
    ObsConditions::EquipmentType type;
};

#endif