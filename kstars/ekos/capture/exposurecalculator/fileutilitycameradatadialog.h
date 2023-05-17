/*
    SPDX-FileCopyrightText: 2023 Joseph McGee <joseph.mcgee@sbcglobal.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "fileutilitycameradata.h"
#include <QDialog>

namespace Ui
{
class FileUtilityCameraDataDialog;
}

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
        QVector<QString> getAvailableCameraDataFiles();
        void setAvailableCameraDataFiles(QVector<QString> newAvailableCameraDataFiles);
        void refreshCameraList(); // call to repaint the file list after download completes

        int getDownloadFileCounter() const;
        void setDownloadFileCounter(int newDownloadFileCounter);
        void decrementDownloadFileCounter();


    private slots:
        void startCameraDownload();

    private:
        Ui::FileUtilityCameraDataDialog *ui;
        QString aPreferredCameraId;

        QNetworkAccessManager *aNetworkAccessManager;
        QNetworkRequest *request;
        QNetworkReply *reply;
        QVector<QString> availableCameraDataFiles;

        // void delay(int delaySeconds);
        int downloadFileCounter;
};

