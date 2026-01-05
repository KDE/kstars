/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "propertytemplatebuilder.h"
#include "../tasktemplate.h"
#include "../templatemanager.h"
#include "indi/indilistener.h"
#include "indi/indistd.h"
#include "indi/indidevice.h"
#include "indi/indiproperty.h"
#include "ekos/auxiliary/opticaltrainmanager.h"
#include "ekos/scheduler/schedulermodulestate.h"
#include "ekos/scheduler/scheduler.h"
#include "ekos/manager.h"
#include "kstarsdata.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QStackedWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTextEdit>
#include <QCheckBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QGroupBox>
#include <QMessageBox>
#include <QScrollArea>
#include <QDialogButtonBox>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUuid>
#include <KLocalizedString>

namespace Ekos
{

PropertyTemplateBuilderDialog::PropertyTemplateBuilderDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(i18n("Property Template Builder"));
    resize(700, 600);
    setupUI();
    populateDevices();
}

PropertyTemplateBuilderDialog::PropertyTemplateBuilderDialog(TaskTemplate *existingTemplate, QWidget *parent)
    : QDialog(parent)
    , m_templateToEdit(existingTemplate)
    , m_isEditMode(true)
{
    setWindowTitle(i18n("Edit Template"));
    resize(700, 600);
    setupUI();
    loadTemplateForEditing(existingTemplate);
}

PropertyTemplateBuilderDialog::~PropertyTemplateBuilderDialog()
{
    if (m_generatedTemplate && m_generatedTemplate->parent() == this)
    {
        m_generatedTemplate->deleteLater();
    }
}

void PropertyTemplateBuilderDialog::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    // Page title and description
    m_pageTitle = new QLabel(this);
    QFont titleFont = m_pageTitle->font();
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleFont.setBold(true);
    m_pageTitle->setFont(titleFont);
    mainLayout->addWidget(m_pageTitle);

    m_pageDescription = new QLabel(this);
    m_pageDescription->setWordWrap(true);
    mainLayout->addWidget(m_pageDescription);

    // Stacked widget for pages
    m_pageStack = new QStackedWidget(this);
    mainLayout->addWidget(m_pageStack, 1);

    // Setup individual pages
    setupDeviceSelectionPage();
    setupPropertySelectionPage();
    setupActionConfigurationPage();
    setupMetadataPage();
    setupPreviewPage();

    // Navigation buttons
    auto *buttonLayout = new QHBoxLayout();
    m_backButton = new QPushButton(QIcon::fromTheme("go-previous"), i18n("Back"), this);
    m_nextButton = new QPushButton(QIcon::fromTheme("go-next"), i18n("Next"), this);

    m_saveButton = new QPushButton(QIcon::fromTheme("document-save"), i18n("Save"), this);
    m_saveButton->setEnabled(false);  // Disabled until preview is generated
    m_cancelButton = new QPushButton(QIcon::fromTheme("dialog-cancel"), i18n("Cancel"), this);

    buttonLayout->addWidget(m_backButton);
    buttonLayout->addWidget(m_nextButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_saveButton);
    buttonLayout->addWidget(m_cancelButton);

    mainLayout->addLayout(buttonLayout);

    // Connect signals
    connect(m_backButton, &QPushButton::clicked, this, &PropertyTemplateBuilderDialog::onBackClicked);
    connect(m_nextButton, &QPushButton::clicked, this, &PropertyTemplateBuilderDialog::onNextClicked);
    connect(m_saveButton, &QPushButton::clicked, this, &PropertyTemplateBuilderDialog::accept);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    // Set initial page
    goToPage(PAGE_DEVICE_SELECTION);
}

void PropertyTemplateBuilderDialog::setupDeviceSelectionPage()
{
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);

    // Device selection
    auto *formLayout = new QFormLayout();
    m_deviceCombo = new QComboBox(page);
    m_deviceCombo->setEditable(false);
    formLayout->addRow(i18n("Device:"), m_deviceCombo);

    m_deviceInfoLabel = new QLabel(page);
    m_deviceInfoLabel->setWordWrap(true);
    m_deviceInfoLabel->setStyleSheet("QLabel { color: gray; font-style: italic; }");
    formLayout->addRow("", m_deviceInfoLabel);

    layout->addLayout(formLayout);
    layout->addStretch();

    connect(m_deviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PropertyTemplateBuilderDialog::onDeviceSelected);

    m_pageStack->addWidget(page);
}

void PropertyTemplateBuilderDialog::setupPropertySelectionPage()
{
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);

    // Property selection
    auto *formLayout = new QFormLayout();
    m_propertyCombo = new QComboBox(page);
    m_propertyCombo->setEditable(true);
    m_propertyCombo->setInsertPolicy(QComboBox::NoInsert);
    formLayout->addRow(i18n("Property:"), m_propertyCombo);

    m_propertyTypeLabel = new QLabel(page);
    formLayout->addRow(i18n("Type:"), m_propertyTypeLabel);

    m_propertyPermissionLabel = new QLabel(page);
    formLayout->addRow(i18n("Permission:"), m_propertyPermissionLabel);

    m_propertyInfoLabel = new QLabel(page);
    m_propertyInfoLabel->setWordWrap(true);
    m_propertyInfoLabel->setStyleSheet("QLabel { color: gray; font-style: italic; }");
    formLayout->addRow("", m_propertyInfoLabel);

    layout->addLayout(formLayout);
    layout->addStretch();

    connect(m_propertyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PropertyTemplateBuilderDialog::onPropertySelected);

    m_pageStack->addWidget(page);
}

