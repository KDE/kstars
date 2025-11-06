/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QSharedPointer>

class QStackedWidget;
class QListWidget;
class QListWidgetItem;
class QComboBox;
class QPushButton;
class QLabel;
class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;
class QTextEdit;
class QCheckBox;
class QRadioButton;
class QButtonGroup;
class QGroupBox;

namespace ISD
{
class GenericDevice;
}

namespace INDI
{
class Property;
}

namespace Ekos
{

class TaskTemplate;

/**
 * @class PropertyTemplateBuilderDialog
 * @brief Wizard dialog for creating custom templates for device properties
 *
 * This dialog provides a multi-stage wizard interface for creating custom
 * task queue templates that can SET and/or EVALUATE any INDI device property.
 * Supports NUMBER, SWITCH, TEXT, and LIGHT property types.
 */
class PropertyTemplateBuilderDialog : public QDialog
{
        Q_OBJECT

    public:
        explicit PropertyTemplateBuilderDialog(QWidget *parent = nullptr);
        explicit PropertyTemplateBuilderDialog(TaskTemplate *existingTemplate, QWidget *parent = nullptr);
        ~PropertyTemplateBuilderDialog() override;

        /**
         * @brief Get the generated template
         * @return Generated TaskTemplate, or nullptr if not completed
         */
        TaskTemplate* generatedTemplate() const
        {
            return m_generatedTemplate;
        }

    protected:
        void accept() override;

    private slots:
        void onDeviceSelected(int index);
        void onPropertySelected(int index);
        void onActionTypeChanged();
        void onPropertyElementSelected(int index);
        void onNextClicked();
        void onBackClicked();
        void onPreviewClicked();

    private:
        // Setup methods
        void setupUI();
        void setupDeviceSelectionPage();
        void setupPropertySelectionPage();
        void setupActionConfigurationPage();
        void setupMetadataPage();
        void setupPreviewPage();

        // Navigation
        void updateNavigationButtons();
        void goToPage(int pageIndex);

        // Device and property discovery
        void populateDevices();
        void populateProperties();
        void populatePropertyElements();

        // Property type detection
        enum PropertyType
        {
            PROP_NUMBER,
            PROP_SWITCH,
            PROP_TEXT,
            PROP_LIGHT,
            PROP_BLOB,
            PROP_UNKNOWN
        };
        PropertyType getPropertyType(const INDI::Property &prop) const;
        QString propertyTypeToString(PropertyType type) const;

        // Configuration UI builders
        void buildNumberConfiguration();
        void buildSwitchConfiguration();
        void buildTextConfiguration();
        void buildLightConfiguration();

        // Validation
        bool validateCurrentPage();
        bool validateDeviceSelection();
        bool validatePropertySelection();
        bool validateActionConfiguration();
        bool validateMetadata();

        // Template generation
        bool generateTemplate();
        QJsonObject generateTemplateJson();
        QJsonArray generateActions();
        QJsonObject generateSetAction();
        QJsonObject generateEvaluateAction();

        // Helper methods
        QString getUniqueTemplateId();
        QString getDefaultTemplateName();
        QString getUserTemplatesPath();
        bool saveTemplate(const QJsonObject &templateJson);
        QSharedPointer<ISD::GenericDevice> getDevice(const QString &deviceName) const;
        QJsonObject getDeviceFromProfile(const QString &deviceName) const;
        
        // Edit mode
        void loadTemplateForEditing(TaskTemplate *tmpl);

        // Pages
        enum Page
        {
            PAGE_DEVICE_SELECTION = 0,
            PAGE_PROPERTY_SELECTION,
            PAGE_ACTION_CONFIGURATION,
            PAGE_METADATA,
            PAGE_PREVIEW
        };

        // UI Widgets - Main
        QStackedWidget *m_pageStack = nullptr;
        QPushButton *m_nextButton = nullptr;
        QPushButton *m_backButton = nullptr;
        QPushButton *m_saveButton = nullptr;
        QPushButton *m_cancelButton = nullptr;
        QLabel *m_pageTitle = nullptr;
        QLabel *m_pageDescription = nullptr;

        // Page 1: Device Selection
        QComboBox *m_deviceCombo = nullptr;
        QLabel *m_deviceInfoLabel = nullptr;

        // Page 2: Property Selection
        QComboBox *m_propertyCombo = nullptr;
        QLabel *m_propertyInfoLabel = nullptr;
        QLabel *m_propertyTypeLabel = nullptr;
        QLabel *m_propertyPermissionLabel = nullptr;

        // Page 3: Action Configuration
        QWidget *m_actionConfigWidget = nullptr;
        QCheckBox *m_includeSetAction = nullptr;
        QCheckBox *m_includeEvaluateAction = nullptr;
        QComboBox *m_propertyElementCombo = nullptr;

        // Number-specific
        QDoubleSpinBox *m_numberSetValue = nullptr;
        QComboBox *m_numberEvaluateCondition = nullptr;
        QDoubleSpinBox *m_numberEvaluateTarget = nullptr;
        QDoubleSpinBox *m_numberEvaluateMargin = nullptr;

        // Switch-specific
        QComboBox *m_switchSetState = nullptr;
        QComboBox *m_switchEvaluateState = nullptr;

        // Text-specific
        QLineEdit *m_textSetValue = nullptr;
        QComboBox *m_textEvaluateCondition = nullptr;
        QLineEdit *m_textEvaluateValue = nullptr;

        // Light-specific
        QComboBox *m_lightEvaluateState = nullptr;

        // Common action settings
        QSpinBox *m_timeoutSpin = nullptr;
        QSpinBox *m_retriesSpin = nullptr;
        QComboBox *m_failureActionCombo = nullptr;

        // Page 4: Metadata
        QLineEdit *m_templateNameEdit = nullptr;
        QTextEdit *m_templateDescEdit = nullptr;
        QComboBox *m_categoryCombo = nullptr;

        // Page 5: Preview
        QTextEdit *m_previewText = nullptr;

        // State
        int m_currentPage = PAGE_DEVICE_SELECTION;
        QString m_selectedDevice;
        QString m_selectedProperty;
        PropertyType m_selectedPropertyType = PROP_UNKNOWN;
        QString m_selectedPropertyPermission;
        uint32_t m_deviceInterface = 0;
        QMap<QString, QString> m_propertyElements; // element name -> description
        bool m_isIndiMode = false; // true if using live INDI, false if using profile cache
        TaskTemplate *m_generatedTemplate = nullptr;
        QJsonObject m_deviceCacheData; // Cached device data for planning mode
        
        // Edit mode state
        TaskTemplate *m_templateToEdit = nullptr;
        bool m_isEditMode = false;
};

} // namespace Ekos
