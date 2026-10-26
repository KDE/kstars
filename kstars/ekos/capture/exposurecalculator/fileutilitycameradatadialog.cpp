/*
    SPDX-FileCopyrightText: 2023 Joseph McGee <joseph.mcgee@sbcglobal.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QLoggingCategory>
#include <QDir>
#include <QFile>
#include "fileutilitycameradata.h"
#include "fileutilitycameradatadialog.h"
#include "./ui_fileutilitycameradatadialog.h"
#include <ekos_capture_debug.h>
#include <KLocalizedString>

FileUtilityCameraDataDialog::FileUtilityCameraDataDialog(
    QWidget *parent,
    const QString &aPreferredCameraId) :
    QDialog(parent),
    ui(new Ui::FileUtilityCameraDataDialog),
    aPreferredCameraId(aPreferredCameraId)
{
    ui->setupUi(this);

    OptimalExposure::FileUtilityCameraData::downloadRepositoryCameraDataFileList(this);

    connect(ui->downloadB, &QPushButton::clicked, this, &FileUtilityCameraDataDialog::startCameraDownload);
    connect(ui->availableRemoteCameraList, &QListWidget::itemSelectionChanged, this,
            &FileUtilityCameraDataDialog::updateDownloadButton);

    // ui->buttonBox->
    // ui->buttonBox->Cancel

    ui->availableRemoteCameraList->clear();
    ui->availableRemoteCameraList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    updateDownloadButton();

}

void FileUtilityCameraDataDialog::updateDownloadButton()
{
    // Nothing to download while downloads are running or no camera is selected
    ui->downloadB->setEnabled(downloadFileCounter == 0 && !ui->availableRemoteCameraList->selectedItems().isEmpty());
}

FileUtilityCameraDataDialog::~FileUtilityCameraDataDialog()
{
    delete ui;
}

void FileUtilityCameraDataDialog::refreshCameraList()
{
    // ui needs a list of remote files that can be selected
    // This setup depends upon the downloadRepositoryCameraDataFileList

    ui->availableRemoteCameraList->clear();
    ui->availableRemoteCameraList->setSelectionMode(QAbstractItemView::ExtendedSelection);

    int updateCount = 0;
    for (const auto &availableCameraFile : availableCameraDataFiles)
    {
        // Make the camera list user friendly... to match the camera device id
        QString availableCameraId = OptimalExposure::FileUtilityCameraData::cameraDataFileNameToCameraId(
                                        availableCameraFile.fileName);

        // Compare the installed file with the repository to show what can be downloaded or updated
        bool installed = false, updateAvailable = false;
        QFile installedFile(OptimalExposure::FileUtilityCameraData::installedCameraDataFile(availableCameraId));
        if (installedFile.fileName().length() > 0 && installedFile.open(QIODevice::ReadOnly))
        {
            installed = true;
            updateAvailable = OptimalExposure::FileUtilityCameraData::gitBlobSha(installedFile.readAll())
                              != availableCameraFile.sha;
        }

        QString text = availableCameraId;
        if (updateAvailable)
            text = i18nc("camera name, followed by its status", "%1 (update available)", availableCameraId);
        else if (installed)
            text = i18nc("camera name, followed by its status", "%1 (installed)", availableCameraId);

        auto *item = new QListWidgetItem(text, ui->availableRemoteCameraList);
        item->setData(Qt::UserRole, availableCameraId);

        // A camera whose data is up to date has nothing to download
        if (installed && !updateAvailable)
            item->setFlags(item->flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled));

        // Preselect updates, and the active camera unless its data is up to date
        if (updateAvailable)
        {
            item->setSelected(true);
            updateCount++;
        }
        else if (!installed && availableCameraId == aPreferredCameraId)
            item->setSelected(true);
    }

    if (updateCount > 0)
        ui->counter->setText(i18np("An update is available for 1 installed camera.",
                                   "Updates are available for %1 installed cameras.", updateCount));

    QList<QListWidgetItem *> selected = ui->availableRemoteCameraList->selectedItems();
    if (!selected.isEmpty())
        ui->availableRemoteCameraList->scrollToItem(selected.first());
}

void FileUtilityCameraDataDialog::showListError(const QString &error)
{
    ui->counter->setText(i18n("The list of cameras could not be downloaded: %1", error));
}


void FileUtilityCameraDataDialog::startCameraDownload()
{
    QList<QListWidgetItem *> selectedItems =  ui->availableRemoteCameraList->selectedItems();
    if (selectedItems.size() > 0)
    {
        failedDownloads.clear();
        downloadFileCounter = selectedItems.size();
        updateDownloadButton();
        ui->counter->setText(i18np("Downloading 1 camera...", "Downloading %1 cameras...", downloadFileCounter));
        for (auto aSelectedCameraItem : selectedItems)
        {
            QString aSelectedCameraId = aSelectedCameraItem->data(Qt::UserRole).toString();
            OptimalExposure::FileUtilityCameraData::downloadCameraDataFile(aSelectedCameraId, this);
        }
    }
}

int FileUtilityCameraDataDialog::getDownloadFileCounter() const
{
    return downloadFileCounter;
}

void FileUtilityCameraDataDialog::downloadFinished(const QString &cameraId, bool success)
{
    if (!success)
        failedDownloads << cameraId;

    downloadFileCounter--;
    qCInfo(KSTARS_EKOS_CAPTURE) << "Camera Data download file counter decremented "
                                << QString::number(downloadFileCounter);
    if (downloadFileCounter > 0)
    {
        ui->counter->setText(i18np("Downloading 1 camera...", "Downloading %1 cameras...", downloadFileCounter));
        return;
    }

    if (failedDownloads.isEmpty())
    {
        // All downloads completed, dialog can close
        QMetaObject::invokeMethod(this, "accept", Qt::QueuedConnection);
        return;
    }

    refreshCameraList();
    updateDownloadButton();
    ui->counter->setText(i18n("Could not download: %1", failedDownloads.join(", ")));
}

void FileUtilityCameraDataDialog::setAvailableCameraDataFiles(
    const QList<OptimalExposure::RemoteCameraDataFile> &newAvailableCameraDataFiles)
{
    availableCameraDataFiles = newAvailableCameraDataFiles;
}


QList<OptimalExposure::RemoteCameraDataFile> FileUtilityCameraDataDialog::getAvailableCameraDataFiles()
{
    return availableCameraDataFiles;
}

QNetworkRequest *FileUtilityCameraDataDialog::getRequest() const
{
    return request;
}

void FileUtilityCameraDataDialog::setRequest(QNetworkRequest *newRequest)
{
    request = newRequest;
}

QNetworkReply *FileUtilityCameraDataDialog::getReply() const
{
    return reply;
}

void FileUtilityCameraDataDialog::setReply(QNetworkReply *newResponse)
{
    reply = newResponse;
}

QNetworkAccessManager *FileUtilityCameraDataDialog::getANetworkAccessManager() const
{
    return aNetworkAccessManager;
}

void FileUtilityCameraDataDialog::setANetworkAccessManager(QNetworkAccessManager *newANetworkAccessManager)
{
    aNetworkAccessManager = newANetworkAccessManager;
}

