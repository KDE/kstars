/*
    SPDX-FileCopyrightText: 2023 Joseph McGee <joseph.mcgee@sbcglobal.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#ifndef FILEUTILITYCAMERADATA_H
#define FILEUTILITYCAMERADATA_H

#include <QAbstractItemModel>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QUrl>
#include <QTimer>
#include "imagingcameradata.h"
#include "cameragainreadnoise.h"
#include "fileutilitycameradatadialog.h"
#include <kspaths.h>

QT_BEGIN_NAMESPACE
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

QT_END_NAMESPACE
#endif // FILEUTILITYCAMERADATA_H
