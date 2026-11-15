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
#include "auxiliary/cdsmirrors.h"
#include "kstars_debug.h"

#include <KConfigDialog>

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QPushButton>
#include <QStringList>

// Qt version calming
#include <qtkeepemptyparts.h>

OpsHIPSDisplay::OpsHIPSDisplay() : QFrame(KStars::Instance())
{
    setupUi(this);
}

OpsHIPSCache::OpsHIPSCache() : QFrame(KStars::Instance())
{
    setupUi(this);

    connect(selectDirectoryB, &QPushButton::clicked, this, [this]()
    {
        QString dir = QFileDialog::getExistingDirectory(this, i18nc("@title:window", "HiPS Offline Storage"),
                      kcfg_HIPSOfflinePath->text());

        if (dir.isEmpty())
            return;

        kcfg_HIPSOfflinePath->setText(dir);

        HIPSManager::Instance()->loadOfflineLevels(dir);
        HIPSManager::Instance()->setCurrentSource("DSS Colored");
    });
}

OpsHIPS::OpsHIPS() : QFrame(KStars::Instance())
{
    setupUi(this);

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists("hipssettings");

    QString path = QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath(
                       QLatin1String("hips_previews/"));
    QDir dir;
    dir.mkpath(path);

    connect(refreshSourceB, &QPushButton::clicked, this, &OpsHIPS::slotRefresh);

    connect(sourcesList, &QListWidget::itemChanged, this, &OpsHIPS::slotItemUpdated);
    connect(sourcesList, &QListWidget::itemClicked, this, &OpsHIPS::slotItemClicked);
    connect(filterEdit, &QLineEdit::textChanged, this, &OpsHIPS::slotFilterChanged);

    if (sourcesList->count() == 0)
        slotRefresh();
}

void OpsHIPS::slotRefresh()
{
    // Try the CDS MocServer first, then its mirror
    m_MocServerUrls = CDSMirrors::baseUrls(CDSMirrors::Service::MocServer);
    m_MocServerIndex = 0;
    requestSources();
}

void OpsHIPS::requestSources()
{
    if (m_MocServerIndex >= m_MocServerUrls.count())
        return;

    downloadJob = new FileDownloader();

    downloadJob->setProgressDialogEnabled(true, i18n("HiPS Update"), i18n("Downloading HiPS sources..."));
    // A maintenance page instead of the source list counts as a failure, so that the next server is tried
    downloadJob->registerDataVerification([](const QByteArray & data)
    {
        return data.contains("hips_service_url");
    });

    connect(downloadJob, &FileDownloader::downloaded, this, &OpsHIPS::downloadReady);
    connect(downloadJob, &FileDownloader::error, this, &OpsHIPS::downloadError);

    downloadJob->get(QUrl(m_MocServerUrls.at(m_MocServerIndex) +
                          "?hips_service_url=*&dataproduct_type=!catalog&dataproduct_type=!cube&get=record"));
}

void OpsHIPS::downloadReady()
{
    if (m_MocServerIndex < m_MocServerUrls.count())
        CDSMirrors::reportSuccess(CDSMirrors::Service::MocServer, m_MocServerUrls.at(m_MocServerIndex));

    sources = HIPSManager::parseSourceRecords(downloadJob->downloadedData());

    QStringList hipsTitles;
    for (const auto &oneSource : std::as_const(sources))
    {
        if (oneSource.contains("obs_title"))
            hipsTitles << oneSource.value("obs_title");
    }

    // Get existing database sources
    QList<QMap<QString, QString>> dbSources;
    KStarsData::Instance()->userdb()->GetAllHIPSSources(dbSources);

    // Get existing database titles
    QStringList dbTitles;
    for (QMap<QString, QString> oneSource : dbSources)
        dbTitles << oneSource["obs_title"];

    // Sort titles alphabetically (case-insensitive)
    hipsTitles.sort(Qt::CaseInsensitive);
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
    downloadJob->deleteLater();

    if (++m_MocServerIndex < m_MocServerUrls.count())
    {
        qCWarning(KSTARS) << "Error downloading HiPS sources:" << errorString << "Trying"
                          << m_MocServerUrls.at(m_MocServerIndex);
        requestSources();
        return;
    }

    KSNotification::error(i18n("Error downloading HiPS sources: %1", errorString));
}

void OpsHIPS::slotItemUpdated(QListWidgetItem *item)
{
    for(QMap<QString, QString> &oneSource : sources)
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
    for(QMap<QString, QString> &oneSource : sources)
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

    QString currentPreviewPath = QDir(KSPaths::locate(QStandardPaths::AppLocalDataLocation,
                                      QLatin1String("hips_previews"))).filePath(previewName);
    if (currentPreviewPath.isEmpty() == false)
    {
        sourceImage->setPixmap(QPixmap(currentPreviewPath));
    }
    else
    {
        currentPreviewPath = QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath(
                                 QLatin1String("hips_previews/") + previewName);

        previewJob = new FileDownloader();
        connect(previewJob, &FileDownloader::downloaded, this, &OpsHIPS::previewReady);

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

void OpsHIPS::slotFilterChanged(const QString &text)
{
    // Filter the list based on the search text
    QString filterText = text.trimmed();

    for(int i = 0; i < sourcesList->count(); ++i)
    {
        QListWidgetItem* item = sourcesList->item(i);
        if (item)
        {
            // Show item if filter is empty or if item text contains filter text (case-insensitive)
            bool matches = filterText.isEmpty() || item->text().contains(filterText, Qt::CaseInsensitive);
            item->setHidden(!matches);
        }
    }
}
