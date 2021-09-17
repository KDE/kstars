/*
    SPDX-FileCopyrightText: 2004 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "opsadvanced.h"

#include "kspaths.h"
#include "kstars.h"
#include "ksutils.h"
#include "Options.h"
#include "skymap.h"
#include "ksmessagebox.h"
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

    connect(purgeAllConfigB, &QPushButton::clicked, this, &OpsAdvanced::slotPurge);

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
    QUrl path = QUrl::fromLocalFile(QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("logs"));

    QDesktopServices::openUrl(path);
}

void OpsAdvanced::slotApply()
{
    KSUtils::Logging::SyncFilterRules();
}

void OpsAdvanced::slotPurge()
{
    connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this]()
    {
        KSMessageBox::Instance()->disconnect(this);
        QString dbFile = QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("userdb.sqlite");
        QFile::remove(dbFile);
        QString configFile = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)).filePath("kstarsrc");
        QFile::remove(configFile);
        configFile = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)).filePath("kstars.notifyrc");
        QFile::remove(configFile);

        KSMessageBox::Instance()->info(i18n("Purge complete. Please restart KStars."));
    });

    connect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, [this]()
    {
        KSMessageBox::Instance()->disconnect(this);
    });

    KSMessageBox::Instance()->warningContinueCancel(
        i18n("Warning! All KStars configuration is going to be purged. This cannot be reversed."),
        i18n("Clear Configuration"), 30, false);

}
