/*
    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "profilescript.h"

#include <QHBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QFileDialog>

#include <KLocalizedString>

ProfileScript::ProfileScript(QWidget *parent) : QWidget(parent)
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);

    m_DriverCombo = new QComboBox(this);
    m_DriverCombo->setEditable(true);
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    connect(m_DriverCombo, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::activated),
            this, [this](const QString & value)
    {
        m_Driver = value;
    });
#else
    connect(m_DriverCombo, &QComboBox::textActivated, this, [this](const QString & value)
    {
        m_Driver = value;
    });
#endif

    m_PreDelaySpin = new QSpinBox(this);
    m_PreDelaySpin->setToolTip(i18n("Delay this many seconds before executing pre driver script."));
    m_PreDelaySpin->setRange(0, 300);
    m_PreDelaySpin->setSingleStep(5);
    connect(m_PreDelaySpin, &QSpinBox::editingFinished, this, [this]()
    {
        m_PreDelay = m_PreDelaySpin->value();
    });

    m_PostDelaySpin = new QSpinBox(this);
    m_PostDelaySpin->setToolTip(i18n("Delay this many seconds after driver startup before executing post driver script."));
    m_PostDelaySpin->setRange(0, 300);
    m_PostDelaySpin->setSingleStep(5);
    connect(m_PostDelaySpin, &QSpinBox::editingFinished, this, [this]()
    {
        m_PostDelay = m_PostDelaySpin->value();
    });

    m_PreScriptEdit = new QLineEdit(this);
    m_PreScriptEdit->setMinimumWidth(100);
    m_PreScriptEdit->setToolTip(i18n("Select script to execute before starting the driver"));
    m_PreScriptEdit->setClearButtonEnabled(true);
    connect(m_PreScriptEdit, &QLineEdit::editingFinished, this, [this]()
    {
        m_PreScript = m_PreScriptEdit->text();
    });

    m_PostScriptEdit = new QLineEdit(this);
    m_PostScriptEdit->setMinimumWidth(100);
    m_PostScriptEdit->setToolTip(i18n("Select script to execute after starting the driver"));
    m_PostScriptEdit->setClearButtonEnabled(true);
    connect(m_PostScriptEdit, &QLineEdit::editingFinished, this, [this]()
    {
        m_PostScript = m_PostScriptEdit->text();
    });

    m_PreScriptB = new QPushButton(this);
    m_PreScriptB->setIcon(QIcon::fromTheme("document-open"));
    connect(m_PreScriptB, &QPushButton::clicked, this, &ProfileScript::selectPreScript);

    m_PostScriptB = new QPushButton(this);
    m_PostScriptB->setIcon(QIcon::fromTheme("document-open"));
    connect(m_PostScriptB, &QPushButton::clicked, this, &ProfileScript::selectPostScript);

    m_RemoveB = new QPushButton(this);
    m_RemoveB->setIcon(QIcon::fromTheme("list-remove"));
    connect(m_RemoveB, &QPushButton::clicked, this, &ProfileScript::removedRequested);

    // Add widgets to layout
    mainLayout->addWidget(m_PreDelaySpin);
    mainLayout->addWidget(m_PreScriptEdit);
    mainLayout->addWidget(m_PreScriptB);
    mainLayout->addWidget(m_DriverCombo);
    mainLayout->addWidget(m_PostDelaySpin);
    mainLayout->addWidget(m_PostScriptEdit);
    mainLayout->addWidget(m_PostScriptB);
    mainLayout->addWidget(m_RemoveB);
}

void ProfileScript::syncGUI()
{
    m_PreDelaySpin->setValue(m_PreDelay);
    m_PreScriptEdit->setText(m_PreScript);
    m_DriverCombo->setCurrentText(m_Driver);
    m_PostDelaySpin->setValue(m_PostDelay);
    m_PostScriptEdit->setText(m_PostScript);
}

void ProfileScript::setDriverList(const QStringList &value)
{
    m_DriverCombo->clear();
    m_DriverCombo->addItems(value);
    if (m_Driver.isEmpty())
        m_Driver = value.first();
}

void ProfileScript::selectPreScript()
{
    m_PreScript = QFileDialog::getOpenFileName(this, i18nc("@title:window", "Select Pre Driver Startup Script"));
    m_PreScriptEdit->setText(m_PreScript);
}

void ProfileScript::selectPostScript()
{
    m_PostScript = QFileDialog::getOpenFileName(this, i18nc("@title:window", "Select Post Driver Startup Script"));
    m_PostScriptEdit->setText(m_PostScript);
}

QJsonObject ProfileScript::toJSON() const
{
    QJsonObject settings =
    {
        {"Driver", m_Driver},
        {"PreDelay", static_cast<int>(m_PreDelay)},
        {"PreScript", m_PreScript},
        {"PostDelay", static_cast<int>(m_PostDelay)},
        {"PostScript", m_PostScript},
    };

    return settings;
}