void PropertyTemplateBuilderDialog::setupActionConfigurationPage()
{
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);

    // Scroll area for configuration options
    auto *scrollArea = new QScrollArea(page);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    m_actionConfigWidget = new QWidget();
    auto *configLayout = new QVBoxLayout(m_actionConfigWidget);

    // Action type selection
    auto *actionTypeGroup = new QGroupBox(i18n("Actions"), m_actionConfigWidget);
    auto *actionTypeLayout = new QVBoxLayout(actionTypeGroup);

    m_includeSetAction = new QCheckBox(i18n("SET - Write value to property"), actionTypeGroup);
    m_includeSetAction->setChecked(true);
    actionTypeLayout->addWidget(m_includeSetAction);

    m_includeEvaluateAction = new QCheckBox(i18n("EVALUATE - Check property condition"), actionTypeGroup);
    m_includeEvaluateAction->setChecked(true);
    actionTypeLayout->addWidget(m_includeEvaluateAction);

    configLayout->addWidget(actionTypeGroup);

    // Property element selection
    auto *elementGroup = new QGroupBox(i18n("Property Element"), m_actionConfigWidget);
    auto *elementLayout = new QFormLayout(elementGroup);

    m_propertyElementCombo = new QComboBox(elementGroup);
    elementLayout->addRow(i18n("Element:"), m_propertyElementCombo);

    configLayout->addWidget(elementGroup);

    // This will be populated dynamically based on property type
    configLayout->addStretch();

    // Common action settings
    auto *settingsGroup = new QGroupBox(i18n("Action Settings"), m_actionConfigWidget);
    auto *settingsLayout = new QFormLayout(settingsGroup);

    m_timeoutSpin = new QSpinBox(settingsGroup);
    m_timeoutSpin->setRange(1, 3600);
    m_timeoutSpin->setValue(30);
    m_timeoutSpin->setSuffix(" " + i18n("seconds"));
    settingsLayout->addRow(i18n("Timeout:"), m_timeoutSpin);

    m_retriesSpin = new QSpinBox(settingsGroup);
    m_retriesSpin->setRange(0, 10);
    m_retriesSpin->setValue(2);
    settingsLayout->addRow(i18n("Retries:"), m_retriesSpin);

    m_failureActionCombo = new QComboBox(settingsGroup);
    m_failureActionCombo->addItem(i18n("Abort Queue"), 0);
    m_failureActionCombo->addItem(i18n("Continue"), 1);
    m_failureActionCombo->addItem(i18n("Skip to Next Task"), 2);
    settingsLayout->addRow(i18n("On Failure:"), m_failureActionCombo);

    configLayout->addWidget(settingsGroup);

    scrollArea->setWidget(m_actionConfigWidget);
    layout->addWidget(scrollArea);

    connect(m_includeSetAction, &QCheckBox::toggled, this, &PropertyTemplateBuilderDialog::onActionTypeChanged);
    connect(m_includeEvaluateAction, &QCheckBox::toggled, this, &PropertyTemplateBuilderDialog::onActionTypeChanged);
    connect(m_propertyElementCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PropertyTemplateBuilderDialog::onPropertyElementSelected);

    m_pageStack->addWidget(page);
}

void PropertyTemplateBuilderDialog::setupMetadataPage()
{
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);

    auto *formLayout = new QFormLayout();

    m_templateNameEdit = new QLineEdit(page);
    formLayout->addRow(i18n("Template Name:"), m_templateNameEdit);

    m_templateDescEdit = new QTextEdit(page);
    m_templateDescEdit->setMaximumHeight(100);
    formLayout->addRow(i18n("Description:"), m_templateDescEdit);

    m_categoryCombo = new QComboBox(page);
    m_categoryCombo->setEditable(true);
    m_categoryCombo->addItems(QStringList() << "Custom" << "Camera" << "Mount" << "Focuser"
                              << "Filter Wheel" << "Dome" << "Weather" << "Auxiliary");
    formLayout->addRow(i18n("Category:"), m_categoryCombo);

    layout->addLayout(formLayout);
    layout->addStretch();

    m_pageStack->addWidget(page);
}

void PropertyTemplateBuilderDialog::setupPreviewPage()
{
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);

    auto *previewLabel = new QLabel(i18n("Template Preview (JSON)"), page);
    QFont font = previewLabel->font();
    font.setBold(true);
    previewLabel->setFont(font);
    layout->addWidget(previewLabel);

    m_previewText = new QTextEdit(page);
    m_previewText->setReadOnly(true);
    m_previewText->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    layout->addWidget(m_previewText);

    m_pageStack->addWidget(page);
}

void PropertyTemplateBuilderDialog::goToPage(int pageIndex)
{
    m_currentPage = pageIndex;
    m_pageStack->setCurrentIndex(pageIndex);

    // Update title and description based on page
    switch (pageIndex)
    {
        case PAGE_DEVICE_SELECTION:
            m_pageTitle->setText(i18n("Step 1: Select Device"));
            m_pageDescription->setText(i18n("Choose the INDI device for which you want to create a custom template."));
            break;
        case PAGE_PROPERTY_SELECTION:
            m_pageTitle->setText(i18n("Step 2: Select Property"));
            m_pageDescription->setText(
                i18n("Choose the property you want to control with this template. Only settable properties (NUMBER, SWITCH, TEXT) and readable status properties (LIGHT) are shown."));
            break;
        case PAGE_ACTION_CONFIGURATION:
            m_pageTitle->setText(i18n("Step 3: Configure Actions"));
            m_pageDescription->setText(i18n("Configure how the property should be set and/or evaluated."));
            break;
        case PAGE_METADATA:
            m_pageTitle->setText(i18n("Step 4: Template Metadata"));
            m_pageDescription->setText(i18n("Provide a name, description, and category for your template."));
            break;
        case PAGE_PREVIEW:
            m_pageTitle->setText(i18n("Step 5: Preview and Save"));
            m_pageDescription->setText(i18n("Review the generated template before saving."));
            break;
    }

    updateNavigationButtons();
}

void PropertyTemplateBuilderDialog::updateNavigationButtons()
{
    // In edit mode, don't allow going back from ACTION_CONFIGURATION page
    if (m_isEditMode && m_currentPage == PAGE_ACTION_CONFIGURATION)
    {
        m_backButton->setEnabled(false);
    }
    else
    {
        m_backButton->setEnabled(m_currentPage > PAGE_DEVICE_SELECTION);
    }

    m_nextButton->setEnabled(m_currentPage < PAGE_PREVIEW);

    if (m_currentPage == PAGE_PREVIEW)
    {
        m_nextButton->setEnabled(false);

        // Auto-generate preview when on preview page
        QJsonObject templateJson = generateTemplateJson();
        QJsonDocument doc(templateJson);
        m_previewText->setPlainText(doc.toJson(QJsonDocument::Indented));

        // Enable Save button now that preview is ready
        m_saveButton->setEnabled(true);
    }
    else
    {
        // Disable Save button on other pages
        m_saveButton->setEnabled(false);
    }
}

void PropertyTemplateBuilderDialog::onNextClicked()
{
    if (!validateCurrentPage())
    {
        return;
    }

    int nextPage = m_currentPage + 1;

    // Populate data for next page
    if (nextPage == PAGE_PROPERTY_SELECTION)
    {
        populateProperties();
    }
    else if (nextPage == PAGE_ACTION_CONFIGURATION)
    {
        populatePropertyElements();
    }
    else if (nextPage == PAGE_METADATA)
    {
        m_templateNameEdit->setText(getDefaultTemplateName());
        m_templateDescEdit->setPlainText(QString("Custom template for %1 property on %2")
                                         .arg(m_selectedProperty).arg(m_selectedDevice));
    }

    goToPage(nextPage);
}

