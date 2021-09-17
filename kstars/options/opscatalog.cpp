/*
    SPDX-FileCopyrightText: 2004 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "opscatalog.h"

#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"
#include "skymap.h"
#include "skycomponents/catalogscomponent.h"
#include "skycomponents/skymapcomposite.h"
#include "widgets/magnitudespinbox.h"

#include <KActionCollection>
#include <KConfigDialog>

#include <QList>
#include <QListWidgetItem>
#include <QTextStream>
#include <QFileDialog>

OpsCatalog::OpsCatalog() : QFrame(KStars::Instance())
{
    setupUi(this);

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists("settings");

    //    kcfg_MagLimitDrawStar->setValue( Options::magLimitDrawStar() );
    kcfg_StarDensity->setValue(Options::starDensity());
    //    kcfg_MagLimitDrawStarZoomOut->setValue( Options::magLimitDrawStarZoomOut() );
    //    m_MagLimitDrawStar = kcfg_MagLimitDrawStar->value();
    m_StarDensity = kcfg_StarDensity->value();
    //    m_MagLimitDrawStarZoomOut = kcfg_MagLimitDrawStarZoomOut->value();

    //    kcfg_MagLimitDrawStar->setMinimum( Options::magLimitDrawStarZoomOut() );
    //    kcfg_MagLimitDrawStarZoomOut->setMaximum( 12.0 );

    kcfg_MagLimitDrawDeepSky->setMaximum(16.0);
    kcfg_MagLimitDrawDeepSkyZoomOut->setMaximum(16.0);

    kcfg_DSOCachePercentage->setValue(Options::dSOCachePercentage());
    connect(kcfg_DSOCachePercentage, &QSlider::valueChanged, this,
            [&] { isDirty = true; });

    kcfg_DSOMinZoomFactor->setValue(Options::dSOMinZoomFactor());
    connect(kcfg_DSOMinZoomFactor, &QSlider::valueChanged, this, [&] { isDirty = true; });

    kcfg_ShowUnknownMagObjects->setChecked(Options::showUnknownMagObjects());
    connect(kcfg_ShowUnknownMagObjects, &QCheckBox::stateChanged, this,
            [&] { isDirty = true; });

    //disable star-related widgets if not showing stars
    if (!kcfg_ShowStars->isChecked())
        slotStarWidgets(false);

    /*
    connect( kcfg_MagLimitDrawStar, SIGNAL(valueChanged(double)),
             SLOT(slotSetDrawStarMagnitude(double)) );
    connect( kcfg_MagLimitDrawStarZoomOut, SIGNAL(valueChanged(double)),
             SLOT(slotSetDrawStarZoomOutMagnitude(double)) );
    */
    connect(kcfg_ShowStars, SIGNAL(toggled(bool)), SLOT(slotStarWidgets(bool)));
    connect(kcfg_ShowDeepSky, SIGNAL(toggled(bool)), SLOT(slotDeepSkyWidgets(bool)));
    connect(kcfg_ShowDeepSkyNames, SIGNAL(toggled(bool)), kcfg_DeepSkyLongLabels,
            SLOT(setEnabled(bool)));
    connect(m_ConfigDialog->button(QDialogButtonBox::Apply), SIGNAL(clicked()),
            SLOT(slotApply()));
    connect(m_ConfigDialog->button(QDialogButtonBox::Ok), SIGNAL(clicked()),
            SLOT(slotApply()));
    connect(m_ConfigDialog->button(QDialogButtonBox::Cancel), SIGNAL(clicked()),
            SLOT(slotCancel()));

    connect(manageButton, &QPushButton::clicked, KStars::Instance(),
            &KStars::slotDSOCatalogGUI);

    isDirty = false;
}

void OpsCatalog::slotApply()
{
    if (isDirty == false)
        return;

    isDirty = false;

    Options::setStarDensity(kcfg_StarDensity->value());
    //    Options::setMagLimitDrawStarZoomOut( kcfg_MagLimitDrawStarZoomOut->value() );

    //FIXME: need to add the ShowDeepSky meta-option to the config dialog!
    //For now, I'll set showDeepSky to true if any catalog options changed

    KStars::Instance()->data()->skyComposite()->reloadDeepSky();

    // update time for all objects because they might be not initialized
    // it's needed when using horizontal coordinates
    KStars::Instance()->data()->setFullTimeUpdate();
    KStars::Instance()->updateTime();
    KStars::Instance()->map()->forceUpdate();

    Options::setDSOCachePercentage(kcfg_DSOCachePercentage->value());
    KStars::Instance()->data()->skyComposite()->catalogsComponent()->resizeCache(
        kcfg_DSOCachePercentage->value());

    Options::setDSOMinZoomFactor(kcfg_DSOMinZoomFactor->value());
    Options::setShowUnknownMagObjects(kcfg_ShowUnknownMagObjects->isChecked());
}

void OpsCatalog::slotCancel()
{
    //Revert all local option placeholders to the values in the global config object
    //    m_MagLimitDrawStar = Options::magLimitDrawStar();
    m_StarDensity = Options::starDensity();
    //    m_MagLimitDrawStarZoomOut = Options::magLimitDrawStarZoomOut();
}

void OpsCatalog::slotStarWidgets(bool on)
{
    //    LabelMagStars->setEnabled(on);
    LabelStarDensity->setEnabled(on);
    //    LabelMagStarsZoomOut->setEnabled(on);
    LabelDensity->setEnabled(on);
    //    LabelMag1->setEnabled(on);
    //    LabelMag2->setEnabled(on);
    //    kcfg_MagLimitDrawStar->setEnabled(on);
    kcfg_StarDensity->setEnabled(on);
    LabelStarDensity->setEnabled(on);
    //    kcfg_MagLimitDrawStarZoomOut->setEnabled(on);
    kcfg_StarLabelDensity->setEnabled(on);
    kcfg_ShowStarNames->setEnabled(on);
    kcfg_ShowStarMagnitudes->setEnabled(on);
}

void OpsCatalog::slotDeepSkyWidgets(bool on)
{
    LabelMagDeepSky->setEnabled(on);
    LabelMagDeepSkyZoomOut->setEnabled(on);
    kcfg_MagLimitDrawDeepSky->setEnabled(on);
    kcfg_MagLimitDrawDeepSkyZoomOut->setEnabled(on);
    kcfg_ShowDeepSkyNames->setEnabled(on);
    kcfg_ShowDeepSkyMagnitudes->setEnabled(on);
    kcfg_DSOCachePercentage->setEnabled(on);
    DSOCacheLabel->setEnabled(on);
    kcfg_DSOMinZoomFactor->setEnabled(on);
    kcfg_ShowUnknownMagObjects->setEnabled(on);
    DSOMInZoomLabel->setEnabled(on);
    DeepSkyLabelDensityLabel->setEnabled(on);
    kcfg_DeepSkyLabelDensity->setEnabled(on);
    kcfg_DeepSkyLongLabels->setEnabled(on);
    LabelMag3->setEnabled(on);
    LabelMag4->setEnabled(on);
}
