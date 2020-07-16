/*  INDI Options
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

 */

#include "opscalibration.h"

#include "guide.h"
#include "kstars.h"
#include "Options.h"
#include "internalguide/internalguider.h"
#include "internalguide/calibration.h"

#include <QPushButton>
#include <QFileDialog>
#include <QCheckBox>
#include <QStringList>
#include <QComboBox>

#include <KConfigDialog>

namespace Ekos
{
OpsCalibration::OpsCalibration(InternalGuider *guiderObject) : QFrame(KStars::Instance())
{
    setupUi(this);

    guider = guiderObject;

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists("guidesettings");

    connect(m_ConfigDialog->button(QDialogButtonBox::Apply), SIGNAL(clicked()), SLOT(slotApply()));
    connect(m_ConfigDialog->button(QDialogButtonBox::Ok), SIGNAL(clicked()), SLOT(slotApply()));
}

void OpsCalibration::showEvent(QShowEvent *)
{
    double x, y;
    guider->getReticleParameters(&x, &y);

    spinBox_ReticleX->setValue(x);
    spinBox_ReticleY->setValue(y);

    uint16_t subX, subY, subW, subH, subBinX, subBinY;
    guider->getFrameParams(&subX, &subY, &subW, &subH, &subBinX, &subBinY);

    spinBox_ReticleX->setMaximum(subW);
    spinBox_ReticleY->setMaximum(subH);

    auto cal = guider->getCalibration();
    if (cal.isInitialized())
    {
        ra_cal_degrees->setText(QString::number(cal.getRAAngle(), 'f', 1));
        ra_cal_mspp->setText(QString::number(cal.raPulseMillisecondsPerArcsecond(), 'f', 1));
        dec_cal_degrees->setText(QString::number(cal.getDECAngle(), 'f', 1));
        dec_cal_mspp->setText(QString::number(cal.decPulseMillisecondsPerArcsecond(), 'f', 1));
        dec_cal_degrees_unit->setText(
            cal.declinationSwapEnabled() ? "degrees (swapped)" : "degrees");
    }
    else
    {
        ra_cal_degrees->setText("xxxx");
        ra_cal_mspp->setText("xxxx");
        dec_cal_degrees->setText("xxxx");
        dec_cal_degrees_unit->setText("degrees");
        dec_cal_mspp->setText("xxxx");
    }
}

void OpsCalibration::slotApply()
{
    // HY (7/12/20):
    // This can be an issue if the window is opened and then the reticle changes, e.g.
    // if the options window is opened before guiding starts!
    // I've seen a few other random unintended changes of the reticle when setting other params.
    // Commenting it out.
    // guider->setReticleParameters(spinBox_ReticleX->value(), spinBox_ReticleY->value());
}
}
