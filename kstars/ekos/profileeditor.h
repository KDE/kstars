/*  Profile Editor
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#pragma once

#include "oal/scope.h"
#include "ui_profileeditor.h"

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

        void setPi(ProfileInfo *value);

        void loadDrivers();
        void loadScopeEquipment();

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
        void showINDIHub();

    private:
        QString getTooltip(DriverInfo *dv);
        void scanIP(const QString &ip);
        void clearAllRequests();

        ProfileEditorUI *ui { nullptr };
        ProfileInfo *pi { nullptr };
        QList<OAL::Scope *> m_scopeList;
        QStandardItemModel *m_MountModel { nullptr };
        uint8_t m_INDIHub { 0 };

        QPointer<QProgressDialog> m_ProgressDialog;
        QNetworkAccessManager m_Manager;
        QList<QNetworkReply*> m_Replies;

        bool m_CancelScan { false };
};
