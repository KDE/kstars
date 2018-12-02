/*  Ekos Serial Port Assistant tool
    Copyright (C) 2019 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#pragma once

#include <QDialog>
#include <QStandardItemModel>

#include <memory>

#include "indi/indistd.h"
#include "profileinfo.h"

#include "ui_serialportassistant.h"

class SerialPortAssistant : public QDialog, public Ui::SerialPortAssistant
{
public:
    SerialPortAssistant(ProfileInfo *profile, QWidget *parent = nullptr);

    void addDevice(ISD::GDInterface *device);


private:
    bool loadRules();
    bool removeActiveRule();
    void addPage(ISD::GDInterface *device);
    void gotoPage(ISD::GDInterface *device);
    void discoverDevice();

    QList<ISD::GDInterface *> devices;

    std::unique_ptr<QStandardItemModel> model;
    ISD::GDInterface *currentDevice { nullptr };
    const ProfileInfo *m_Profile;
};
