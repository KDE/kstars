/*
    SPDX-FileCopyrightText: 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "oal/scope.h"
#include "ui_profileeditor.h"
#include "indi/indicommon.h"
#include <QDialog>
#include <QFrame>
#include <QString>
#include <QStringList>
#include <QPointer>
#include <QProgressDialog>
#include <QNetworkAccessManager>

class ProfileInfo;
class QStandardItemModel;
class DriverInfo;

class ProfileEditorUI : public QFrame, public Ui::ProfileEditorUI
{
        Q_OBJECT

    public:
        /** @short Constructor */
        explicit ProfileEditorUI(QWidget *parent);
};

class ProfileEditor : public QDialog
{
        Q_OBJECT
    public:
        /** @short Constructor */
        explicit ProfileEditor(QWidget *ks);

        /** @short Destructor */
        virtual ~ProfileEditor() override = default;

        void setPi(const QSharedPointer<ProfileInfo> &newProfile);

        void loadDrivers();

        void setProfileName(const QString &name);
        void setAuxDrivers(const QStringList &aux);
        void setHostPort(const QString &host, const QString &port);
        void setWebManager(bool enabled, const QString &port = "8624");
        void setGuiderType(int type);
        void setConnectionOptionsEnabled(bool enable);

        void setSettings(const QJsonObject &profile);

    public slots:
        void saveProfile();
        void setRemoteMode(bool enable);

    private slots:
        void updateGuiderSelection(int id);
        void scanNetwork();
        void executeScriptEditor();

    private:
        void populateManufacturerCombo(QStandardItemModel *model, QComboBox *combo, const QString &selectedDriver, bool isLocal,
                                       const QList<DeviceFamily> &families);
        QString getTooltip(const QSharedPointer<DriverInfo> &driver);
        void scanIP(const QString &ip);
        void clearAllRequests();

        struct DeviceInfo
        {
            QComboBox *combo;
            QStandardItemModel *model;
            QString selectedDriver;
            QList<DeviceFamily> families;
        };

        ProfileEditorUI *ui { nullptr };
        QSharedPointer<ProfileInfo> pi;
        QMap<QString, DeviceInfo> m_Devices;

        QPointer<QProgressDialog> m_ProgressDialog;
        QNetworkAccessManager m_Manager;
        QList<QNetworkReply*> m_Replies;

        bool m_CancelScan { false };
};
