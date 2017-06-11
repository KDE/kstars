/*  DSLR Info Dialog
    Copyright (C) 2017 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

*/

#include <KMessageBox>
#include <KLocalizedString>

#include "dslrinfodialog.h"
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
        QIcon::fromTheme("camera-photo", QIcon(":/icons/breeze/default/camera-photo.svg")).pixmap(48, 48));
}

void DSLRInfo::save()
{
    int w    = maxWidth->value();
    int h    = maxHeight->value();
    double x = pixelX->value();
    double y = pixelY->value();

    if (w == 0 || h == 0 || x == 0 || y == 0)
    {
        KMessageBox::error(0, i18n("Invalid values. Please set all values."));
        return;
    }

    ISD::CCDChip *primaryChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);
    primaryChip->setImageInfo(w, h, x, y, 8);

    currentCCD->setConfig(SAVE_CONFIG);
}
