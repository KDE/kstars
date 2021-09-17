/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2017 Robert Lancaster <rlancaste@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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
        case SSolver::DEG_WIDTH:
            fov_w /= 60;
            fov_h /= 60;
            kcfg_AstrometryImageScaleLow->setValue(qMin(fov_w, fov_h));
            kcfg_AstrometryImageScaleHigh->setValue(qMax(fov_w, fov_h));
            break;

        case SSolver::ARCMIN_WIDTH:
            kcfg_AstrometryImageScaleLow->setValue(qMin(fov_w, fov_h));
            kcfg_AstrometryImageScaleHigh->setValue(qMax(fov_w, fov_h));
            break;

        case SSolver::ARCSEC_PER_PIX:
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
    //if (Options::solverBackend() != 1)
    //    return;

    bool raOK = false, deOK = false;
    dms RA = estRA->createDms(false, &raOK);
    dms DE = estDec->createDms(true, &deOK);

    if (raOK && deOK)
    {
        Options::setAstrometryPositionRA(RA.Degrees());
        Options::setAstrometryPositionDE(DE.Degrees());
    }

    //alignModule->generateArgs();
}
}
