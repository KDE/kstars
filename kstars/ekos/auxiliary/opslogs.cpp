/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "opslogs.h"

#include "kstars.h"
#include "Options.h"
#include "auxiliary/kspaths.h"
#include "indi/indilistener.h"

#include <KConfigDialog>
#include <KFormat>
#include <KMessageBox>

#include <QFrame>
#include <QUrl>
#include <QDesktopServices>

#include <basedevice.h>

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

    connect(clearLogsB, SIGNAL(clicked()), this, SLOT(slotClearLogs()));
    connect(kcfg_VerboseLogging, SIGNAL(toggled(bool)), this, SLOT(slotToggleVerbosityOptions()));

    connect(kcfg_LogToFile, SIGNAL(toggled(bool)), this, SLOT(slotToggleOutputOptions()));

    connect(showLogsB, &QPushButton::clicked, []()
    {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("logs")));
    });

    for (auto &b : modulesGroup->buttons())
        b->setEnabled(kcfg_VerboseLogging->isChecked());
    for (auto &b : driversGroup->buttons())
        b->setEnabled(kcfg_VerboseLogging->isChecked());

    qint64 totalSize = getDirSize(QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("logs"));
    totalSize += getDirSize(QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("autofocus"));

    clearLogsB->setToolTip(i18n("Clear all logs (%1)", KFormat().formatByteSize(totalSize)));

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
    if (Options::iNDIRotatorLogging())
        m_INDIDebugInterface |= INDI::BaseDevice::ROTATOR_INTERFACE;
    if (Options::iNDIGPSLogging())
        m_INDIDebugInterface |= INDI::BaseDevice::GPS_INTERFACE;
    if (Options::iNDIAOLogging())
        m_INDIDebugInterface |= INDI::BaseDevice::AO_INTERFACE;
    if (Options::iNDIAuxiliaryLogging())
        m_INDIDebugInterface |= INDI::BaseDevice::AUX_INTERFACE;

    Options::setINDILogging((m_INDIDebugInterface > 0));

    m_SettingsChanged = (previousInterface != m_INDIDebugInterface);
}

// Following 2 functions from Stackoverflow #47854288
qint64 OpsLogs::getDirSize(const QString &dirPath)
{
    qint64 size = 0;
    QDir dir(dirPath);

    QDir::Filters fileFilters = QDir::Files | QDir::System | QDir::Hidden;

    for (QString &filePath : dir.entryList(fileFilters))
    {
        QFileInfo fi(dir, filePath);

        size += fi.size();
    }

    QDir::Filters dirFilters = QDir::Dirs | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden;

    for (QString &childDirPath : dir.entryList(dirFilters))
        size += getDirSize(dirPath + QDir::separator() + childDirPath);

    return size;
}

void OpsLogs::slotClearLogs()
{
    if (KMessageBox::questionYesNo(nullptr, i18n("Are you sure you want to delete all logs?")) == KMessageBox::Yes)
    {
        QDir logDir(QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("logs"));
        logDir.removeRecursively();
        logDir.mkpath(".");

        QDir autoFocusDir(QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("autofocus"));
        autoFocusDir.removeRecursively();
        autoFocusDir.mkpath(".");

        clearLogsB->setToolTip(i18n("Clear all logs"));
    }
}

}