void PropertyTemplateBuilderDialog::onBackClicked()
{
    if (m_currentPage > PAGE_DEVICE_SELECTION)
    {
        goToPage(m_currentPage - 1);
    }
}

void PropertyTemplateBuilderDialog::onPreviewClicked()
{
    if (!validateCurrentPage())
    {
        return;
    }

    // Generate preview
    QJsonObject templateJson = generateTemplateJson();
    QJsonDocument doc(templateJson);
    m_previewText->setPlainText(doc.toJson(QJsonDocument::Indented));

    goToPage(PAGE_PREVIEW);
}

void PropertyTemplateBuilderDialog::populateDevices()
{
    m_deviceCombo->clear();

    // Check if INDI is connected
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

    m_isIndiMode = hasConnectedDevices;

    if (hasConnectedDevices)
    {
        // Use live INDI devices
        for (const auto &device : devices)
        {
            if (device && device->isConnected())
            {
                m_deviceCombo->addItem(device->getDeviceName(), device->getDeviceName());
            }
        }

        m_deviceInfoLabel->setText(i18n("Using connected INDI devices"));
    }
    else
    {
        // Use cached devices from profile
        Ekos::Manager *manager = Ekos::Manager::Instance();
        if (!manager || !manager->schedulerModule() || !manager->schedulerModule()->moduleState())
        {
            m_deviceInfoLabel->setText(i18n("No devices available"));
            return;
        }

        QString profileName = manager->schedulerModule()->moduleState()->currentProfile();
        if (profileName == i18n("Default"))
            profileName = manager->getCurrentProfile();

        if (profileName.isEmpty())
        {
            m_deviceInfoLabel->setText(i18n("No profile selected"));
            return;
        }

        // Get profile ID
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
            m_deviceInfoLabel->setText(i18n("Profile not found"));
            return;
        }

        // Get optical trains
        QList<QVariantMap> opticalTrains;
        if (!KStarsData::Instance()->userdb()->GetOpticalTrains(profileID, opticalTrains))
        {
            m_deviceInfoLabel->setText(i18n("No optical trains found"));
            return;
        }

        OpticalTrainManager *otm = OpticalTrainManager::Instance();
        if (!otm)
        {
            m_deviceInfoLabel->setText(i18n("Optical Train Manager not available"));
            return;
        }

        // Collect unique devices from all trains
        QSet<QString> uniqueDeviceNames;
        for (const auto &train : opticalTrains)
        {
            int trainID = train["id"].toInt();
            QJsonArray trainDevices;

            if (otm->importTrainINDIProperties(trainDevices, trainID))
            {
                for (const QJsonValue &deviceValue : trainDevices)
                {
                    QJsonObject deviceObj = deviceValue.toObject();
                    QString deviceName = deviceObj["device_name"].toString();

                    if (!uniqueDeviceNames.contains(deviceName))
                    {
                        uniqueDeviceNames.insert(deviceName);
                        m_deviceCombo->addItem(deviceName + " [Profile]", deviceName);
                    }
                }
            }
        }

        m_deviceInfoLabel->setText(i18n("Using cached devices from profile: %1", profileName));
    }

    if (m_deviceCombo->count() == 0)
    {
        m_deviceInfoLabel->setText(i18n("No devices found"));
    }
}

void PropertyTemplateBuilderDialog::onDeviceSelected(int index)
{
    if (index < 0)
    {
        m_selectedDevice.clear();
        m_deviceInterface = 0;
        return;
    }

    m_selectedDevice = m_deviceCombo->itemData(index).toString();

    // Get device interface
    if (m_isIndiMode)
    {
        auto device = getDevice(m_selectedDevice);
        if (device)
        {
            m_deviceInterface = device->getDriverInterface();
        }
    }
    else
    {
        QJsonObject deviceData = getDeviceFromProfile(m_selectedDevice);
        m_deviceInterface = deviceData["interface"].toInt();
        m_deviceCacheData = deviceData;
    }
}

void PropertyTemplateBuilderDialog::populateProperties()
{
    m_propertyCombo->clear();

    if (m_selectedDevice.isEmpty())
    {
        return;
    }

    if (m_isIndiMode)
    {
        // Get properties from live device
        auto device = getDevice(m_selectedDevice);
        if (!device)
        {
            m_propertyInfoLabel->setText(i18n("Device not found"));
            return;
        }

        auto properties = device->getProperties();
        for (const auto &prop : properties)
        {
            PropertyType propType = getPropertyType(prop);

            // Skip BLOB and UNKNOWN types
            if (propType == PROP_BLOB || propType == PROP_UNKNOWN)
                continue;

            QString propName = prop.getName();
            QString propLabel = prop.getLabel();
            QString displayName = propLabel.isEmpty() ? propName : QString("%1 (%2)").arg(propLabel).arg(propName);

            m_propertyCombo->addItem(displayName, propName);
        }
    }
    else
    {
        // Get properties from cached device
        QJsonArray properties = m_deviceCacheData["properties"].toArray();
        for (const QJsonValue &propValue : properties)
        {
            QJsonObject prop = propValue.toObject();
            QString propType = prop["type"].toString().toUpper();

            // Skip BLOB properties
            if (propType == "BLOB")
                continue;

            QString propName = prop["name"].toString();
            QString propLabel = prop["label"].toString();
            QString displayName = propLabel.isEmpty() ? propName : QString("%1 (%2)").arg(propLabel).arg(propName);

            m_propertyCombo->addItem(displayName, propName);
        }
    }

    if (m_propertyCombo->count() == 0)
    {
        m_propertyInfoLabel->setText(i18n("No compatible properties found"));
    }
    else
    {
        m_propertyInfoLabel->setText(i18n("%1 properties available", m_propertyCombo->count()));
    }
}

