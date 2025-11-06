/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "deviceselectiondialog.h"
#include "../tasktemplate.h"
#include "indi/indilistener.h"
#include "indi/indistd.h"
#include "ekos/auxiliary/opticaltrainmanager.h"
#include "ekos/scheduler/schedulermodulestate.h"
#include "ekos/scheduler/scheduler.h"
#include "ekos/manager.h"
#include "kstarsdata.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QDialogButtonBox>
#include <QJsonObject>
#include <QJsonArray>
#include <KLocalizedString>

namespace Ekos
{

DeviceSelectionDialog::DeviceSelectionDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(i18n("Select Device"));
    setupUI();
}

DeviceSelectionDialog::~DeviceSelectionDialog()
{
}

void DeviceSelectionDialog::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    // Title label
    m_titleLabel = new QLabel(this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    mainLayout->addWidget(m_titleLabel);

    // Description label
    m_descriptionLabel = new QLabel(this);
    m_descriptionLabel->setWordWrap(true);
    mainLayout->addWidget(m_descriptionLabel);

    // Device list
    m_deviceList = new QListWidget(this);
    m_deviceList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_deviceList->setAlternatingRowColors(true);
    m_deviceList->setMinimumHeight(200);
    mainLayout->addWidget(m_deviceList);

    // Info label
    auto *infoLabel = new QLabel(i18n("Select the device to use for this task"), this);
    infoLabel->setStyleSheet("QLabel { color: gray; font-style: italic; }");
    mainLayout->addWidget(infoLabel);

    // Button box
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okButton = buttonBox->button(QDialogButtonBox::Ok);
    m_cancelButton = buttonBox->button(QDialogButtonBox::Cancel);
    m_okButton->setEnabled(false);

    mainLayout->addWidget(buttonBox);

    // Connect signals
    connect(m_deviceList, &QListWidget::itemSelectionChanged,
            this, &DeviceSelectionDialog::onDeviceSelectionChanged);
    connect(m_deviceList, &QListWidget::itemDoubleClicked,
            this, &DeviceSelectionDialog::onDeviceDoubleClicked);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &DeviceSelectionDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    setMinimumWidth(400);
}

void DeviceSelectionDialog::setTemplate(TaskTemplate *tmpl)
{
    m_template = tmpl;

    if (m_template)
    {
        m_titleLabel->setText(m_template->name());
        m_descriptionLabel->setText(m_template->description());
        populateDevices();
    }
}

void DeviceSelectionDialog::populateDevices()
{
    m_deviceList->clear();

    if (!m_template)
        return;

    // Check if INDI is connected - determines which mode to use
    const QList<QSharedPointer<ISD::GenericDevice>> devices = INDIListener::devices();
    bool hasConnectedDevices = false;

    for (const auto &device : devices)
    {
        if (device && device->isConnected())
        {
            hasConnectedDevices = true;
            break;
        }
    }

    // Choose mode based on whether we have connected INDI devices
    if (hasConnectedDevices)
    {
        // Execution mode: Use live INDI devices
        populateDevicesFromINDI();
    }
    else
    {
        // Planning mode: Use cached properties from profile
        populateDevicesFromProfile();
    }
}

void DeviceSelectionDialog::populateDevicesFromINDI()
{
    // Get all connected devices from INDIListener
    const QList<QSharedPointer<ISD::GenericDevice>> devices = INDIListener::devices();

    int matchingDeviceCount = 0;

    for (const auto &device : devices)
    {
        if (!device || !device->isConnected())
            continue;

        // Check if device interface matches template's supported interfaces
        uint32_t deviceInterface = device->getDriverInterface();

        if (m_template->supportsDeviceInterface(deviceInterface))
        {
            // Validate that device has required properties with correct permissions
            if (!validateDeviceProperties(device))
                continue;

            QString deviceName = device->getDeviceName();
            QString deviceType = getDeviceTypeString(deviceInterface);
            QString iconName = getDeviceIcon(deviceInterface);

            auto *item = new QListWidgetItem(QIcon::fromTheme(iconName),
                                             QString("%1 (%2)").arg(deviceName).arg(deviceType));
            item->setData(Qt::UserRole, deviceName);
            m_deviceList->addItem(item);
            matchingDeviceCount++;
        }
    }

    if (matchingDeviceCount == 0)
    {
        auto *item = new QListWidgetItem(i18n("No compatible devices found"));
        item->setFlags(Qt::ItemIsEnabled); // Not selectable
        item->setForeground(Qt::gray);
        m_deviceList->addItem(item);
    }
    else if (matchingDeviceCount == 1)
    {
        // Auto-select if only one device
        m_deviceList->setCurrentRow(0);
    }
}

