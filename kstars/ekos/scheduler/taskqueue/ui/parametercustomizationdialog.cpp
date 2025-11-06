/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "parametercustomizationdialog.h"
#include "../tasktemplate.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QDialogButtonBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QMessageBox>
#include <QFileDialog>
#include <KLocalizedString>

namespace Ekos
{

ParameterCustomizationDialog::ParameterCustomizationDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(i18n("Customize Parameters"));
    setupUI();
}

ParameterCustomizationDialog::~ParameterCustomizationDialog()
{
}

void ParameterCustomizationDialog::setupUI()
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

    // Device label
    m_deviceLabel = new QLabel(this);
    mainLayout->addWidget(m_deviceLabel);

    // Scroll area for parameters
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);

    m_formWidget = new QWidget(m_scrollArea);
    m_formLayout = new QFormLayout(m_formWidget);
    m_formLayout->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    m_formLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_scrollArea->setWidget(m_formWidget);
    mainLayout->addWidget(m_scrollArea);

    // Button box
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okButton = buttonBox->button(QDialogButtonBox::Ok);
    m_cancelButton = buttonBox->button(QDialogButtonBox::Cancel);

    mainLayout->addWidget(buttonBox);

    // Connect signals
    connect(buttonBox, &QDialogButtonBox::accepted, this, &ParameterCustomizationDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    setMinimumSize(500, 400);
}

void ParameterCustomizationDialog::setTemplate(TaskTemplate *tmpl)
{
    m_template = tmpl;

    if (m_template)
    {
        m_titleLabel->setText(m_template->name());
        populateParameters();
    }
}

void ParameterCustomizationDialog::setDeviceName(const QString &deviceName)
{
    m_deviceName = deviceName;
    m_deviceLabel->setText(i18n("Device: %1", deviceName));
}

void ParameterCustomizationDialog::populateParameters()
{
    if (!m_template)
        return;

    // Clear existing widgets
    while (m_formLayout->rowCount() > 0)
    {
        m_formLayout->removeRow(0);
    }
    m_parameterWidgets.clear();

    // Create widget for each parameter
    QList<TaskTemplate::Parameter> params = m_template->parameters();

    for (const TaskTemplate::Parameter &param : params)
    {
        QWidget *widget = createParameterWidget(param);
        if (widget)
        {
            // Create label with description
            QString labelText = param.description.isEmpty() ? param.name : param.description;
            if (!param.unit.isEmpty())
            {
                labelText += QString(" (%1)").arg(param.unit);
            }

            QLabel *label = new QLabel(labelText, m_formWidget);
            label->setWordWrap(true);

            m_formLayout->addRow(label, widget);
            m_parameterWidgets[param.name] = widget;
        }
    }
}