void PropertyTemplateBuilderDialog::onPropertySelected(int index)
{
    if (index < 0)
    {
        m_selectedProperty.clear();
        m_selectedPropertyType = PROP_UNKNOWN;
        return;
    }

    m_selectedProperty = m_propertyCombo->itemData(index).toString();

    // Get property details
    if (m_isIndiMode)
    {
        auto device = getDevice(m_selectedDevice);
        if (device)
        {
            INDI::Property prop = device->getProperty(m_selectedProperty.toUtf8());
            if (prop.isValid())
            {
                m_selectedPropertyType = getPropertyType(prop);

                IPerm permission = prop.getPermission();
                if (permission == IP_RO)
                    m_selectedPropertyPermission = "ro";
                else if (permission == IP_WO)
                    m_selectedPropertyPermission = "wo";
                else
                    m_selectedPropertyPermission = "rw";

                m_propertyTypeLabel->setText(propertyTypeToString(m_selectedPropertyType));
                m_propertyPermissionLabel->setText(m_selectedPropertyPermission.toUpper());
            }
        }
    }
    else
    {
        QJsonArray properties = m_deviceCacheData["properties"].toArray();
        for (const QJsonValue &propValue : properties)
        {
            QJsonObject prop = propValue.toObject();
            if (prop["name"].toString() == m_selectedProperty)
            {
                QString typeStr = prop["type"].toString().toUpper();
                if (typeStr == "NUMBER")
                    m_selectedPropertyType = PROP_NUMBER;
                else if (typeStr == "SWITCH")
                    m_selectedPropertyType = PROP_SWITCH;
                else if (typeStr == "TEXT")
                    m_selectedPropertyType = PROP_TEXT;
                else if (typeStr == "LIGHT")
                    m_selectedPropertyType = PROP_LIGHT;

                m_selectedPropertyPermission = prop["permission"].toString();

                m_propertyTypeLabel->setText(propertyTypeToString(m_selectedPropertyType));
                m_propertyPermissionLabel->setText(m_selectedPropertyPermission.toUpper());
                break;
            }
        }
    }
}

void PropertyTemplateBuilderDialog::populatePropertyElements()
{
    m_propertyElementCombo->clear();
    m_propertyElements.clear();

    if (m_selectedDevice.isEmpty() || m_selectedProperty.isEmpty())
    {
        return;
    }

    if (m_isIndiMode)
    {
        auto device = getDevice(m_selectedDevice);
        if (!device)
            return;

        INDI::Property prop = device->getProperty(m_selectedProperty.toUtf8());
        if (!prop.isValid())
            return;

        // Get elements based on property type
        switch (m_selectedPropertyType)
        {
            case PROP_NUMBER:
            {
                auto *nvp = prop.getNumber();
                if (nvp)
                {
                    for (int i = 0; i < nvp->count(); i++)
                    {
                        QString elemName = nvp->at(i)->name;
                        QString elemLabel = nvp->at(i)->label;
                        m_propertyElements[elemName] = elemLabel;
                        m_propertyElementCombo->addItem(elemLabel.isEmpty() ? elemName : elemLabel, elemName);
                    }
                }
                break;
            }
            case PROP_SWITCH:
            {
                auto *svp = prop.getSwitch();
                if (svp)
                {
                    for (int i = 0; i < svp->count(); i++)
                    {
                        QString elemName = svp->at(i)->name;
                        QString elemLabel = svp->at(i)->label;
                        m_propertyElements[elemName] = elemLabel;
                        m_propertyElementCombo->addItem(elemLabel.isEmpty() ? elemName : elemLabel, elemName);
                    }
                }
                break;
            }
            case PROP_TEXT:
            {
                auto *tvp = prop.getText();
                if (tvp)
                {
                    for (int i = 0; i < tvp->count(); i++)
                    {
                        QString elemName = tvp->at(i)->name;
                        QString elemLabel = tvp->at(i)->label;
                        m_propertyElements[elemName] = elemLabel;
                        m_propertyElementCombo->addItem(elemLabel.isEmpty() ? elemName : elemLabel, elemName);
                    }
                }
                break;
            }
            case PROP_LIGHT:
            {
                auto *lvp = prop.getLight();
                if (lvp)
                {
                    for (int i = 0; i < lvp->count(); i++)
                    {
                        QString elemName = lvp->at(i)->name;
                        QString elemLabel = lvp->at(i)->label;
                        m_propertyElements[elemName] = elemLabel;
                        m_propertyElementCombo->addItem(elemLabel.isEmpty() ? elemName : elemLabel, elemName);
                    }
                }
                break;
            }
            default:
                break;
        }
    }
    else
    {
        // Get elements from cached property
        QJsonArray properties = m_deviceCacheData["properties"].toArray();
        for (const QJsonValue &propValue : properties)
        {
            QJsonObject prop = propValue.toObject();
            if (prop["name"].toString() == m_selectedProperty)
            {
                QJsonArray elements = prop["elements"].toArray();
                for (const QJsonValue &elemValue : elements)
                {
                    QJsonObject elem = elemValue.toObject();
                    QString elemName = elem["name"].toString();
                    QString elemLabel = elem["label"].toString();
                    m_propertyElements[elemName] = elemLabel;
                    m_propertyElementCombo->addItem(elemLabel.isEmpty() ? elemName : elemLabel, elemName);
                }
                break;
            }
        }
    }

    // Build property-specific configuration UI
    switch (m_selectedPropertyType)
    {
        case PROP_NUMBER:
            buildNumberConfiguration();
            break;
        case PROP_SWITCH:
            buildSwitchConfiguration();
            break;
        case PROP_TEXT:
            buildTextConfiguration();
            break;
        case PROP_LIGHT:
            buildLightConfiguration();
            break;
        default:
            break;
    }

    // Update action checkboxes based on property permission
    if (m_selectedPropertyPermission == "ro")
    {
        m_includeSetAction->setChecked(false);
        m_includeSetAction->setEnabled(false);
        m_includeEvaluateAction->setChecked(true);
    }
    else
    {
        m_includeSetAction->setEnabled(true);
        m_includeSetAction->setChecked(true);
        m_includeEvaluateAction->setChecked(true);
    }
}