void DeviceSelectionDialog::populateDevicesFromProfile()
{
    // Get cached device properties from OpticalTrainManager
    QJsonArray devices;
    OpticalTrainManager *otm = OpticalTrainManager::Instance();

    if (!otm)
    {
        auto *item = new QListWidgetItem(i18n("Optical Train Manager not available"));
        item->setFlags(Qt::ItemIsEnabled); // Not selectable
        item->setForeground(Qt::gray);
        m_deviceList->addItem(item);
        return;
    }

    // Get the current profile name from SchedulerModuleState via Manager
    Ekos::Manager *manager = Ekos::Manager::Instance();
    if (!manager || !manager->schedulerModule() || !manager->schedulerModule()->moduleState())
    {
        auto *item = new QListWidgetItem(i18n("Scheduler state not available"));
        item->setFlags(Qt::ItemIsEnabled); // Not selectable
        item->setForeground(Qt::gray);
        m_deviceList->addItem(item);
        return;
    }

    QString profileName = manager->schedulerModule()->moduleState()->currentProfile();

    if (profileName == i18n("Default"))
        profileName = manager->getCurrentProfile();

    if (profileName.isEmpty())
    {
        auto *item = new QListWidgetItem(i18n("No profile selected"));
        item->setFlags(Qt::ItemIsEnabled); // Not selectable
        item->setForeground(Qt::gray);
        m_deviceList->addItem(item);
        return;
    }

    // Get profile ID by searching through Manager's profiles list
    int profileID = -1;
    for (const auto &profile : manager->getAllProfiles())
    {
        if (profile && profile->name == profileName)
        {
            profileID = profile->id;
            break;
        }
    }

    if (profileID < 0)
    {
        auto *item = new QListWidgetItem(i18n("Profile not found: %1").arg(profileName));
        item->setFlags(Qt::ItemIsEnabled); // Not selectable
        item->setForeground(Qt::gray);
        m_deviceList->addItem(item);
        return;
    }

    // Get all optical trains for this profile
    QList<QVariantMap> opticalTrains;
    if (!KStarsData::Instance()->userdb()->GetOpticalTrains(profileID, opticalTrains))
    {
        auto *item = new QListWidgetItem(i18n("No optical trains for profile: %1").arg(profileName));
        item->setFlags(Qt::ItemIsEnabled); // Not selectable
        item->setForeground(Qt::gray);
        m_deviceList->addItem(item);
        return;
    }

    // Collect devices from all trains in this profile
    QSet<QString> uniqueDeviceNames; // Track unique devices
    for (const auto &train : opticalTrains)
    {
        int trainID = train["id"].toInt();
        QJsonArray trainDevices;

        if (otm->importTrainINDIProperties(trainDevices, trainID))
        {
            // Add devices from this train to our collection, avoiding duplicates
            for (const QJsonValue &deviceValue : trainDevices)
            {
                QJsonObject deviceObj = deviceValue.toObject();
                QString deviceName = deviceObj["device_name"].toString();

                // Only add if we haven't seen this device before
                if (!uniqueDeviceNames.contains(deviceName))
                {
                    uniqueDeviceNames.insert(deviceName);
                    devices.append(deviceValue);
                }
            }
        }
    }

    if (devices.isEmpty())
    {
        auto *item = new QListWidgetItem(i18n("No cached devices for profile: %1").arg(profileName));
        item->setFlags(Qt::ItemIsEnabled); // Not selectable
        item->setForeground(Qt::gray);
        m_deviceList->addItem(item);
        return;
    }

    int matchingDeviceCount = 0;

    for (const QJsonValue &deviceValue : devices)
    {
        QJsonObject deviceObj = deviceValue.toObject();

        // Get device interface and name
        uint32_t deviceInterface = deviceObj["interface"].toInt();
        QString deviceName = deviceObj["device_name"].toString();

        if (m_template->supportsDeviceInterface(deviceInterface))
        {
            // Validate that device has required properties with correct permissions
            if (!validateDevicePropertiesFromJSON(deviceObj))
                continue;

            QString deviceType = getDeviceTypeString(deviceInterface);
            QString iconName = getDeviceIcon(deviceInterface);

            auto *item = new QListWidgetItem(QIcon::fromTheme(iconName),
                                             QString("%1 (%2) [Profile]").arg(deviceName).arg(deviceType));
            item->setData(Qt::UserRole, deviceName);
            m_deviceList->addItem(item);
            matchingDeviceCount++;
        }
    }

    if (matchingDeviceCount == 0)
    {
        auto *item = new QListWidgetItem(i18n("No compatible devices in profile"));
        item->setFlags(Qt::ItemIsEnabled); // Not selectable
        item->setForeground(Qt::gray);
        m_deviceList->addItem(item);
    }
    else if (matchingDeviceCount == 1)
    {
        // Auto-select if only one device
        m_deviceList->setCurrentRow(0);
    }
}

