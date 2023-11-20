/*
    SPDX-FileCopyrightText: 2023 Joseph McGee <joseph.mcgee@sbcglobal.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QLoggingCategory>
#include <QDir>
#include <QFile>
#include "fileutilitycameradata.h"
#include "./ui_fileutilitycameradatadialog.h"
#include <ekos_capture_debug.h>

FileUtilityCameraDataDialog::FileUtilityCameraDataDialog(
    QWidget *parent,
    const QString &aPreferredCameraId) :
    QDialog(parent),
    aPreferredCameraId(aPreferredCameraId),
    ui(new Ui::FileUtilityCameraDataDialog)
{
    ui->setupUi(this);

    OptimalExposure::FileUtilityCameraData::downloadRepositoryCameraDataFileList(this);

    connect(ui->downloadB, &QPushButton::clicked, this, &FileUtilityCameraDataDialog::startCameraDownload);

    // ui->buttonBox->
    // ui->buttonBox->Cancel

    ui->availableRemoteCameraList->clear();
    ui->availableRemoteCameraList->setSelectionMode(QAbstractItemView::ExtendedSelection);

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

    // This probably needs to move to a method called when downloadRepositoryCameraDataFileList has completed
    QVector<QString> availableCameraDataFiles = getAvailableCameraDataFiles();
    foreach(QString availableCameraFile, availableCameraDataFiles)
    {
        // Make the camera list user friendly... to match the camera device id
        QString availableCameraId = OptimalExposure::FileUtilityCameraData::cameraDataFileNameToCameraId(availableCameraFile);

        ui->availableRemoteCameraList->addItem(availableCameraId);
    }

    // Pre-select the value for aPreferredCameraId, (which should be the active camera device id)
    QList<QListWidgetItem *> items = ui->availableRemoteCameraList->findItems(aPreferredCameraId, Qt::MatchExactly);
    if (items.size() > 0)
    {
        items[0]->setSelected(true);
    }
}


void FileUtilityCameraDataDialog::startCameraDownload()
{


    QList<QListWidgetItem *> selectedItems =  ui->availableRemoteCameraList->selectedItems();
    if (selectedItems.size() > 0)
    {
        this->setDownloadFileCounter(selectedItems.size());
        // qCInfo(KSTARS_EKOS_CAPTURE) << "Selected Cameras " << selectedItems.size();
        foreach(QListWidgetItem *aSelectedCameraItem, selectedItems)
        {
            QString aSelectedCameraId = aSelectedCameraItem->text();
            // qCInfo(KSTARS_EKOS_CAPTURE) << "attempt Download of " << aSelectedCameraId;
            OptimalExposure::FileUtilityCameraData::downloadCameraDataFile(aSelectedCameraId, this);
        }
    }
}

int FileUtilityCameraDataDialog::getDownloadFileCounter() const
{
    return downloadFileCounter;
}

void FileUtilityCameraDataDialog::setDownloadFileCounter(int newDownloadFileCounter)
{
    downloadFileCounter = newDownloadFileCounter;
    ui->counter->text() = QString::number(downloadFileCounter);
    qCInfo(KSTARS_EKOS_CAPTURE) << "Camera Data download file counter set "
                                << QString::number(downloadFileCounter);
}

void FileUtilityCameraDataDialog::decrementDownloadFileCounter()
{
    downloadFileCounter--;
    ui->counter->text() = QString::number(downloadFileCounter);
    qCInfo(KSTARS_EKOS_CAPTURE) << "Camera Data download file counter decremented "
                                << QString::number(downloadFileCounter);
    if(downloadFileCounter == 0)
    {
        // downloads should have completed, dialog can close
        // FileUtilityCameraDataDialog::delay(1);
        qCInfo(KSTARS_EKOS_CAPTURE) << "Camera Data download file counter reach 0. Forcing dialog to close.";
        QMetaObject::invokeMethod(this, "close", Qt::QueuedConnection);
    }
}

void FileUtilityCameraDataDialog::setAvailableCameraDataFiles(QVector<QString> newAvailableCameraDataFiles)
{
    availableCameraDataFiles = newAvailableCameraDataFiles;
}


QVector<QString> FileUtilityCameraDataDialog::getAvailableCameraDataFiles()
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