void PropertyTemplateBuilderDialog::buildNumberConfiguration()
{
    // Remove old property-specific widgets
    QLayout *layout = m_actionConfigWidget->layout();

    // Find and remove old property-specific groups (inserted after element group)
    for (int i = layout->count() - 1; i >= 0; i--)
    {
        QLayoutItem *item = layout->itemAt(i);
        if (item->widget() && item->widget()->objectName().startsWith("propConfig"))
        {
            QWidget *w = item->widget();
            layout->removeWidget(w);
            w->deleteLater();
        }
    }

    // Create NUMBER-specific configuration
    auto *setGroup = new QGroupBox(i18n("SET Action Configuration"), m_actionConfigWidget);
    setGroup->setObjectName("propConfigSet");
    auto *setLayout = new QFormLayout(setGroup);

    m_numberSetValue = new QDoubleSpinBox(setGroup);
    m_numberSetValue->setRange(-999999, 999999);
    m_numberSetValue->setDecimals(6);
    m_numberSetValue->setValue(0);
    setLayout->addRow(i18n("Value:"), m_numberSetValue);

    auto *evalGroup = new QGroupBox(i18n("EVALUATE Action Configuration"), m_actionConfigWidget);
    evalGroup->setObjectName("propConfigEval");
    auto *evalLayout = new QFormLayout(evalGroup);

    m_numberEvaluateCondition = new QComboBox(evalGroup);
    m_numberEvaluateCondition->addItem(i18n("Equals"), 0);
    m_numberEvaluateCondition->addItem(i18n("Less Than"), 1);
    m_numberEvaluateCondition->addItem(i18n("Greater Than"), 2);
    m_numberEvaluateCondition->addItem(i18n("Less or Equal"), 3);
    m_numberEvaluateCondition->addItem(i18n("Greater or Equal"), 4);
    m_numberEvaluateCondition->addItem(i18n("Not Equal"), 5);
    m_numberEvaluateCondition->addItem(i18n("Within Range"), 6);
    evalLayout->addRow(i18n("Condition:"), m_numberEvaluateCondition);

    m_numberEvaluateTarget = new QDoubleSpinBox(evalGroup);
    m_numberEvaluateTarget->setRange(-999999, 999999);
    m_numberEvaluateTarget->setDecimals(6);
    m_numberEvaluateTarget->setValue(0);
    evalLayout->addRow(i18n("Target:"), m_numberEvaluateTarget);

    m_numberEvaluateMargin = new QDoubleSpinBox(evalGroup);
    m_numberEvaluateMargin->setRange(0, 999999);
    m_numberEvaluateMargin->setDecimals(6);
    m_numberEvaluateMargin->setValue(0.1);
    evalLayout->addRow(i18n("Margin/Tolerance:"), m_numberEvaluateMargin);

    // Insert before stretch and settings group
    QVBoxLayout *vbox = qobject_cast<QVBoxLayout*>(layout);
    if (vbox)
    {
        vbox->insertWidget(vbox->count() - 2, setGroup);
        vbox->insertWidget(vbox->count() - 2, evalGroup);
    }
}

void PropertyTemplateBuilderDialog::buildSwitchConfiguration()
{
    // Remove old property-specific widgets
    QLayout *layout = m_actionConfigWidget->layout();

    for (int i = layout->count() - 1; i >= 0; i--)
    {
        QLayoutItem *item = layout->itemAt(i);
        if (item->widget() && item->widget()->objectName().startsWith("propConfig"))
        {
            QWidget *w = item->widget();
            layout->removeWidget(w);
            w->deleteLater();
        }
    }

    // Create SWITCH-specific configuration
    auto *setGroup = new QGroupBox(i18n("SET Action Configuration"), m_actionConfigWidget);
    setGroup->setObjectName("propConfigSet");
    auto *setLayout = new QFormLayout(setGroup);

    m_switchSetState = new QComboBox(setGroup);
    m_switchSetState->addItem(i18n("ON"), "ON");
    m_switchSetState->addItem(i18n("OFF"), "OFF");
    setLayout->addRow(i18n("State:"), m_switchSetState);

    auto *evalGroup = new QGroupBox(i18n("EVALUATE Action Configuration"), m_actionConfigWidget);
    evalGroup->setObjectName("propConfigEval");
    auto *evalLayout = new QFormLayout(evalGroup);

    m_switchEvaluateState = new QComboBox(evalGroup);
    m_switchEvaluateState->addItem(i18n("ON"), "ON");
    m_switchEvaluateState->addItem(i18n("OFF"), "OFF");
    evalLayout->addRow(i18n("Expected State:"), m_switchEvaluateState);

    QVBoxLayout *vbox = qobject_cast<QVBoxLayout*>(layout);
    if (vbox)
    {
        vbox->insertWidget(vbox->count() - 2, setGroup);
        vbox->insertWidget(vbox->count() - 2, evalGroup);
    }
}

void PropertyTemplateBuilderDialog::buildTextConfiguration()
{
    // Remove old property-specific widgets
    QLayout *layout = m_actionConfigWidget->layout();

    for (int i = layout->count() - 1; i >= 0; i--)
    {
        QLayoutItem *item = layout->itemAt(i);
        if (item->widget() && item->widget()->objectName().startsWith("propConfig"))
        {
            QWidget *w = item->widget();
            layout->removeWidget(w);
            w->deleteLater();
        }
    }

    // Create TEXT-specific configuration
    auto *setGroup = new QGroupBox(i18n("SET Action Configuration"), m_actionConfigWidget);
    setGroup->setObjectName("propConfigSet");
    auto *setLayout = new QFormLayout(setGroup);

    m_textSetValue = new QLineEdit(setGroup);
    setLayout->addRow(i18n("Value:"), m_textSetValue);

    auto *evalGroup = new QGroupBox(i18n("EVALUATE Action Configuration"), m_actionConfigWidget);
    evalGroup->setObjectName("propConfigEval");
    auto *evalLayout = new QFormLayout(evalGroup);

    m_textEvaluateCondition = new QComboBox(evalGroup);
    m_textEvaluateCondition->addItem(i18n("Equals"), 0);
    m_textEvaluateCondition->addItem(i18n("Contains"), 1);
    evalLayout->addRow(i18n("Condition:"), m_textEvaluateCondition);

    m_textEvaluateValue = new QLineEdit(evalGroup);
    evalLayout->addRow(i18n("Expected Value:"), m_textEvaluateValue);

    QVBoxLayout *vbox = qobject_cast<QVBoxLayout*>(layout);
    if (vbox)
    {
        vbox->insertWidget(vbox->count() - 2, setGroup);
        vbox->insertWidget(vbox->count() - 2, evalGroup);
    }
}

void PropertyTemplateBuilderDialog::buildLightConfiguration()
{
    // Remove old property-specific widgets
    QLayout *layout = m_actionConfigWidget->layout();

    for (int i = layout->count() - 1; i >= 0; i--)
    {
        QLayoutItem *item = layout->itemAt(i);
        if (item->widget() && item->widget()->objectName().startsWith("propConfig"))
        {
            QWidget *w = item->widget();
            layout->removeWidget(w);
            w->deleteLater();
        }
    }

    // LIGHT properties are read-only, so only EVALUATE action
    auto *evalGroup = new QGroupBox(i18n("EVALUATE Action Configuration"), m_actionConfigWidget);
    evalGroup->setObjectName("propConfigEval");
    auto *evalLayout = new QFormLayout(evalGroup);

    m_lightEvaluateState = new QComboBox(evalGroup);
    m_lightEvaluateState->addItem(i18n("Idle"), "Idle");
    m_lightEvaluateState->addItem(i18n("OK"), "Ok");
    m_lightEvaluateState->addItem(i18n("Busy"), "Busy");
    m_lightEvaluateState->addItem(i18n("Alert"), "Alert");
    evalLayout->addRow(i18n("Expected State:"), m_lightEvaluateState);

    QVBoxLayout *vbox = qobject_cast<QVBoxLayout*>(layout);
    if (vbox)
    {
        vbox->insertWidget(vbox->count() - 2, evalGroup);
    }
}

