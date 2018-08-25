/*  DSLR Info Dialog
    Copyright (C) 2017 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

*/

#include "dslrinfodialog.h"

#include <KMessageBox>
#include <KLocalizedString>

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
        KMessageBox::error(0, i18n("Invalid values. Please set all values."));
        return;
    }

    ISD::CCDChip *primaryChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);
    primaryChip->setImageInfo(sensorMaxWidth, sensorMaxHeight, sensorPixelW, sensorPixelH, 8);
    primaryChip->setFrame(sensorMaxWidth, sensorMaxHeight, sensorPixelW, sensorPixelH);

    currentCCD->setConfig(SAVE_CONFIG);
}
