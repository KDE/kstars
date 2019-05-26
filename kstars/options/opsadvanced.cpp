/***************************************************************************
                          opsadvanced.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun 14 Mar 2004
    copyright            : (C) 2004 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "opsadvanced.h"

#include "kspaths.h"
#include "kstars.h"
#include "ksutils.h"
#include "Options.h"
#include "skymap.h"
#include "widgets/timespinbox.h"

#include <KConfigDialog>

#include <QCheckBox>
#include <QDesktopServices>
#include <QLabel>
#include <QRadioButton>

OpsAdvanced::OpsAdvanced() : QFrame(KStars::Instance())
{
    setupUi(this);

    //Initialize the timestep value
    SlewTimeScale->tsbox()->changeScale(Options::slewTimeScale());

    connect(SlewTimeScale, SIGNAL(scaleChanged(float)), this, SLOT(slotChangeTimeScale(float)));

    connect(kcfg_HideOnSlew, SIGNAL(clicked()), this, SLOT(slotToggleHideOptions()));

    connect(kcfg_VerboseLogging, SIGNAL(toggled(bool)), this, SLOT(slotToggleVerbosityOptions()));

    connect(kcfg_LogToFile, SIGNAL(toggled(bool)), this, SLOT(slotToggleOutputOptions()));

    connect(showLogsB, SIGNAL(clicked()), this, SLOT(slotShowLogFiles()));

    connect(kcfg_ObsListDemoteHole, &QCheckBox::toggled,
            [this](bool state)
    {
        kcfg_ObsListHoleSize->setEnabled(state);
    });

    connect(zoomScrollSlider, &QSlider::valueChanged, [&](int value)
    {
        kcfg_ZoomScrollFactor->setValue(value / 100.0);
    });

    connect(kcfg_DefaultCursor, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), [](int index)
    {
        Options::setDefaultCursor(index);
        SkyMap::Instance()->setMouseCursorShape(static_cast<SkyMap::Cursor>(index));
    });

    //Get a pointer to the KConfigDialog
    KConfigDialog *m_ConfigDialog = KConfigDialog::exists("settings");
    connect(m_ConfigDialog->button(QDialogButtonBox::Apply), SIGNAL(clicked()), SLOT(slotApply()));
    connect(m_ConfigDialog->button(QDialogButtonBox::Ok), SIGNAL(clicked()), SLOT(slotApply()));
}

void OpsAdvanced::slotChangeTimeScale(float newScale)
{
    Options::setSlewTimeScale(newScale);
}

void OpsAdvanced::slotToggleHideOptions()
{
    textLabelHideTimeStep->setEnabled(kcfg_HideOnSlew->isChecked());
    SlewTimeScale->setEnabled(kcfg_HideOnSlew->isChecked());
    HideBox->setEnabled(kcfg_HideOnSlew->isChecked());
}

void OpsAdvanced::slotToggleVerbosityOptions()
{
    if (kcfg_DisableLogging->isChecked())
        KSUtils::Logging::Disable();
}

void OpsAdvanced::slotToggleOutputOptions()
{
    if (kcfg_LogToDefault->isChecked())
    {
        if (kcfg_DisableLogging->isChecked() == false)
            KSUtils::Logging::UseDefault();
    }
    else
        KSUtils::Logging::UseFile();
}

void OpsAdvanced::slotShowLogFiles()
{
    QUrl path = QUrl::fromLocalFile(KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "logs");

    QDesktopServices::openUrl(path);
}

void OpsAdvanced::slotApply()
{
    KSUtils::Logging::SyncFilterRules();
}
