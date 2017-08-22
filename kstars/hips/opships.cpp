/*  HiPS Options
    Copyright (C) 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include <QPushButton>
#include <QFileDialog>
#include <QCheckBox>
#include <QStringList>
#include <QComboBox>

#include <KConfigDialog>

#include "Options.h"
#include "opships.h"
#include "kstars.h"
#include "auxiliary/ksnotification.h"
#include "auxiliary/filedownloader.h"
#include "skymap.h"

static const QStringList hipsKeys = { "obs_title", "obs_description", "hips_order", "hips_frame", "hips_tile_width", "hips_tile_format", "hips_service_url", "TIMESTAMP"};

OpsHIPS::OpsHIPS() : QFrame(KStars::Instance())
{
    setupUi(this);

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists("hipssettings");

    connect(m_ConfigDialog->button(QDialogButtonBox::Apply), SIGNAL(clicked()), SLOT(slotApply()));
    connect(m_ConfigDialog->button(QDialogButtonBox::Ok), SIGNAL(clicked()), SLOT(slotApply()));

    connect(refreshSourceB, SIGNAL(clicked()), this, SLOT(slotRefresh()));

    if (sourcesList->count() == 0)
        slotRefresh();
}

OpsHIPS::~OpsHIPS()
{
}

void OpsHIPS::slotApply()
{
    SkyMap::Instance()->forceUpdate();
}

void OpsHIPS::slotRefresh()
{
    downloadJob = new FileDownloader();

    downloadJob->setProgressDialogEnabled(true, i18n("HiPS Update"), i18n("Downloading HiPS sources..."));

    QObject::connect(downloadJob, SIGNAL(downloaded()), this, SLOT(downloadReady()));
    QObject::connect(downloadJob, SIGNAL(error(QString)), this, SLOT(downloadError(QString)));

    downloadJob->get(QUrl("http://alasky.unistra.fr/MocServer/query?hips_service_url=*&dataproduct_type=!catalog&dataproduct_type=!cube&&moc_sky_fraction=1&get=record"));
}

void OpsHIPS::downloadReady()
{
    sources.clear();

    QTextStream stream(downloadJob->downloadedData());

    QStringList titles;

    QVariantMap oneSource;
    while (stream.atEnd() == false)
    {
        QString line = stream.readLine();
        if (line.isEmpty())
        {
            sources.append(oneSource);
            oneSource.clear();
            continue;
        }

        QStringList keyvalue = line.split("=", QString::KeepEmptyParts);
        QString key   = keyvalue[0].simplified();
        if (hipsKeys.contains(key) == false)
            continue;
        QString value = keyvalue[1].simplified();
        oneSource[key] = value;
        if (key == "obs_title")
            titles << value;
    }

    // Get existing database sources
    QList<QVariantMap> dbSources;
    KStarsData::Instance()->userdb()->GetAllHIPSSources(dbSources);

    // Get existing database titles
    QStringList dbTitles;
    for (QVariantMap oneSource : dbSources)
        dbTitles << oneSource["title"].toString();

    // Add all titiles to list widget
    sourcesList->addItems(titles);
    QListWidgetItem* item = nullptr;

    // Make sources checkable and check sources that already exist in the database
    for(int i = 0; i < sourcesList->count(); ++i)
    {
        item = sourcesList->item(i);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(dbTitles.contains(item->text()) ? Qt::Checked : Qt::Unchecked);
    }

    // Delete job later
    downloadJob->deleteLater();
}

void OpsHIPS::downloadError(const QString &errorString)
{
    KSNotification::error(i18n("Error downloading HiPS sources: %1", errorString));
    downloadJob->deleteLater();
}

