/*
    SPDX-FileCopyrightText: 2003 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "opsindi.h"

#include "ksnotification.h"
#include "kstars.h"
#include "Options.h"

#include <KConfigDialog>

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QPushButton>
#include <QStringList>

OpsINDI::OpsINDI() : QFrame(KStars::Instance())
{
    setupUi(this);

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists("settings");

    selectFITSDirB->setIcon(
        QIcon::fromTheme("document-open-folder"));
    selectFITSDirB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    selectDriversDirB->setIcon(
        QIcon::fromTheme("document-open-folder"));
    selectDriversDirB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    selectINDIHubB->setIcon(
        QIcon::fromTheme("document-open"));
    selectINDIHubB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

#ifdef Q_OS_OSX
    connect(kcfg_indiServerIsInternal, SIGNAL(clicked()), this, SLOT(toggleINDIInternal()));
    kcfg_indiServerIsInternal->setToolTip(i18n("Internal or external INDI server?"));
    connect(kcfg_indiDriversAreInternal, SIGNAL(clicked()), this, SLOT(toggleDriversInternal()));
    kcfg_indiDriversAreInternal->setToolTip(i18n("Internal or external INDI drivers?"));

    if (Options::indiServerIsInternal())
        kcfg_indiServer->setEnabled(false);
    if (Options::indiDriversAreInternal())
        kcfg_indiDriversDir->setEnabled(false);

#else
    kcfg_indiServerIsInternal->setVisible(false);
    kcfg_indiDriversAreInternal->setVisible(false);
#endif

    connect(selectFITSDirB, SIGNAL(clicked()), this, SLOT(saveFITSDirectory()));
    connect(selectDriversDirB, SIGNAL(clicked()), this, SLOT(saveDriversDirectory()));
    connect(showLogsB, SIGNAL(clicked()), this, SLOT(slotShowLogFiles()));
    connect(kcfg_indiServer, SIGNAL(editingFinished()), this, SLOT(verifyINDIServer()));
    connect(selectINDIHubB, &QPushButton::clicked, this, &OpsINDI::saveINDIHubAgent);

#ifdef Q_OS_WIN
    kcfg_indiServer->setEnabled(false);
#endif
}

void OpsINDI::toggleINDIInternal()
{
    kcfg_indiServer->setEnabled(!kcfg_indiServerIsInternal->isChecked());
    if (kcfg_indiServerIsInternal->isChecked())
        kcfg_indiServer->setText("*Internal INDI Server*");
    else
        kcfg_indiServer->setText(KSUtils::getDefaultPath("indiServer"));
}

void OpsINDI::toggleDriversInternal()
{
    kcfg_indiDriversDir->setEnabled(!kcfg_indiDriversAreInternal->isChecked());
    if (kcfg_indiDriversAreInternal->isChecked())
        kcfg_indiDriversDir->setText("*Internal INDI Drivers*");
    else
        kcfg_indiDriversDir->setText(KSUtils::getDefaultPath("indiDriversDir"));

    KSNotification::info(i18n("You need to restart KStars for this change to take effect."));
}

void OpsINDI::saveFITSDirectory()
{
    QString dir =
        QFileDialog::getExistingDirectory(KStars::Instance(), i18nc("@title:window", "FITS Default Directory"), kcfg_fitsDir->text());

    if (!dir.isEmpty())
        kcfg_fitsDir->setText(dir);
}

void OpsINDI::saveDriversDirectory()
{
    QString dir = QFileDialog::getExistingDirectory(KStars::Instance(), i18nc("@title:window", "INDI Drivers Directory"),
                  kcfg_indiDriversDir->text());

    if (!dir.isEmpty())
    {
        kcfg_indiDriversDir->setText(dir);
        KSNotification::info(i18n("You need to restart KStars for this change to take effect."));
    }
}

void OpsINDI::saveINDIHubAgent()
{
    QUrl url = QFileDialog::getOpenFileUrl(this, i18nc("@title:window", "Select INDIHub Agent"), QUrl(),  "indihub-agent");
    if (url.isEmpty())
        return;

    kcfg_INDIHubAgent->setText(url.toLocalFile());
}

void OpsINDI::slotShowLogFiles()
{
    QUrl path = QUrl::fromLocalFile(QDir::homePath() + "/.indi/logs");

    QDesktopServices::openUrl(path);
}

void OpsINDI::verifyINDIServer()
{
    // Do not verify internal
    if (kcfg_indiServerIsInternal->isChecked())
        return;

    QFileInfo indiserver(kcfg_indiServer->text());

    if (indiserver.exists() && indiserver.isFile() && indiserver.baseName() == "indiserver")
        return;

    KSNotification::error(i18n("%1 is not a valid INDI server binary.", kcfg_indiServer->text()));
}
