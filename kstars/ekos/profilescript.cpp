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

    m_StoppingDelaySpin = new QSpinBox(this);
    m_StoppingDelaySpin->setToolTip(i18n("Delay this many seconds after executing stopping driver script before stopping driver."));
    m_StoppingDelaySpin->setRange(0, 300);
    m_StoppingDelaySpin->setSingleStep(5);
    connect(m_StoppingDelaySpin, &QSpinBox::editingFinished, this, [this]()
            {
                m_StoppingDelay = m_StoppingDelaySpin->value();
            });

    m_StoppedDelaySpin = new QSpinBox(this);
    m_StoppedDelaySpin->setToolTip(i18n("Delay this many seconds after driver stopped before executing stopped driver script."));
    m_StoppedDelaySpin->setRange(0, 300);
    m_StoppedDelaySpin->setSingleStep(5);
    connect(m_StoppedDelaySpin, &QSpinBox::editingFinished, this, [this]()
            {
                m_StoppedDelay = m_StoppedDelaySpin->value();
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

    m_StoppingScriptEdit = new QLineEdit(this);
    m_StoppingScriptEdit->setMinimumWidth(100);
    m_StoppingScriptEdit->setToolTip(i18n("Select script to execute before stopping the driver"));
    m_StoppingScriptEdit->setClearButtonEnabled(true);
    connect(m_StoppingScriptEdit, &QLineEdit::editingFinished, this, [this]()
            {
                m_StoppingScript = m_StoppingScriptEdit->text();
            });

    m_StoppedScriptEdit = new QLineEdit(this);
    m_StoppedScriptEdit->setMinimumWidth(100);
    m_StoppedScriptEdit->setToolTip(i18n("Select script to execute after stopping the driver"));
    m_StoppedScriptEdit->setClearButtonEnabled(true);
    connect(m_StoppedScriptEdit, &QLineEdit::editingFinished, this, [this]()
            {
                m_StoppedScript = m_StoppedScriptEdit->text();
            });

    m_PreScriptB = new QPushButton(this);
    m_PreScriptB->setIcon(QIcon::fromTheme("document-open"));
    connect(m_PreScriptB, &QPushButton::clicked, this, &ProfileScript::selectPreScript);

    m_PostScriptB = new QPushButton(this);
    m_PostScriptB->setIcon(QIcon::fromTheme("document-open"));
    connect(m_PostScriptB, &QPushButton::clicked, this, &ProfileScript::selectPostScript);

    m_StoppingScriptB = new QPushButton(this);
    m_StoppingScriptB->setIcon(QIcon::fromTheme("document-open"));
    connect(m_StoppingScriptB, &QPushButton::clicked, this, &ProfileScript::selectStoppingScript);

    m_StoppedScriptB = new QPushButton(this);
    m_StoppedScriptB->setIcon(QIcon::fromTheme("document-open"));
    connect(m_StoppedScriptB, &QPushButton::clicked, this, &ProfileScript::selectStoppedScript);

    m_RemoveB = new QPushButton(this);
    m_RemoveB->setIcon(QIcon::fromTheme("list-remove"));
    connect(m_RemoveB, &QPushButton::clicked, this, &ProfileScript::removedRequested);

    // Add widgets to layout
    mainLayout->addWidget(m_DriverCombo);
    mainLayout->addWidget(m_PreDelaySpin);
    mainLayout->addWidget(m_PreScriptEdit);
    mainLayout->addWidget(m_PreScriptB);
    mainLayout->addWidget(m_PostDelaySpin);
    mainLayout->addWidget(m_PostScriptEdit);
    mainLayout->addWidget(m_PostScriptB);
    mainLayout->addWidget(m_StoppingDelaySpin);
    mainLayout->addWidget(m_StoppingScriptEdit);
    mainLayout->addWidget(m_StoppingScriptB);
    mainLayout->addWidget(m_StoppedDelaySpin);
    mainLayout->addWidget(m_StoppedScriptEdit);
    mainLayout->addWidget(m_StoppedScriptB);
    mainLayout->addWidget(m_RemoveB);
}

void ProfileScript::syncGUI()
{
    m_DriverCombo->setCurrentText(m_Driver);
    m_PreDelaySpin->setValue(m_PreDelay);
    m_PreScriptEdit->setText(m_PreScript);
    m_PostDelaySpin->setValue(m_PostDelay);
    m_PostScriptEdit->setText(m_PostScript);
    m_StoppingDelaySpin->setValue(m_StoppingDelay);
    m_StoppingScriptEdit->setText(m_StoppingScript);
    m_StoppedDelaySpin->setValue(m_StoppedDelay);
    m_StoppedScriptEdit->setText(m_StoppedScript);
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

void ProfileScript::selectStoppingScript()
{
    m_StoppingScript = QFileDialog::getOpenFileName(this, i18nc("@title:window", "Select Pre Driver Stop Script"));
    m_StoppingScriptEdit->setText(m_StoppingScript);
}

void ProfileScript::selectStoppedScript()
{
    m_StoppedScript = QFileDialog::getOpenFileName(this, i18nc("@title:window", "Select Post Driver Stop Script"));
    m_StoppedScriptEdit->setText(m_StoppedScript);
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
        {"StoppingDelay", static_cast<int>(m_StoppingDelay)},
        {"StoppingScript", m_StoppingScript},
        {"StoppedDelay", static_cast<int>(m_StoppedDelay)},
        {"StoppedScript", m_StoppedScript},
    };

    return settings;
}
