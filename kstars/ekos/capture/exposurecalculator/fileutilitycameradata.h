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
#include <QDialog>
#include <kspaths.h>

QT_BEGIN_NAMESPACE
namespace OptimalExposure
{
/** A camera data file listed in the remote repository. */
struct RemoteCameraDataFile
{
    QString fileName;
    /** Git blob SHA-1 of the file, used to detect changed files. */
    QString sha;
};

class FileUtilityCameraData
{

    public:

        /**
         * @brief List the camera data files, one per camera.
         * When a camera has a file in the user directory and in the KStars data directory,
         * the user's copy (downloaded or hand-written) is listed.
         */
        QStringList static getAvailableCameraFilesList();
        /** @brief List the camera data files in directories, the first directory having priority. */
        QStringList static cameraFilesInDirectories(const QStringList &directories);
        /** @brief Path of the installed data file for a camera, or an empty string. */
        QString static installedCameraDataFile(const QString &cameraId);
        /** @brief Directory where downloaded camera data files are saved, with a trailing slash. */
        QString static localCameraDataDirectory();

        /** @brief Parse the GitHub tree listing of the repository into its XML files. */
        QList<RemoteCameraDataFile> static parseRepositoryListing(const QByteArray &json);
        /** @brief Git blob SHA-1 of content, as listed by the GitHub tree API. */
        QString static gitBlobSha(const QByteArray &content);

        // bool static isExposureCalculatorCameraDataAvailable();

        void static downloadRepositoryCameraDataFileList(QDialog *aDialog);
        void static downloadCameraDataFile(QString cameraId, QDialog *aDialog);
        int static readCameraDataFile(QString cameraId, ImagingCameraData *anImagingCameraData);
        int static writeCameraDataFile(ImagingCameraData *anImagingCameraData);
        void static buildCameraDataFile();
        void static initializeCameraDataPaths();

        QString static cameraIdToCameraDataFileName(QString cameraId);
        QString static cameraDataFileNameToCameraId(QString cameraDataFileName);

        QString static const cameraDataRemoteRepositoryList;
        QString static const cameraDataRemoteRepository;
};
}

QT_END_NAMESPACE
#endif // FILEUTILITYCAMERADATA_H
