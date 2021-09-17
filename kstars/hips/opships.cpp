/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/


#include "opships.h"

#include "kstars.h"
#include "hipsmanager.h"
#include "Options.h"
#include "skymap.h"
#include "auxiliary/ksnotification.h"
#include "auxiliary/filedownloader.h"
#include "auxiliary/kspaths.h"

#include <KConfigDialog>

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QPushButton>
#include <QStringList>

static const QStringList hipsKeys = { "ID", "obs_title", "obs_description", "hips_order", "hips_frame", "hips_tile_width", "hips_tile_format", "hips_service_url", "moc_sky_fraction"};

OpsHIPSDisplay::OpsHIPSDisplay() : QFrame(KStars::Instance())
{
    setupUi(this);
}

OpsHIPSCache::OpsHIPSCache() : QFrame(KStars::Instance())
{
    setupUi(this);
}

OpsHIPS::OpsHIPS() : QFrame(KStars::Instance())
{
    setupUi(this);

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists("hipssettings");

    QString path = QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(QLatin1String("hips_previews/"));
    QDir dir;
    dir.mkpath(path);    

    connect(refreshSourceB, SIGNAL(clicked()), this, SLOT(slotRefresh()));

    connect(sourcesList, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(slotItemUpdated(QListWidgetItem*)));
    connect(sourcesList, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(slotItemClicked(QListWidgetItem*)));

    if (sourcesList->count() == 0)
        slotRefresh();
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

    QStringList hipsTitles;

    QMap<QString,QString> oneSource;
    while (stream.atEnd() == false)
    {
        QString line = stream.readLine();
        if (line.isEmpty())
        {
            sources.append(oneSource);
            oneSource.clear();
            continue;
        }

        QStringList keyvalue = line.split('=', QString::KeepEmptyParts);
        QString key   = keyvalue[0].simplified();
        if (hipsKeys.contains(key) == false)
            continue;
        QString value = keyvalue[1].simplified();
        oneSource[key] = value;
        if (key == "obs_title")
            hipsTitles << value;
    }

    // Get existing database sources
    QList<QMap<QString,QString>> dbSources;
    KStarsData::Instance()->userdb()->GetAllHIPSSources(dbSources);

    // Get existing database titles
    QStringList dbTitles;
    for (QMap<QString,QString> oneSource : dbSources)
        dbTitles << oneSource["obs_title"];

    // Add all titles to list widget
    sourcesList->addItems(hipsTitles);
    QListWidgetItem* item = nullptr;

    // Make sources checkable and check sources that already exist in the database
    sourcesList->blockSignals(true);
    for(int i = 0; i < sourcesList->count(); ++i)
    {
        item = sourcesList->item(i);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(dbTitles.contains(item->text()) ? Qt::Checked : Qt::Unchecked);

        if (item->text() == Options::hIPSSource())
        {
            item->setSelected(true);
            sourcesList->scrollToItem(item);
            slotItemClicked(item);
        }
    }
    sourcesList->blockSignals(false);

    // Delete job later
    downloadJob->deleteLater();
}

void OpsHIPS::downloadError(const QString &errorString)
{
    KSNotification::error(i18n("Error downloading HiPS sources: %1", errorString));
    downloadJob->deleteLater();
}

void OpsHIPS::slotItemUpdated(QListWidgetItem *item)
{
    for(QMap<QString,QString> &oneSource: sources)
    {
        if (oneSource.value("obs_title") == item->text())
        {
            if (item->checkState() == Qt::Checked)
                KStarsData::Instance()->userdb()->AddHIPSSource(oneSource);
            else
                KStarsData::Instance()->userdb()->DeleteHIPSSource(oneSource.value("ID"));
            break;
        }
    }
}

void OpsHIPS::slotItemClicked(QListWidgetItem *item)
{
    for(QMap<QString,QString> &oneSource: sources)
    {
        if (oneSource.value("obs_title") == item->text())
        {
            sourceDescription->setText(oneSource.value("obs_description"));
            // Get stored preview, if not found, it will be downloaded.
            setPreview(oneSource.value("ID"), oneSource.value("hips_service_url"));
            break;
        }
    }
}

void OpsHIPS::setPreview(const QString &id, const QString &url)
{
    uint hash = qHash(id);
    QString previewName = QString("%1.jpg").arg(hash);

    QString currentPreviewPath = QDir(KSPaths::locate(QStandardPaths::AppDataLocation, QLatin1String("hips_previews"))).filePath(previewName);
    if (currentPreviewPath.isEmpty() == false)
    {
        sourceImage->setPixmap(QPixmap(currentPreviewPath));
    }
    else
    {
        currentPreviewPath = QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(QLatin1String("hips_previews/") + previewName);

        previewJob = new FileDownloader();
        connect(previewJob, SIGNAL(downloaded()), this, SLOT(previewReady()));

        previewJob->setDownloadedFileURL(QUrl::fromLocalFile(currentPreviewPath));

        previewJob->get(QUrl(url + QLatin1String("/preview.jpg")));
    }
}

void OpsHIPS::previewReady()
{
    QString previewFile = previewJob->getDownloadedFileURL().toLocalFile();
    QFileInfo previewInfo(previewFile);
    // If less than 1kb then it's junk
    if (previewInfo.size() < 1024)
    {
        sourceImage->setPixmap(QPixmap(":/images/noimage.png"));
        QFile::remove(previewFile);
    }
    else
        sourceImage->setPixmap(QPixmap(previewFile));
}

