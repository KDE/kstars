/*
    SPDX-FileCopyrightText: 2011 Rafał Kułaga <rl.kulaga@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "pwizfovconfig.h"
#include <KLocalizedString>

PWizFovConfigUI::PWizFovConfigUI(QWidget *parent) : QFrame(parent)
{
    setupUi(this);
    setupWidgets();

    connect(addLegendBox, SIGNAL(toggled(bool)), this, SLOT(slotUpdateLegendFields(bool)));
}

Legend::LEGEND_TYPE PWizFovConfigUI::getLegendType()
{
    switch (typeCombo->currentIndex())
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
    types << i18n("Scale with magnitudes chart") << i18n("Only scale") << i18n("Only magnitudes chart");
    typeCombo->addItems(types);

    orientationCombo->addItem(i18n("Horizontal"));
    orientationCombo->addItem(i18n("Vertical"));

    QStringList positions;
    positions << i18n("Upper left corner") << i18n("Upper right corner") << i18n("Lower left corner")
              << i18n("Lower right corner");
    positionCombo->addItems(positions);

    useAlphaBlendBox->setEnabled(false);
    typeCombo->setEnabled(false);
    orientationCombo->setEnabled(false);
    positionCombo->setEnabled(false);
}
