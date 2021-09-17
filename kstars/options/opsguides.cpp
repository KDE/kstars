/*
    SPDX-FileCopyrightText: 2005 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "opsguides.h"

#include "ksfilereader.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"
#include "skymap.h"
#include "skycomponents/skymapcomposite.h"
#include "auxiliary/kspaths.h"

#include <KConfigDialog>

#include <QPushButton>
#include <QFileDialog>

OpsGuides::OpsGuides() : QFrame(KStars::Instance())
{
    setupUi(this);

    foreach (const QString &item, KStarsData::Instance()->skyComposite()->getCultureNames())
        kcfg_SkyCulture->addItem(i18nc("Sky Culture", item.toUtf8().constData()));

    m_ConfigDialog = KConfigDialog::exists("settings");

    connect(m_ConfigDialog->button(QDialogButtonBox::Apply), SIGNAL(clicked()), SLOT(slotApply()));
    connect(m_ConfigDialog->button(QDialogButtonBox::Ok), SIGNAL(clicked()), SLOT(slotApply()));

    // When setting up the widget, update the enabled status of the
    // checkboxes depending on the options.
    slotToggleOpaqueGround(Options::showGround());
    slotToggleConstellOptions(Options::showCNames());
    slotToggleConstellationArt(Options::showConstellationArt());
    slotToggleMilkyWayOptions(Options::showMilkyWay());
    slotToggleAutoSelectGrid(Options::autoSelectGrid());

    connect(kcfg_ShowCNames, SIGNAL(toggled(bool)), this, SLOT(slotToggleConstellOptions(bool)));
    connect(kcfg_ShowConstellationArt, SIGNAL(toggled(bool)), this, SLOT(slotToggleConstellationArt(bool)));
    connect(kcfg_ShowMilkyWay, SIGNAL(toggled(bool)), this, SLOT(slotToggleMilkyWayOptions(bool)));
    connect(kcfg_ShowGround, SIGNAL(toggled(bool)), this, SLOT(slotToggleOpaqueGround(bool)));
    connect(kcfg_AutoSelectGrid, SIGNAL(toggled(bool)), this, SLOT(slotToggleAutoSelectGrid(bool)));

    // Track changes to apply settings
    connect(constellationButtonGroup, static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonPressed), this,
            [&]()
    {
        isDirty = true;
    });
    connect(nameButtonGroup, static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonPressed), this,
            [&]()
    {
        isDirty = true;
    });
    connect(kcfg_SkyCulture, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this,
            [&]()
    {
        isDirty = true;
    });

    isDirty = false;

}

void OpsGuides::slotToggleConstellOptions(bool state)
{
    ConstellOptions->setEnabled(state);
}

void OpsGuides::slotToggleConstellationArt(bool state)
{
    kcfg_ShowConstellationArt->setEnabled(state);
}

void OpsGuides::slotToggleMilkyWayOptions(bool state)
{
    kcfg_FillMilkyWay->setEnabled(state);
}

void OpsGuides::slotToggleOpaqueGround(bool state)
{
    kcfg_ShowHorizon->setEnabled(!state);
}

void OpsGuides::slotToggleAutoSelectGrid(bool state)
{
    kcfg_ShowEquatorialGrid->setEnabled(!state);
    kcfg_ShowHorizontalGrid->setEnabled(!state);
}

void OpsGuides::slotApply()
{
    if (isDirty == false)
        return;

    isDirty = false;

    KStarsData *data = KStarsData::Instance();
    SkyMap *map      = SkyMap::Instance();

    // If the focus object was a constellation and the sky culture has changed, remove the focus object
    if (map->focusObject() && map->focusObject()->type() == SkyObject::CONSTELLATION)
    {
        if (data->skyComposite()->currentCulture() !=
                data->skyComposite()->getCultureName(kcfg_SkyCulture->currentIndex()) ||
                data->skyComposite()->isLocalCNames() != Options::useLocalConstellNames())
        {
            map->setClickedObject(nullptr);
            map->setFocusObject(nullptr);
        }
    }

    data->skyComposite()->setCurrentCulture(
        KStarsData::Instance()->skyComposite()->getCultureName(kcfg_SkyCulture->currentIndex()));
    data->skyComposite()->reloadCLines();
    data->skyComposite()->reloadCNames();
    data->skyComposite()->reloadConstellationArt();

    data->setFullTimeUpdate();
    KStars::Instance()->updateTime();
    map->forceUpdate();
}