void PropertyTemplateBuilderDialog::onActionTypeChanged()
{
    // Validate that at least one action is selected
    if (!m_includeSetAction->isChecked() && !m_includeEvaluateAction->isChecked())
    {
        QMessageBox::warning(this, i18n("Invalid Configuration"),
                             i18n("At least one action type (SET or EVALUATE) must be selected."));
        m_includeEvaluateAction->setChecked(true);
    }
}

void PropertyTemplateBuilderDialog::onPropertyElementSelected(int index)
{
    Q_UNUSED(index);
    // Element selection changed - could update UI if needed
}

bool PropertyTemplateBuilderDialog::validateCurrentPage()
{
    switch (m_currentPage)
    {
        case PAGE_DEVICE_SELECTION:
            return validateDeviceSelection();
        case PAGE_PROPERTY_SELECTION:
            return validatePropertySelection();
        case PAGE_ACTION_CONFIGURATION:
            return validateActionConfiguration();
        case PAGE_METADATA:
            return validateMetadata();
        case PAGE_PREVIEW:
            return true;
        default:
            return true;
    }
}

bool PropertyTemplateBuilderDialog::validateDeviceSelection()
{
    if (m_selectedDevice.isEmpty())
    {
        QMessageBox::warning(this, i18n("Validation Error"),
                             i18n("Please select a device."));
        return false;
    }
    return true;
}

bool PropertyTemplateBuilderDialog::validatePropertySelection()
{
    if (m_selectedProperty.isEmpty())
    {
        QMessageBox::warning(this, i18n("Validation Error"),
                             i18n("Please select a property."));
        return false;
    }

    if (m_selectedPropertyType == PROP_UNKNOWN || m_selectedPropertyType == PROP_BLOB)
    {
        QMessageBox::warning(this, i18n("Validation Error"),
                             i18n("Selected property type is not supported."));
        return false;
    }

    return true;
}

bool PropertyTemplateBuilderDialog::validateActionConfiguration()
{
    if (!m_includeSetAction->isChecked() && !m_includeEvaluateAction->isChecked())
    {
        QMessageBox::warning(this, i18n("Validation Error"),
                             i18n("At least one action (SET or EVALUATE) must be selected."));
        return false;
    }

    if (m_propertyElementCombo->currentIndex() < 0)
    {
        QMessageBox::warning(this, i18n("Validation Error"),
                             i18n("Please select a property element."));
        return false;
    }

    return true;
}

bool PropertyTemplateBuilderDialog::validateMetadata()
{
    if (m_templateNameEdit->text().trimmed().isEmpty())
    {
        QMessageBox::warning(this, i18n("Validation Error"),
                             i18n("Please provide a template name."));
        return false;
    }

    if (m_categoryCombo->currentText().trimmed().isEmpty())
    {
        QMessageBox::warning(this, i18n("Validation Error"),
                             i18n("Please select or enter a category."));
        return false;
    }

    return true;
}

PropertyTemplateBuilderDialog::PropertyType PropertyTemplateBuilderDialog::getPropertyType(const INDI::Property &prop) const
{
    if (prop.getType() == INDI_NUMBER)
        return PROP_NUMBER;
    else if (prop.getType() == INDI_SWITCH)
        return PROP_SWITCH;
    else if (prop.getType() == INDI_TEXT)
        return PROP_TEXT;
    else if (prop.getType() == INDI_LIGHT)
        return PROP_LIGHT;
    else if (prop.getType() == INDI_BLOB)
        return PROP_BLOB;
    else
        return PROP_UNKNOWN;
}

QString PropertyTemplateBuilderDialog::propertyTypeToString(PropertyType type) const
{
    switch (type)
    {
        case PROP_NUMBER:
            return "NUMBER";
        case PROP_SWITCH:
            return "SWITCH";
        case PROP_TEXT:
            return "TEXT";
        case PROP_LIGHT:
            return "LIGHT";
        case PROP_BLOB:
            return "BLOB";
        default:
            return "UNKNOWN";
    }
}

QSharedPointer<ISD::GenericDevice> PropertyTemplateBuilderDialog::getDevice(const QString &deviceName) const
{
    const QList<QSharedPointer<ISD::GenericDevice>> devices = INDIListener::devices();
    for (const auto &device : devices)
    {
        if (device && device->getDeviceName() == deviceName)
        {
            return device;
        }
    }
    return QSharedPointer<ISD::GenericDevice>();
}

QJsonObject PropertyTemplateBuilderDialog::getDeviceFromProfile(const QString &deviceName) const
{
    // The device cache data should already be populated in onDeviceSelected
    // This method is a fallback if we need to search again
    if (m_deviceCacheData["device_name"].toString() == deviceName)
    {
        return m_deviceCacheData;
    }

    // Search through optical trains again if needed
    Ekos::Manager *manager = Ekos::Manager::Instance();
    if (!manager || !manager->schedulerModule() || !manager->schedulerModule()->moduleState())
    {
        return QJsonObject();
    }

    QString profileName = manager->schedulerModule()->moduleState()->currentProfile();
    if (profileName == i18n("Default"))
        profileName = manager->getCurrentProfile();

    if (profileName.isEmpty())
        return QJsonObject();

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
        return QJsonObject();

    QList<QVariantMap> opticalTrains;
    if (!KStarsData::Instance()->userdb()->GetOpticalTrains(profileID, opticalTrains))
        return QJsonObject();

    OpticalTrainManager *otm = OpticalTrainManager::Instance();
    if (!otm)
        return QJsonObject();

    for (const auto &train : opticalTrains)
    {
        int trainID = train["id"].toInt();
        QJsonArray trainDevices;

        if (otm->importTrainINDIProperties(trainDevices, trainID))
        {
            for (const QJsonValue &deviceValue : trainDevices)
            {
                QJsonObject deviceObj = deviceValue.toObject();
                if (deviceObj["device_name"].toString() == deviceName)
                {
                    return deviceObj;
                }
            }
        }
    }

    return QJsonObject();
}

