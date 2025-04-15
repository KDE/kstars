/*
    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "profilescriptdialog.h"
#include "profilescript.h"

#include <QHBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QJsonDocument>
#include <QLabel>

#include <KLocalizedString>

ProfileScriptDialog::ProfileScriptDialog(const QStringList &drivers, const QByteArray &settings,
        QWidget *parent) : QDialog(parent)
{
    setWindowTitle(i18n("Profile Scripts Editor"));

    m_DriversList = drivers;
    m_MainLayout = new QVBoxLayout(this);

    QHBoxLayout *labelLayout = new QHBoxLayout;
    m_DriverLabel = new QLabel(i18n("Driver"), this);
    m_PreLabel = new QLabel(i18n("Pre start"), this);
    m_PostLabel = new QLabel(i18n("Post start"), this);
    m_PreStopLabel = new QLabel(i18n("Pre stop"), this);
    m_PostStopLabel = new QLabel(i18n("Post stop"), this);

    labelLayout->addWidget(m_DriverLabel);
    labelLayout->addWidget(m_PreLabel);
    labelLayout->addWidget(m_PostLabel);
    labelLayout->addWidget(m_PreStopLabel);
    labelLayout->addWidget(m_PostStopLabel);
    m_MainLayout->insertLayout(m_MainLayout->count() - 1, labelLayout);

    m_ButtonBox = new QDialogButtonBox(Qt::Horizontal, this);
    m_ButtonBox->addButton(QDialogButtonBox::Save);
    connect(m_ButtonBox, &QDialogButtonBox::accepted, this, &ProfileScriptDialog::generateSettings);
    connect(m_ButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_AddRuleB = new QPushButton(i18n("Add Rule"), this);
    m_AddRuleB->setIcon(QIcon::fromTheme("list-add"));
    connect(m_AddRuleB, &QPushButton::clicked, this, &ProfileScriptDialog::addRule);

    QHBoxLayout *bottomLayout = new QHBoxLayout;
    bottomLayout->addWidget(m_AddRuleB);
    bottomLayout->addStretch();
    bottomLayout->addWidget(m_ButtonBox);

    m_MainLayout->addItem(bottomLayout);
    m_MainLayout->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::MinimumExpanding));

    if (!settings.isEmpty())
        parseSettings(settings);
}

void ProfileScriptDialog::addRule()
{
    if (m_ProfileScriptWidgets.count() == m_DriversList.count())
        return;

    QStringList remainingDrivers = m_DriversList;
    for (auto &oneRule : m_ProfileScriptWidgets)
        remainingDrivers.removeOne(oneRule->property("Driver").toString());

    ProfileScript *newItem = new ProfileScript(this);
    connect(newItem, &ProfileScript::removedRequested, this, &ProfileScriptDialog::removeRule);
    newItem->setDriverList(remainingDrivers);
    m_ProfileScriptWidgets.append(newItem);
    m_MainLayout->insertWidget(m_MainLayout->count() - 2, newItem);
    adjustSize();
}

void ProfileScriptDialog::addJSONRule(QJsonObject settings)
{
    if (m_ProfileScriptWidgets.count() == m_DriversList.count())
        return;

    ProfileScript *newItem = new ProfileScript(this);
    connect(newItem, &ProfileScript::removedRequested, this, &ProfileScriptDialog::removeRule);
    newItem->setProperty("PreDelay", settings["PreDelay"].toInt());
    newItem->setProperty("PreScript", settings["PreScript"].toString());
    newItem->setProperty("Driver", settings["Driver"].toString());
    newItem->setProperty("PostDelay", settings["PostDelay"].toInt());
    newItem->setProperty("PostScript", settings["PostScript"].toString());
    newItem->setProperty("StoppingDelay", settings["StoppingDelay"].toInt());
    newItem->setProperty("StoppingScript", settings["StoppingScript"].toString());
    newItem->setProperty("StoppedDelay", settings["StoppedDelay"].toInt());
    newItem->setProperty("StoppedScript", settings["StoppedScript"].toString());
    newItem->setDriverList(m_DriversList);

    // Make sure GUI reflects all property changes.
    newItem->syncGUI();
    m_ProfileScriptWidgets.append(newItem);
    m_MainLayout->insertWidget(m_MainLayout->count() - 2, newItem);

    adjustSize();
}

void ProfileScriptDialog::removeRule()
{
    auto toRemove = sender();
    auto result = std::find_if(m_ProfileScriptWidgets.begin(), m_ProfileScriptWidgets.end(), [toRemove](const auto & oneRule)
    {
        return oneRule == toRemove;
    });
    if (result != m_ProfileScriptWidgets.end())
    {
        m_MainLayout->removeWidget(*result);
        (*result)->deleteLater();
        m_ProfileScriptWidgets.removeOne(*result);
        adjustSize();
    }
}

void ProfileScriptDialog::parseSettings(const QByteArray &settings)
{
    QJsonParseError jsonError;
    QJsonDocument doc = QJsonDocument::fromJson(settings, &jsonError);

    if (jsonError.error != QJsonParseError::NoError)
        return;

    m_ProfileScripts = doc.array();

    for (const auto &oneRule : qAsConst(m_ProfileScripts))
        addJSONRule(oneRule.toObject());
}

void ProfileScriptDialog::generateSettings()
{
    m_ProfileScripts = QJsonArray();

    for (auto &oneRule : m_ProfileScriptWidgets)
    {
        m_ProfileScripts.append(oneRule->toJSON());
    }

    close();
}