void DeviceSelectionDialog::onDeviceSelectionChanged()
{
    QListWidgetItem *item = m_deviceList->currentItem();

    if (item && item->flags().testFlag(Qt::ItemIsSelectable))
    {
        m_selectedDevice = item->data(Qt::UserRole).toString();
        m_okButton->setEnabled(true);
    }
    else
    {
        m_selectedDevice.clear();
        m_okButton->setEnabled(false);
    }
}

void DeviceSelectionDialog::onDeviceDoubleClicked(QListWidgetItem *item)
{
    if (item && item->flags().testFlag(Qt::ItemIsSelectable))
    {
        m_selectedDevice = item->data(Qt::UserRole).toString();
        accept();
    }
}

void DeviceSelectionDialog::accept()
{
    if (!m_selectedDevice.isEmpty())
    {
        QDialog::accept();
    }
}

bool DeviceSelectionDialog::validateDeviceProperties(const QSharedPointer<ISD::GenericDevice> &device)
{
    if (!m_template || !device)
        return false;

    // Get all SET actions from template (actions that write to properties)
    QJsonArray actions = m_template->actionDefinitions();

    for (const QJsonValue &actionValue : actions)
    {
        QJsonObject action = actionValue.toObject();

        // Only validate SET actions (type 0) as they require write permission
        if (action["type"].toInt() != 0)  // 0 = SET action
            continue;

        QString propertyName = action["property"].toString();

        // Get the property from the device
        INDI::Property prop = device->getProperty(propertyName.toUtf8());

        // Property must exist
        if (!prop.isValid())
            return false;

        // Property must have write permission (IP_RW or IP_WO)
        IPerm permission = prop.getPermission();
        if (permission == IP_RO)  // Read-only is not acceptable for SET actions
            return false;
    }

    return true;
}

bool DeviceSelectionDialog::validateDevicePropertiesFromJSON(const QJsonObject &deviceObj)
{
    if (!m_template)
        return false;

    // Get all SET actions from template
    QJsonArray actions = m_template->actionDefinitions();
    QJsonArray properties = deviceObj["properties"].toArray();

    for (const QJsonValue &actionValue : actions)
    {
        QJsonObject action = actionValue.toObject();

        // Only validate SET actions (type 0) as they require write permission
        if (action["type"].toInt() != 0)  // 0 = SET action
            continue;

        QString requiredPropertyName = action["property"].toString();
        bool propertyFound = false;

        // Search for the property in the device's property list
        for (const QJsonValue &propValue : properties)
        {
            QJsonObject prop = propValue.toObject();
            QString propertyName = prop["name"].toString();

            if (propertyName == requiredPropertyName)
            {
                propertyFound = true;

                // Check permission - must be "rw" or "wo", not "ro"
                QString permission = prop["permission"].toString();
                if (permission == "ro")
                    return false;  // Read-only is not acceptable

                break;
            }
        }

        // Property must exist
        if (!propertyFound)
            return false;
    }

    return true;
}

