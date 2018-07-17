/*  Astrometry.net Options Editor
    Copyright (C) 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>
    Copyright (C) 2017 Robert Lancaster <rlancaste@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "opsastrometry.h"

#include "align.h"
#include "kstars.h"
#include "Options.h"

#include <KConfigDialog>

namespace Ekos
{
OpsAstrometry::OpsAstrometry(Align *parent) : QWidget(KStars::Instance())
{
    setupUi(this);

    alignModule = parent;

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists("alignsettings");

    dms ra, de;
    ra.setD(Options::astrometryPositionRA());
    de.setD(Options::astrometryPositionDE());

    estRA->setText(ra.toHMSString());
    estDec->setText(de.toDMSString());

    imageWarningLabel->setHidden(kcfg_AstrometryAutoUpdateImageScale->isChecked());
    positionWarningLabel->setHidden(kcfg_AstrometryAutoUpdatePosition->isChecked());

    connect(kcfg_AstrometryAutoUpdateImageScale, SIGNAL(toggled(bool)), imageWarningLabel, SLOT(setHidden(bool)));
    connect(kcfg_AstrometryAutoUpdatePosition, SIGNAL(toggled(bool)), positionWarningLabel, SLOT(setHidden(bool)));

    connect(m_ConfigDialog->button(QDialogButtonBox::Apply), SIGNAL(clicked()), SLOT(slotApply()));
    connect(m_ConfigDialog->button(QDialogButtonBox::Ok), SIGNAL(clicked()), SLOT(slotApply()));

    connect(updateScale, SIGNAL(clicked()), this, SLOT(slotUpdateScale()));
    connect(updatePosition, SIGNAL(clicked()), this, SLOT(slotUpdatePosition()));
    updateScale->setIcon(QIcon::fromTheme("edit-copy"));
    updateScale->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    updatePosition->setIcon(QIcon::fromTheme("edit-copy"));
    updatePosition->setAttribute(Qt::WA_LayoutUsesWidgetRect);
}

void OpsAstrometry::showEvent(QShowEvent *)
{
    dms ra, de;
    ra.setD(Options::astrometryPositionRA());
    de.setD(Options::astrometryPositionDE());

    estRA->setText(ra.toHMSString());
    estDec->setText(de.toDMSString());
}

//This updates the telescope/image field scale in the astrometry options editor to match the currently connected devices.
void OpsAstrometry::slotUpdateScale()
{
    double fov_w, fov_h, fov_pixscale;

    // Values in arcmins. Scale in arcsec per pixel
    alignModule->getCalculatedFOVScale(fov_w, fov_h, fov_pixscale);

    if (fov_w == 0 || fov_h == 0)
    {
        kcfg_AstrometryImageScaleLow->setValue(0);
        kcfg_AstrometryImageScaleHigh->setValue(0);
        return;
    }

    switch (kcfg_AstrometryImageScaleUnits->currentIndex())
    {
        case SCALE_DEGREES:
            fov_w /= 60;
            fov_h /= 60;
            kcfg_AstrometryImageScaleLow->setValue(qMin(fov_w, fov_h));
            kcfg_AstrometryImageScaleHigh->setValue(qMax(fov_w, fov_h));
            break;

        case SCALE_ARCMINUTES:
            kcfg_AstrometryImageScaleLow->setValue(qMin(fov_w, fov_h));
            kcfg_AstrometryImageScaleHigh->setValue(qMax(fov_w, fov_h));
            break;

        case SCALE_ARCSECPERPIX:
            // 10% range
            kcfg_AstrometryImageScaleLow->setValue(fov_pixscale * 0.9);
            kcfg_AstrometryImageScaleHigh->setValue(fov_pixscale * 1.1);
            break;

        default:
            return;
    }
}

//This updates the RA and DEC position in the astrometry options editor to match the current telescope position.
void OpsAstrometry::slotUpdatePosition()
{
    estRA->setText(alignModule->ScopeRAOut->text());
    estDec->setText(alignModule->ScopeDecOut->text());
}

void OpsAstrometry::slotApply()
{
    bool raOK = false, deOK = false;
    dms RA = estRA->createDms(false, &raOK);
    dms DE = estDec->createDms(true, &deOK);

    if (raOK && deOK)
    {
        Options::setAstrometryPositionRA(RA.Degrees());
        Options::setAstrometryPositionDE(DE.Degrees());
    }

    QVariantMap optionsMap;

    if (kcfg_AstrometryUseNoVerify->isChecked())
        optionsMap["noverify"] = true;

    if (kcfg_AstrometryUseResort->isChecked())
        optionsMap["resort"] = true;

    if (kcfg_AstrometryUseNoFITS2FITS->isChecked())
        optionsMap["nofits2fits"] = true;

    if (kcfg_AstrometryUseImageScale->isChecked() && kcfg_AstrometryImageScaleLow->value() > 0 && kcfg_AstrometryImageScaleHigh->value() > 0)
    {
        optionsMap["scaleL"]     = kcfg_AstrometryImageScaleLow->value();
        optionsMap["scaleH"]     = kcfg_AstrometryImageScaleHigh->value();
        optionsMap["scaleUnits"] = kcfg_AstrometryImageScaleUnits->currentText();
    }

    if (kcfg_AstrometryUsePosition->isChecked())
    {
        optionsMap["ra"]     = RA.Degrees();
        optionsMap["de"]     = DE.Degrees();
        optionsMap["radius"] = kcfg_AstrometryRadius->value();
    }

    if (kcfg_AstrometryUseDownsample->isChecked())
        optionsMap["downsample"] = kcfg_AstrometryDownsample->value();

    if (kcfg_AstrometryCustomOptions->text().isEmpty() == false)
        optionsMap["custom"] = kcfg_AstrometryCustomOptions->text();

    QStringList solverArgs = Align::generateOptions(optionsMap);

    QString options = solverArgs.join(" ");
    alignModule->solverOptions->setText(options);
    alignModule->solverOptions->setToolTip(options);
}
}
