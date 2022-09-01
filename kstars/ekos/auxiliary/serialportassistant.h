/*
    SPDX-FileCopyrightText: 2019 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>
#include <QStandardItemModel>
#include <QNetworkAccessManager>
#include <QJsonObject>

#include <memory>

#include "indi/indistd.h"
#include "profileinfo.h"

#include "ui_serialportassistant.h"

class SerialPortAssistant : public QDialog, public Ui::SerialPortAssistant
{
    public:
        explicit SerialPortAssistant(ProfileInfo *profile, QWidget *parent = nullptr);

        void addDevice(const QSharedPointer<ISD::GenericDevice> &device);


    private:
        bool loadRules();
        bool removeActiveRule();
        bool addRule(const QJsonObject &rule);
        void addDevicePage(const QSharedPointer<ISD::GenericDevice> &device);
        void gotoDevicePage(const QSharedPointer<ISD::GenericDevice> &device);
        void resetCurrentPage();

        void scanDevices();
        void parseDevices();
        void discoverDevice();

        QList<QSharedPointer<ISD::GenericDevice>> m_Devices;

        std::unique_ptr<QStandardItemModel> model;
        QSharedPointer<ISD::GenericDevice> m_CurrentDevice;
        const ProfileInfo *m_Profile;

        QNetworkAccessManager manager;
};
