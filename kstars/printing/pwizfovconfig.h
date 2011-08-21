/***************************************************************************
                          pwizfovconfig.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Aug 14 2011
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

#ifndef PWIZFOVCONFIG_H
#define PWIZFOVCONFIG_H

#include "ui_pwizfovconfig.h"
#include "legend.h"

class PWizFovConfigUI : public QFrame, public Ui::PWizFovConfig
{
    Q_OBJECT
public:
    PWizFovConfigUI(QWidget *parent = 0);
    bool isSwitchColorsEnabled() { return switchColorsBox->isChecked(); }
    bool isFovShapeOverriden() { return overrideShapeBox->isChecked(); }
    bool isLegendEnabled() { return addLegendBox->isChecked(); }
    bool isAlphaBlendingEnabled() { return useAlphaBlendBox->isChecked(); }
    Legend::LEGEND_TYPE getLegendType();
    Legend::LEGEND_ORIENTATION getLegendOrientation() { return static_cast<Legend::LEGEND_ORIENTATION>(orientationCombo->currentIndex()); }
    Legend::LEGEND_POSITION getLegendPosition() { return static_cast<Legend::LEGEND_POSITION>(positionCombo->currentIndex()); }

private slots:
    void slotUpdateLegendFields(bool enabled);

private:
    void setupWidgets();
    void setupConnections();
};

#endif // PWIZFOVCONFIG_H
