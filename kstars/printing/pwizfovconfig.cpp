/***************************************************************************
                          pwizfovconfig.cpp  -  K Desktop Planetarium
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

#include "pwizfovconfig.h"

PWizFovConfigUI::PWizFovConfigUI(QWidget *parent) : QFrame(parent)
{
    setupUi(this);
    setupWidgets();

    connect(addLegendBox, SIGNAL(toggled(bool)), this, SLOT(slotUpdateLegendFields(bool)));
}

Legend::LEGEND_TYPE PWizFovConfigUI::getLegendType()
{
    switch(typeCombo->currentIndex())
    {
    case 0: // Scale with magnitudes chart
        {
            return Legend::LT_SCALE_MAGNITUDES;
        }

    case 1: // Only scale
        {
            return Legend::LT_SCALE_ONLY;
        }

    case 2: // Only magnitudes chart
        {
            return Legend::LT_MAGNITUDES_ONLY;
        }

    default:
        {
            return Legend::LT_FULL;
        }
    }
}

void PWizFovConfigUI::slotUpdateLegendFields(bool enabled)
{
    useAlphaBlendBox->setEnabled(enabled);
    typeCombo->setEnabled(enabled);
    orientationCombo->setEnabled(enabled);
    positionCombo->setEnabled(enabled);
}

void PWizFovConfigUI::setupWidgets()
{
    QStringList types;
    types << xi18n("Scale with magnitudes chart") << xi18n("Only scale") << xi18n("Only magnitudes chart");
    typeCombo->addItems(types);

    orientationCombo->addItem(xi18n("Horizontal"));
    orientationCombo->addItem(xi18n("Vertical"));

    QStringList positions;
    positions << xi18n("Upper left corner") << xi18n("Upper right corner") << xi18n("Lower left corner")
            << xi18n("Lower right corner");
    positionCombo->addItems(positions);

    useAlphaBlendBox->setEnabled(false);
    typeCombo->setEnabled(false);
    orientationCombo->setEnabled(false);
    positionCombo->setEnabled(false);
}


