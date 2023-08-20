/*
    SPDX-FileCopyrightText: 2023 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "opsimageoverlay.h"

#include "ksfilereader.h"
#include "kspaths.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"
#include "skymap.h"
#include "skycomponents/skymapcomposite.h"
#include "skycomponents/imageoverlaycomponent.h"

#include <KConfigDialog>

#include <QPushButton>
#include <QFileDialog>
#include <QPushButton>
#include <QDesktopServices>

OpsImageOverlay::OpsImageOverlay() : QFrame(KStars::Instance())
{
    setupUi(this);

    m_ConfigDialog = KConfigDialog::exists("settings");

    connect(m_ConfigDialog->button(QDialogButtonBox::Apply), SIGNAL(clicked()), SLOT(slotApply()));
    connect(m_ConfigDialog->button(QDialogButtonBox::Ok), SIGNAL(clicked()), SLOT(slotApply()));

    ImageOverlayComponent *overlayComponent = dynamic_cast<ImageOverlayComponent*>(
                KStarsData::Instance()->skyComposite()->imageOverlay());
    connect(solveButton, &QPushButton::clicked, overlayComponent, &ImageOverlayComponent::startSolving,
            Qt::UniqueConnection);
    connect(refreshB, &QPushButton::clicked, overlayComponent, &ImageOverlayComponent::reload,
            Qt::UniqueConnection);
    connect(kcfg_ShowImageOverlays, &QCheckBox::stateChanged, [](int state)
    {
        Options::setShowImageOverlays(state);
    });
    connect(kcfg_ShowSelectedImageOverlay, &QCheckBox::stateChanged, [](int state)
    {
        Options::setShowSelectedImageOverlay(state);
    });
    connect(kcfg_ImageOverlayMaxDimension, QOverload<int>::of(&QSpinBox::valueChanged), [](int value)
    {
        Options::setImageOverlayMaxDimension(value);
    });
    connect(kcfg_ImageOverlayTimeout, QOverload<int>::of(&QSpinBox::valueChanged), [](int value)
    {
        Options::setImageOverlayTimeout(value);
    });
    connect(kcfg_ImageOverlayDefaultScale, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [](double value)
    {
        Options::setImageOverlayDefaultScale(value);
    });
    connect(imageOverlayShowDirButton, &QPushButton::clicked, []()
    {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QDir(KSPaths::writableLocation(
                                      QStandardPaths::AppLocalDataLocation)).filePath("imageOverlays")));
    });

    syncOptions();
}

QTableWidget *OpsImageOverlay::table ()
{
    return imageOverlayTable;
}

QGroupBox *OpsImageOverlay::tableTitleBox()
{
    return ImageOverlayTableBox;
}

QPlainTextEdit *OpsImageOverlay::statusDisplay ()
{
    return imageOverlayStatus;
}

QPushButton *OpsImageOverlay::solvePushButton ()
{
    return solveButton;
}
QComboBox *OpsImageOverlay::solverProfile ()
{
    return imageOverlaySolverProfile;
}

void OpsImageOverlay::syncOptions()
{
    kcfg_ShowImageOverlays->setChecked(Options::showImageOverlays());
    kcfg_ImageOverlayMaxDimension->setValue(Options::imageOverlayMaxDimension());
    kcfg_ImageOverlayTimeout->setValue(Options::imageOverlayTimeout());
    kcfg_ImageOverlayDefaultScale->setValue(Options::imageOverlayDefaultScale());
}

void OpsImageOverlay::slotApply()
{
    KStarsData *data = KStarsData::Instance();
    SkyMap *map      = SkyMap::Instance();

    data->setFullTimeUpdate();
    KStars::Instance()->updateTime();
    map->forceUpdate();
}