QString PropertyTemplateBuilderDialog::getUniqueTemplateId()
{
    // In edit mode, reuse existing ID
    if (m_isEditMode && m_templateToEdit)
    {
        return m_templateToEdit->id();
    }

    // Generate unique ID using device and property name
    QString baseId = QString("%1_%2").arg(m_selectedDevice).arg(m_selectedProperty);
    baseId = baseId.toLower().replace(' ', '_').replace('-', '_');

    // Add UUID suffix to ensure uniqueness
    QString uuid = QUuid::createUuid().toString();
    uuid = uuid.mid(1, 8); // Take first 8 chars of UUID (without braces)

    return QString("%1_%2").arg(baseId).arg(uuid);
}

QString PropertyTemplateBuilderDialog::getDefaultTemplateName()
{
    QString propLabel = m_propertyElements.value(
                            m_propertyElementCombo->currentData().toString(),
                            m_selectedProperty);

    return QString("%1 - %2").arg(m_selectedDevice).arg(propLabel);
}

QString PropertyTemplateBuilderDialog::getUserTemplatesPath()
{
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QString templatesPath = dataPath + "/taskqueue/templates/user";

    QDir dir;
    if (!dir.exists(templatesPath))
    {
        dir.mkpath(templatesPath);
    }

    return templatesPath;
}

QJsonObject PropertyTemplateBuilderDialog::generateTemplateJson()
{
    QJsonObject templateJson;

    // Metadata
    templateJson["category"] = m_categoryCombo->currentText();
    templateJson["device_type"] = "GENERIC";
    templateJson["version"] = "1.0";

    // Templates array with single template
    QJsonArray templates;
    QJsonObject tmpl;

    tmpl["id"] = getUniqueTemplateId();
    tmpl["name"] = m_templateNameEdit->text();
    tmpl["description"] = m_templateDescEdit->toPlainText();
    tmpl["version"] = "1.0";
    tmpl["category"] = m_categoryCombo->currentText();

    // Mark as user template by including parent_template field
    // Use a special marker to indicate this is a custom property template
    tmpl["parent_template"] = "custom_property_template";

    // Supported interfaces
    QJsonArray interfaces;
    interfaces.append(static_cast<int>(m_deviceInterface));
    tmpl["supported_interfaces"] = interfaces;

    // Parameters (empty for now - could be extended later)
    tmpl["parameters"] = QJsonArray();

    // Actions
    tmpl["actions"] = generateActions();

    templates.append(tmpl);
    templateJson["templates"] = templates;

    return templateJson;
}

QJsonArray PropertyTemplateBuilderDialog::generateActions()
{
    QJsonArray actions;

    QString selectedElement = m_propertyElementCombo->currentData().toString();

    // Generate SET action if requested
    if (m_includeSetAction->isChecked() && m_includeSetAction->isEnabled())
    {
        actions.append(generateSetAction());
    }

    // Generate EVALUATE action if requested
    if (m_includeEvaluateAction->isChecked())
    {
        actions.append(generateEvaluateAction());
    }

    return actions;
}

QJsonObject PropertyTemplateBuilderDialog::generateSetAction()
{
    QJsonObject action;
    action["type"] = "SET";
    action["property"] = m_selectedProperty;
    action["element"] = m_propertyElementCombo->currentData().toString();
    action["timeout"] = m_timeoutSpin->value();
    action["retries"] = m_retriesSpin->value();
    action["failure_action"] = m_failureActionCombo->currentData().toInt();

    // Set value based on property type
    switch (m_selectedPropertyType)
    {
        case PROP_NUMBER:
            action["value"] = m_numberSetValue->value();
            break;
        case PROP_SWITCH:
            action["value"] = m_switchSetState->currentData().toString();
            break;
        case PROP_TEXT:
            action["value"] = m_textSetValue->text();
            break;
        default:
            action["value"] = "";
            break;
    }

    return action;
}

QJsonObject PropertyTemplateBuilderDialog::generateEvaluateAction()
{
    QJsonObject action;
    action["type"] = "EVALUATE";
    action["property"] = m_selectedProperty;
    action["element"] = m_propertyElementCombo->currentData().toString();
    action["timeout"] = m_timeoutSpin->value();
    action["retries"] = m_retriesSpin->value();
    action["failure_action"] = m_failureActionCombo->currentData().toInt();

    // Set property type (0=NUMBER, 1=TEXT, 2=SWITCH, 4=LIGHT)
    switch (m_selectedPropertyType)
    {
        case PROP_NUMBER:
            action["property_type"] = 0;
            action["condition"] = m_numberEvaluateCondition->currentData().toInt();
            action["target"] = m_numberEvaluateTarget->value();
            action["margin"] = m_numberEvaluateMargin->value();
            break;
        case PROP_TEXT:
            action["property_type"] = 1;
            action["condition"] = m_textEvaluateCondition->currentData().toInt();
            action["target"] = m_textEvaluateValue->text();
            break;
        case PROP_SWITCH:
            action["property_type"] = 2;
            action["condition"] = 0; // Equals
            action["target"] = m_switchEvaluateState->currentData().toString();
            break;
        case PROP_LIGHT:
            action["property_type"] = 4;
            action["condition"] = 0; // Equals
            action["target"] = m_lightEvaluateState->currentData().toString();
            break;
        default:
            break;
    }

    return action;
}

bool PropertyTemplateBuilderDialog::saveTemplate(const QJsonObject &templateJson)
{
    QString templatesPath = getUserTemplatesPath();
    QString filename = QString("%1.json").arg(getUniqueTemplateId());
    QString filepath = templatesPath + "/" + filename;

    QFile file(filepath);
    if (!file.open(QIODevice::WriteOnly))
    {
        QMessageBox::critical(this, i18n("Save Error"),
                              i18n("Failed to open file for writing: %1", filepath));
        return false;
    }

    QJsonDocument doc(templateJson);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    return true;
}

bool PropertyTemplateBuilderDialog::generateTemplate()
{
    // Generate the JSON
    QJsonObject templateJson = generateTemplateJson();

    // Save to file
    if (!saveTemplate(templateJson))
    {
        return false;
    }

    // Create TaskTemplate object from JSON
    if (m_generatedTemplate)
    {
        m_generatedTemplate->deleteLater();
        m_generatedTemplate = nullptr;
    }

    m_generatedTemplate = new TaskTemplate(this);

    // Load from the first template in the array
    QJsonArray templates = templateJson["templates"].toArray();
    if (!templates.isEmpty())
    {
        if (!m_generatedTemplate->loadFromJson(templates[0].toObject()))
        {
            QMessageBox::critical(this, i18n("Template Error"),
                                  i18n("Failed to create template object from JSON."));
            m_generatedTemplate->deleteLater();
            m_generatedTemplate = nullptr;
            return false;
        }

        m_generatedTemplate->setSystemTemplate(false);
    }

    return true;
}

