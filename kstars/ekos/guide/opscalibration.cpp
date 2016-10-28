/*  INDI Options
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

 */

#include <QPushButton>
#include <QFileDialog>
#include <QCheckBox>
#include <QStringList>
#include <QComboBox>

#include <KConfigDialog>

#include "Options.h"
#include "opscalibration.h"
#include "guide.h"
#include "kstars.h"

#include "internalguide/internalguider.h"

namespace Ekos
{

OpsCalibration::OpsCalibration(InternalGuider *guiderObject)  : QFrame( KStars::Instance() )
{
    setupUi(this);

    guider = guiderObject;

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists( "guidesettings" );

    connect( m_ConfigDialog->button(QDialogButtonBox::Apply), SIGNAL( clicked() ), SLOT( slotApply() ) );
    connect( m_ConfigDialog->button(QDialogButtonBox::Ok), SIGNAL( clicked() ), SLOT( slotApply() ) );    
}


OpsCalibration::~OpsCalibration() {}

void OpsCalibration::showEvent(QShowEvent *)
{
   double x, y, ang;
   guider->getReticleParameters(&x, &y, &ang);

   spinBox_ReticleX->setValue(x);
   spinBox_ReticleY->setValue(y);
   spinBox_ReticleAngle->setValue(ang);

   uint16_t subX,subY,subW,subH,subBinX,subBinY;
   guider->getFrameParams(&subX, &subY, &subW, &subH, &subBinX, &subBinY);

   spinBox_ReticleX->setMaximum(subW);
   spinBox_ReticleY->setMaximum(subH);
}

void OpsCalibration::slotApply()
{
    guider->setReticleParameters(spinBox_ReticleX->value(), spinBox_ReticleY->value(), spinBox_ReticleAngle->value());
}

}
