/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "dslrinfodialog.h"

#include <KLocalizedString>
#include "ksnotification.h"

#include "Options.h"

DSLRInfo::DSLRInfo(QWidget *parent, ISD::CCD *ccd) : QDialog(parent)
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif

    setupUi(this);

    currentCCD = ccd;

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(save()));

    DSLRIcon->setPixmap(
        QIcon::fromTheme("camera-photo").pixmap(48, 48));
}

void DSLRInfo::save()
{
    sensorMaxWidth    = maxWidth->value();
    sensorMaxHeight   = maxHeight->value();
    sensorPixelW      = pixelX->value();
    sensorPixelH      = pixelY->value();

    if (sensorMaxWidth == 0 || sensorMaxHeight == 0 || sensorPixelW == 0 || sensorPixelH == 0)
    {
        KSNotification::error(i18n("Invalid values. Please set all values."));
        return;
    }

    ISD::CCDChip *primaryChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);
    primaryChip->setImageInfo(sensorMaxWidth, sensorMaxHeight, sensorPixelW, sensorPixelH, 8);
    primaryChip->setFrame(0, 0, sensorMaxWidth, sensorMaxHeight);

    currentCCD->setConfig(SAVE_CONFIG);

    emit infoChanged();
}
