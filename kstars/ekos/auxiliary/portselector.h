/*  Ekos Port Selector
    Copyright (C) 2021 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#pragma once

#include <QDialog>
#include <QStandardItemModel>
#include <QJsonObject>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QHBoxLayout>

#include <KLed>

#include <memory>

#include "indi/indistd.h"
#include "profileinfo.h"

namespace  Selector
{

typedef enum
{
    CONNECTION_SERIAL,
    CONNECTION_NETWORK,
    CONNECTION_NONE

} ConnectionMode;


class Device : public QObject
{
        Q_OBJECT

    public:
        explicit Device(ISD::GDInterface *device);

        QHBoxLayout *rowLayout()
        {
            return m_Layout;
        }

    private:

        // Create GUI
        void initGUI();
        // Sync GUI to data
        void syncGUI();

        void processNumber(INumberVectorProperty *nvp);
        void processText(ITextVectorProperty *tvp);
        void processSwitch(ISwitchVectorProperty *svp);
        void setConnected();
        void setDisconnected();

        // N.B. JM: Using smart pointers doesn't work very well in Qt widget parent/child
        // scheme and can lead to double deletion, so we're sticking to regular pointers.
        // General
        QHBoxLayout *m_Layout {nullptr};
        KLed *m_LED {nullptr};
        QLabel *m_Label {nullptr};
        QPushButton *m_ConnectB {nullptr};
        QPushButton *m_DisconnectB {nullptr};
        // Serial
        QPushButton *m_SerialB {nullptr};
        QComboBox *m_Ports {nullptr};
        QComboBox *m_BaudRates {nullptr};
        // Network
        QPushButton *m_NetworkB {nullptr};
        QLineEdit *m_HostName {nullptr};
        QLineEdit *m_HostPort {nullptr};
        QPushButton *m_HostProtocolTCP {nullptr};
        QPushButton *m_HostProtocolUDP {nullptr};

        QMap<IPState, int> ColorCode;
        ConnectionMode m_ConnectionMode { CONNECTION_NONE };
        ISD::GDInterface *m_Device {nullptr};
};

class Dialog : public QDialog
{
    public:
        explicit Dialog(QWidget *parent = nullptr);

        // Initialize device
        void addDevice(ISD::GDInterface * devices);

    private:

        std::vector <std::unique_ptr< Device>> m_Devices;
        QVBoxLayout *m_Layout {nullptr};
        static constexpr uint32_t BAUD_RATES[6] = { 9600, 19200, 38400, 57600, 115200, 230400 };
};

}
