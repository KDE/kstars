/*
    SPDX-FileCopyrightText: 2021 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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
        explicit Device(ISD::GDInterface *device, QGridLayout *grid, uint8_t row);

        const QString name() const
        {
            return m_Device->getDeviceName();
        }

        uint8_t systemPortCount() const;

        // Create GUI
        bool initGUI();
        // Sync GUI to data
        void syncGUI();

    private:
        void processText(ITextVectorProperty *tvp);
        void processSwitch(ISwitchVectorProperty *svp);
        void setConnected();
        void setDisconnected();
        void setConnectionMode(ConnectionMode mode);

        // N.B. JM: Using smart pointers doesn't work very well in Qt widget parent/child
        // scheme and can lead to double deletion, so we're sticking to regular pointers.
        // General
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

        QMap<IPState, QColor> ColorCode;
        ConnectionMode m_ConnectionMode { CONNECTION_NONE };


        ISD::GDInterface *m_Device {nullptr};
        QGridLayout *m_Grid{nullptr};
        uint8_t m_Row {0};

        static const QStringList BAUD_RATES;
        static const QString ACTIVE_STYLESHEET;
};

class Dialog : public QDialog
{
    public:
        explicit Dialog(QWidget *parent = nullptr);

        // Initialize device
        void addDevice(ISD::GDInterface * devices);

        bool shouldShow() const;

        bool empty() const
        {
            return m_Devices.empty();
        }

    private:

        std::vector <std::unique_ptr< Device>> m_Devices;
        QGridLayout *m_Layout {nullptr};
};

}