QWidget* ParameterCustomizationDialog::createParameterWidget(const TaskTemplate::Parameter &param)
{
    QWidget *widget = nullptr;

    if (param.type == "number")
    {
        // Check if it's a floating point or integer based on default value
        bool isDouble = param.defaultValue.type() == QVariant::Double ||
                        (param.step.isValid() && param.step.toDouble() != param.step.toInt());

        if (isDouble)
        {
            auto *spinBox = new QDoubleSpinBox(m_formWidget);
            spinBox->setFixedWidth(150);  // Fixed width for consistent UI

            if (param.minValue.isValid())
                spinBox->setMinimum(param.minValue.toDouble());
            else
                spinBox->setMinimum(-999999.0);

            if (param.maxValue.isValid())
                spinBox->setMaximum(param.maxValue.toDouble());
            else
                spinBox->setMaximum(999999.0);

            if (param.step.isValid())
                spinBox->setSingleStep(param.step.toDouble());

            spinBox->setValue(param.defaultValue.toDouble());

            widget = spinBox;
        }
        else
        {
            auto *spinBox = new QSpinBox(m_formWidget);
            spinBox->setFixedWidth(150);  // Fixed width for consistent UI

            if (param.minValue.isValid())
                spinBox->setMinimum(param.minValue.toInt());
            else
                spinBox->setMinimum(-999999);

            if (param.maxValue.isValid())
                spinBox->setMaximum(param.maxValue.toInt());
            else
                spinBox->setMaximum(999999);

            if (param.step.isValid())
                spinBox->setSingleStep(param.step.toInt());

            spinBox->setValue(param.defaultValue.toInt());

            widget = spinBox;
        }
    }
    else if (param.type == "text")
    {
        auto *lineEdit = new QLineEdit(m_formWidget);
        lineEdit->setMinimumWidth(200);  // Minimum width for text fields
        lineEdit->setText(param.defaultValue.toString());
        widget = lineEdit;
    }
    else if (param.type == "file")
    {
        // Create a container widget with line edit + browse button
        auto *container = new QWidget(m_formWidget);
        auto *layout = new QHBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);
        
        auto *lineEdit = new QLineEdit(container);
        lineEdit->setMinimumWidth(250);
        lineEdit->setText(param.defaultValue.toString());
        lineEdit->setObjectName("filePathEdit");  // For easy retrieval later
        
        auto *browseButton = new QPushButton(i18n("Browse..."), container);
        connect(browseButton, &QPushButton::clicked, this, [lineEdit, this]() {
            QString fileName = QFileDialog::getOpenFileName(
                this,
                i18n("Select File"),
                lineEdit->text().isEmpty() ? QDir::homePath() : lineEdit->text(),
                i18n("All Files (*)")
            );
            if (!fileName.isEmpty())
            {
                lineEdit->setText(fileName);
            }
        });
        
        layout->addWidget(lineEdit);
        layout->addWidget(browseButton);
        
        widget = container;
    }
    else if (param.type == "boolean")
    {
        auto *checkBox = new QCheckBox(m_formWidget);
        checkBox->setChecked(param.defaultValue.toBool());
        widget = checkBox;
    }

    return widget;
}

void ParameterCustomizationDialog::accept()
{
    // Collect parameter values from widgets
    m_parameterValues.clear();

    QList<TaskTemplate::Parameter> params = m_template->parameters();

    for (const TaskTemplate::Parameter &param : params)
    {
        QWidget *widget = m_parameterWidgets.value(param.name);
        if (!widget)
            continue;

        QVariant value;

        if (param.type == "number")
        {
            if (auto *spinBox = qobject_cast<QSpinBox * >(widget))
            {
                value = spinBox->value();
            }
            else if (auto *doubleSpinBox = qobject_cast<QDoubleSpinBox * >(widget))
            {
                value = doubleSpinBox->value();
            }
        }
        else if (param.type == "text")
        {
            if (auto *lineEdit = qobject_cast<QLineEdit * >(widget))
            {
                value = lineEdit->text();
            }
        }
        else if (param.type == "file")
        {
            // For file type, the widget is a container with a line edit inside
            if (auto *lineEdit = widget->findChild<QLineEdit *>("filePathEdit"))
            {
                value = lineEdit->text();
            }
        }
        else if (param.type == "boolean")
        {
            if (auto *checkBox = qobject_cast<QCheckBox * >(widget))
            {
                value = checkBox->isChecked();
            }
        }

        if (value.isValid())
        {
            m_parameterValues[param.name] = value;
        }
    }

    // Validate parameters
    QString errorMsg;
    if (!validateParameters(errorMsg))
    {
        QMessageBox::warning(this, i18n("Invalid Parameters"), errorMsg);
        return;
    }

    QDialog::accept();
}

bool ParameterCustomizationDialog::validateParameters(QString &errorMsg)
{
    if (!m_template)
    {
        errorMsg = i18n("No template specified");
        return false;
    }

    // Use template's validation
    return m_template->validateParameters(m_parameterValues, errorMsg);
}

} // namespace Ekos
