/*
    SPDX-FileCopyrightText: 2023 Joseph McGee <joseph.mcgee@sbcglobal.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QAbstractItemModel>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QUrl>
#include <QTimer>
#include <QDialog>
#include "imagingcameradata.h"
#include <kspaths.h>

namespace OptimalExposure
{
class FileUtilityCameraData
{

    public:

        QStringList static getAvailableCameraFilesList();

        // bool static isExposureCalculatorCameraDataAvailable();

        void static downloadRepositoryCameraDataFileList(QDialog *aDialog);
        void static downloadCameraDataFile(QString cameraId, QDialog *aDialog);
        int static readCameraDataFile(QString cameraId, ImagingCameraData *anImagingCameraData);
        int static writeCameraDataFile(ImagingCameraData *anImagingCameraData);
        void static buildCameraDataFile();
        void static initializeCameraDataPaths();

        QString static cameraIdToCameraDataFileName(QString cameraId);
        QString static cameraDataFileNameToCameraId(QString cameraDataFileName);

        QString static const cameraApplicationDataRepository;
        QString static const cameraLocalDataRepository;

        QString static const cameraDataRemoteRepositoryList;
        QString static const cameraDataRemoteRepository;
};
}
