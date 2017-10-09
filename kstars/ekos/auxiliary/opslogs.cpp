/*  Ekos Logging Options
    Copyright (C) 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "opslogs.h"

#include <QFrame>
#include <QUrl>
#include <QDesktopServices>

#include <KConfigDialog>
#include <KMessageBox>

#include <basedevice.h>

#include "kstars.h"
#include "auxiliary/kspaths.h"
#include "indi/indilistener.h"
#include "Options.h"

namespace Ekos
{
OpsLogs::OpsLogs() : QFrame(KStars::Instance())
{
    setupUi(this);

    refreshInterface();

    //Get a pointer to the KConfigDialog
    KConfigDialog *m_ConfigDialog = KConfigDialog::exists("logssettings");
    connect(m_ConfigDialog->button(QDialogButtonBox::Apply), SIGNAL(clicked()), SLOT(refreshInterface()));
    connect(m_ConfigDialog->button(QDialogButtonBox::Ok), SIGNAL(clicked()), SLOT(refreshInterface()));

    connect(kcfg_VerboseLogging, SIGNAL(toggled(bool)), this, SLOT(slotToggleVerbosityOptions()));

    connect(kcfg_LogToFile, SIGNAL(toggled(bool)), this, SLOT(slotToggleOutputOptions()));

    connect(showLogsB, &QPushButton::clicked, []()
    {
        QDesktopServices::openUrl(QUrl::fromLocalFile(KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "logs"));
    });

    for (auto &b : modulesGroup->buttons())
        b->setEnabled(kcfg_VerboseLogging->isChecked());
    for (auto &b : driversGroup->buttons())
        b->setEnabled(kcfg_VerboseLogging->isChecked());
}

void OpsLogs::slotToggleVerbosityOptions()
{
    if (kcfg_DisableLogging->isChecked())
        KSUtils::Logging::Disable();

    foreach (QAbstractButton *b, modulesGroup->buttons())
    {
        b->setEnabled(kcfg_VerboseLogging->isChecked());
        // If verbose is not checked, CLEAR all selections
        b->setChecked(kcfg_VerboseLogging->isChecked() ? b->isChecked() : false);
    }

    foreach (QAbstractButton *b, driversGroup->buttons())
    {
        b->setEnabled(kcfg_VerboseLogging->isChecked());
        // If verbose is not checked, CLEAR all selections
        b->setChecked(kcfg_VerboseLogging->isChecked() ? b->isChecked() : false);
    }
}

void OpsLogs::slotToggleOutputOptions()
{
    if (kcfg_LogToDefault->isChecked())
    {
        if (kcfg_DisableLogging->isChecked() == false)
            KSUtils::Logging::UseDefault();
    }
    else
        KSUtils::Logging::UseFile();
}

void OpsLogs::refreshInterface()
{
    uint16_t previousInterface = m_INDIDebugInterface;

    m_INDIDebugInterface = 0;

    if (Options::iNDIMountLogging())
        m_INDIDebugInterface |= INDI::BaseDevice::TELESCOPE_INTERFACE;
    if (Options::iNDICCDLogging())
        m_INDIDebugInterface |= INDI::BaseDevice::CCD_INTERFACE;
    if (Options::iNDIFocuserLogging())
        m_INDIDebugInterface |= INDI::BaseDevice::FOCUSER_INTERFACE;
    if (Options::iNDIFilterWheelLogging())
        m_INDIDebugInterface |= INDI::BaseDevice::FILTER_INTERFACE;
    if (Options::iNDIDomeLogging())
        m_INDIDebugInterface |= INDI::BaseDevice::DOME_INTERFACE;
    if (Options::iNDIWeatherLogging())
        m_INDIDebugInterface |= INDI::BaseDevice::WEATHER_INTERFACE;
    if (Options::iNDIDetectorLogging())
        m_INDIDebugInterface |= INDI::BaseDevice::DETECTOR_INTERFACE;
    if (Options::iNDIAuxiliaryLogging())
        m_INDIDebugInterface |= INDI::BaseDevice::AUX_INTERFACE;

    Options::setINDILogging((m_INDIDebugInterface > 0));

    m_SettingsChanged = (previousInterface != m_INDIDebugInterface);
}
}