QString DeviceSelectionDialog::getDeviceIcon(uint32_t deviceInterface) const
{
    // Match INDI driver interface flags to appropriate icons
    if (deviceInterface & INDI::BaseDevice::TELESCOPE_INTERFACE)
        return "kstars_telescope";
    else if (deviceInterface & INDI::BaseDevice::CCD_INTERFACE)
        return "kstars_ccd";
    else if (deviceInterface & INDI::BaseDevice::GUIDER_INTERFACE)
        return "kstars_ccd"; // Use CCD icon for guiders
    else if (deviceInterface & INDI::BaseDevice::FOCUSER_INTERFACE)
        return "kstars_focuser";
    else if (deviceInterface & INDI::BaseDevice::FILTER_INTERFACE)
        return "kstars_filter";
    else if (deviceInterface & INDI::BaseDevice::DOME_INTERFACE)
        return "kstars_dome";
    else if (deviceInterface & INDI::BaseDevice::GPS_INTERFACE)
        return "preferences-system-time";
    else if (deviceInterface & INDI::BaseDevice::WEATHER_INTERFACE)
        return "weather-clear";
    else if (deviceInterface & INDI::BaseDevice::AO_INTERFACE)
        return "kstars_ccd";
    else if (deviceInterface & INDI::BaseDevice::DUSTCAP_INTERFACE)
        return "games-endturn"; // Generic icon for dust cap
    else if (deviceInterface & INDI::BaseDevice::LIGHTBOX_INTERFACE)
        return "visibility";
    else if (deviceInterface & INDI::BaseDevice::DETECTOR_INTERFACE)
        return "kstars_ccd";
    else if (deviceInterface & INDI::BaseDevice::ROTATOR_INTERFACE)
        return "transform-rotate";
    else if (deviceInterface & INDI::BaseDevice::SPECTROGRAPH_INTERFACE)
        return "kstars_ccd";
    else if (deviceInterface & INDI::BaseDevice::CORRELATOR_INTERFACE)
        return "kstars_ccd";
    else if (deviceInterface & INDI::BaseDevice::AUX_INTERFACE)
        return "applications-system";
    else
        return "applications-system"; // Generic fallback
}

QString DeviceSelectionDialog::getDeviceTypeString(uint32_t deviceInterface) const
{
    // Return user-friendly device type string
    if (deviceInterface & INDI::BaseDevice::TELESCOPE_INTERFACE)
        return i18n("Mount");
    else if (deviceInterface & INDI::BaseDevice::CCD_INTERFACE)
        return i18n("Camera");
    else if (deviceInterface & INDI::BaseDevice::GUIDER_INTERFACE)
        return i18n("Guider");
    else if (deviceInterface & INDI::BaseDevice::FOCUSER_INTERFACE)
        return i18n("Focuser");
    else if (deviceInterface & INDI::BaseDevice::FILTER_INTERFACE)
        return i18n("Filter Wheel");
    else if (deviceInterface & INDI::BaseDevice::DOME_INTERFACE)
        return i18n("Dome");
    else if (deviceInterface & INDI::BaseDevice::GPS_INTERFACE)
        return i18n("GPS");
    else if (deviceInterface & INDI::BaseDevice::WEATHER_INTERFACE)
        return i18n("Weather Station");
    else if (deviceInterface & INDI::BaseDevice::AO_INTERFACE)
        return i18n("Adaptive Optics");
    else if (deviceInterface & INDI::BaseDevice::DUSTCAP_INTERFACE)
        return i18n("Dust Cap");
    else if (deviceInterface & INDI::BaseDevice::LIGHTBOX_INTERFACE)
        return i18n("Light Box");
    else if (deviceInterface & INDI::BaseDevice::DETECTOR_INTERFACE)
        return i18n("Detector");
    else if (deviceInterface & INDI::BaseDevice::ROTATOR_INTERFACE)
        return i18n("Rotator");
    else if (deviceInterface & INDI::BaseDevice::SPECTROGRAPH_INTERFACE)
        return i18n("Spectrograph");
    else if (deviceInterface & INDI::BaseDevice::CORRELATOR_INTERFACE)
        return i18n("Correlator");
    else if (deviceInterface & INDI::BaseDevice::AUX_INTERFACE)
        return i18n("Auxiliary");
    else
        return i18n("Device");
}

} // namespace Ekos
