/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>
#include <QString>
#include <QList>
#include <QJsonArray>

class QListWidget;
class QListWidgetItem;
class QPushButton;
class QLabel;

namespace ISD
{
class GenericDevice;
}

namespace Ekos
{

class TaskTemplate;

/**
 * @class DeviceSelectionDialog
 * @brief Dialog for selecting an INDI device to use with a template
 *
 * This dialog displays a list of connected devices that match the
 * template's supported interfaces, allowing the user to select which
 * device should be used for the task.
 */
class DeviceSelectionDialog : public QDialog
{
        Q_OBJECT

    public:
        explicit DeviceSelectionDialog(QWidget *parent = nullptr);
        ~DeviceSelectionDialog() override;

        /**
         * @brief Set the template to filter devices by
         * @param tmpl Template whose supported_interfaces will be used to filter devices
         */
        void setTemplate(TaskTemplate *tmpl);

        /**
         * @brief Get the selected device name
         * @return Name of the selected device, or empty string if none selected
         */
        QString selectedDevice() const
        {
            return m_selectedDevice;
        }

    protected:
        void accept() override;

    private slots:
        void onDeviceSelectionChanged();
        void onDeviceDoubleClicked(QListWidgetItem *item);

    private:
        void setupUI();
        void populateDevices();
        void populateDevicesFromINDI();
        void populateDevicesFromProfile();
        bool validateDeviceProperties(const QSharedPointer<ISD::GenericDevice> &device);
        bool validateDevicePropertiesFromJSON(const QJsonObject &deviceJson);
        QString getDeviceIcon(uint32_t deviceInterface) const;
        QString getDeviceTypeString(uint32_t deviceInterface) const;

        TaskTemplate *m_template = nullptr;
        QString m_selectedDevice;

        // UI widgets
        QLabel *m_titleLabel = nullptr;
        QLabel *m_descriptionLabel = nullptr;
        QListWidget *m_deviceList = nullptr;
        QPushButton *m_okButton = nullptr;
        QPushButton *m_cancelButton = nullptr;
};

} // namespace Ekos