void PropertyTemplateBuilderDialog::loadTemplateForEditing(TaskTemplate *tmpl)
{
    if (!tmpl)
        return;

    // Extract action information from template's JSON
    QJsonArray actions = tmpl->actionDefinitions();
    if (actions.isEmpty())
        return;

    // Parse first action to get property info
    QJsonObject firstAction = actions[0].toObject();
    m_selectedProperty = firstAction["property"].toString();
    QString selectedElement = firstAction["element"].toString();

    // Determine property type and load action values
    bool hasSetAction = false;
    bool hasEvaluateAction = false;

    for (const QJsonValue &actionValue : actions)
    {
        QJsonObject action = actionValue.toObject();
        QString actionType = action["type"].toString();

        if (actionType == "SET")
        {
            hasSetAction = true;

            // Load SET action values
            m_timeoutSpin->setValue(action["timeout"].toInt(30));
            m_retriesSpin->setValue(action["retries"].toInt(2));
            m_failureActionCombo->setCurrentIndex(
                m_failureActionCombo->findData(action["failure_action"].toInt(0))
            );

            // Load value based on type (determine property type from this)
            QJsonValue value = action["value"];
            if (value.isDouble())
            {
                m_selectedPropertyType = PROP_NUMBER;
            }
            else if (value.isString())
            {
                QString strVal = value.toString();
                if (strVal == "ON" || strVal == "OFF")
                {
                    m_selectedPropertyType = PROP_SWITCH;
                }
                else
                {
                    m_selectedPropertyType = PROP_TEXT;
                }
            }
        }
        else if (actionType == "EVALUATE")
        {
            hasEvaluateAction = true;

            // Load EVALUATE action values to determine property type if not already set
            int propertyType = action["property_type"].toInt();

            switch (propertyType)
            {
                case 0: // NUMBER
                    m_selectedPropertyType = PROP_NUMBER;
                    break;

                case 1: // TEXT
                    m_selectedPropertyType = PROP_TEXT;
                    break;

                case 2: // SWITCH
                    m_selectedPropertyType = PROP_SWITCH;
                    break;

                case 4: // LIGHT
                    m_selectedPropertyType = PROP_LIGHT;
                    break;
            }
        }
    }

    // Build the appropriate property configuration UI first
    switch (m_selectedPropertyType)
    {
        case PROP_NUMBER:
            buildNumberConfiguration();
            break;
        case PROP_SWITCH:
            buildSwitchConfiguration();
            break;
        case PROP_TEXT:
            buildTextConfiguration();
            break;
        case PROP_LIGHT:
            buildLightConfiguration();
            break;
        default:
            break;
    }

    // Store action values before building UI
    QJsonObject setActionData;
    QJsonObject evaluateActionData;

    for (const QJsonValue &actionValue : actions)
    {
        QJsonObject action = actionValue.toObject();
        QString actionType = action["type"].toString();

        if (actionType == "SET")
        {
            setActionData = action;
        }
        else if (actionType == "EVALUATE")
        {
            evaluateActionData = action;
        }
    }

    // Now populate values from stored action data after UI is built
    if (!setActionData.isEmpty())
    {
        QJsonValue value = setActionData["value"];

        switch (m_selectedPropertyType)
        {
            case PROP_NUMBER:
                if (m_numberSetValue)
                    m_numberSetValue->setValue(value.toDouble());
                break;

            case PROP_SWITCH:
                if (m_switchSetState)
                    m_switchSetState->setCurrentIndex(
                        m_switchSetState->findData(value.toString())
                    );
                break;

            case PROP_TEXT:
                if (m_textSetValue)
                    m_textSetValue->setText(value.toString());
                break;

            default:
                break;
        }
    }

    if (!evaluateActionData.isEmpty())
    {
        int propertyType = evaluateActionData["property_type"].toInt();

        switch (propertyType)
        {
            case 0: // NUMBER
                if (m_numberEvaluateCondition)
                    m_numberEvaluateCondition->setCurrentIndex(
                        m_numberEvaluateCondition->findData(evaluateActionData["condition"].toInt())
                    );
                if (m_numberEvaluateTarget)
                    m_numberEvaluateTarget->setValue(evaluateActionData["target"].toDouble());
                if (m_numberEvaluateMargin)
                    m_numberEvaluateMargin->setValue(evaluateActionData["margin"].toDouble(0.1));
                break;

            case 1: // TEXT
                if (m_textEvaluateCondition)
                    m_textEvaluateCondition->setCurrentIndex(
                        m_textEvaluateCondition->findData(evaluateActionData["condition"].toInt())
                    );
                if (m_textEvaluateValue)
                    m_textEvaluateValue->setText(evaluateActionData["target"].toString());
                break;

            case 2: // SWITCH
                if (m_switchEvaluateState)
                    m_switchEvaluateState->setCurrentIndex(
                        m_switchEvaluateState->findData(evaluateActionData["target"].toString())
                    );
                break;

            case 4: // LIGHT
                if (m_lightEvaluateState)
                    m_lightEvaluateState->setCurrentIndex(
                        m_lightEvaluateState->findData(evaluateActionData["target"].toString())
                    );
                break;
        }
    }

    // Set action checkboxes
    if (m_includeSetAction)
        m_includeSetAction->setChecked(hasSetAction);
    if (m_includeEvaluateAction)
        m_includeEvaluateAction->setChecked(hasEvaluateAction);

    // Populate element combo (single element from action)
    if (m_propertyElementCombo)
    {
        m_propertyElementCombo->clear();
        m_propertyElementCombo->addItem(selectedElement, selectedElement);
        m_propertyElements[selectedElement] = selectedElement;
    }

    // Load metadata
    m_templateNameEdit->setText(tmpl->name());
    m_templateDescEdit->setPlainText(tmpl->description());
    m_categoryCombo->setCurrentText(tmpl->category());

    // Jump to action configuration page (PAGE_ACTION_CONFIGURATION)
    goToPage(PAGE_ACTION_CONFIGURATION);
}

void PropertyTemplateBuilderDialog::accept()
{
    // In edit mode, skip device/property validation since they're pre-filled
    if (m_isEditMode)
    {
        if (!validateActionConfiguration() || !validateMetadata())
        {
            return;
        }
    }
    else
    {
        // Validate all pages in create mode
        if (!validateDeviceSelection() || !validatePropertySelection() ||
                !validateActionConfiguration() || !validateMetadata())
        {
            return;
        }
    }

    // Generate the template
    if (!generateTemplate())
    {
        return;
    }

    QDialog::accept();
}

} // namespace Ekos
