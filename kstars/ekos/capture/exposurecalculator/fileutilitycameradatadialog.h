/*
    SPDX-FileCopyrightText: 2023 Joseph McGee <joseph.mcgee@sbcglobal.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#ifndef FILEUTILITYCAMERADATADIALOG_H
#define FILEUTILITYCAMERADATADIALOG_H

#include "fileutilitycameradata.h"
#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui
{
class FileUtilityCameraDataDialog;
}
QT_END_NAMESPACE

class FileUtilityCameraDataDialog : public QDialog
{
        Q_OBJECT

    public:
        FileUtilityCameraDataDialog(QWidget *parent = nullptr,
                                    const QString &aPreferredCameraId = "");
        ~FileUtilityCameraDataDialog();

        QNetworkAccessManager *getANetworkAccessManager() const;
        void setANetworkAccessManager(QNetworkAccessManager *newANetworkAccessManager);

        QNetworkReply *getReply() const;
        void setReply(QNetworkReply *newReply);

        QNetworkRequest *getRequest() const;
        void setRequest(QNetworkRequest *newRequest);

        // Available refers to the files in a repository
        QList<OptimalExposure::RemoteCameraDataFile> getAvailableCameraDataFiles();
        void setAvailableCameraDataFiles(const QList<OptimalExposure::RemoteCameraDataFile> &newAvailableCameraDataFiles);
        void refreshCameraList(); // call to repaint the file list after download completes
        /** @brief Report that the list of cameras could not be downloaded. */
        void showListError(const QString &error);

        int getDownloadFileCounter() const;
        /** @brief Called when the download of one camera data file has completed or failed. */
        void downloadFinished(const QString &cameraId, bool success);


    private Q_SLOTS:
        void startCameraDownload();
        void updateDownloadButton();

    private:
        Ui::FileUtilityCameraDataDialog *ui;
        QString aPreferredCameraId;

        QNetworkAccessManager *aNetworkAccessManager;
        QNetworkRequest *request;
        QNetworkReply *reply;
        QList<OptimalExposure::RemoteCameraDataFile> availableCameraDataFiles;

        // void delay(int delaySeconds);
        int downloadFileCounter { 0 };
        QStringList failedDownloads;
};

#endif // FILEUTILITYCAMERADATADIALOG_H
