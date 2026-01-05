/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "collectionsdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <KLocalizedString>

namespace Ekos
{

CollectionsDialog::CollectionsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(i18n("Load Task Collection"));
    resize(600, 400);

    setupUI();
    loadCollections();
}

CollectionsDialog::~CollectionsDialog()
{
}

void CollectionsDialog::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);

    // Collection list
    auto *listGroupBox = new QGroupBox(i18n("Available Collections"), this);
    auto *listLayout = new QVBoxLayout(listGroupBox);

    m_collectionList = new QListWidget(this);
    m_collectionList->setIconSize(QSize(32, 32));
    listLayout->addWidget(m_collectionList);

    mainLayout->addWidget(listGroupBox);

    // Details panel
    auto *detailsGroupBox = new QGroupBox(i18n("Collection Details"), this);
    auto *detailsLayout = new QVBoxLayout(detailsGroupBox);

    m_nameLabel = new QLabel(this);
    m_nameLabel->setWordWrap(true);
    m_nameLabel->setStyleSheet("font-weight: bold; font-size: 12pt;");
    detailsLayout->addWidget(m_nameLabel);

    m_descriptionLabel = new QLabel(this);
    m_descriptionLabel->setWordWrap(true);
    detailsLayout->addWidget(m_descriptionLabel);

    detailsLayout->addSpacing(10);

    m_tasksLabel = new QLabel(this);
    m_tasksLabel->setWordWrap(true);
    detailsLayout->addWidget(m_tasksLabel);

    detailsLayout->addStretch();

    mainLayout->addWidget(detailsGroupBox);

    // Buttons
    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_cancelButton = new QPushButton(QIcon::fromTheme("dialog-cancel"), i18n("Cancel"), this);
    m_loadButton = new QPushButton(QIcon::fromTheme("document-open"), i18n("Load Collection"), this);
    m_loadButton->setEnabled(false);
    m_loadButton->setDefault(true);

    buttonLayout->addWidget(m_cancelButton);
    buttonLayout->addWidget(m_loadButton);

    mainLayout->addLayout(buttonLayout);

    // Connect signals
    connect(m_collectionList, &QListWidget::currentItemChanged,
            this, &CollectionsDialog::onCollectionSelected);
    connect(m_collectionList, &QListWidget::itemDoubleClicked,
            this, &CollectionsDialog::onLoadClicked);
    connect(m_loadButton, &QPushButton::clicked,
            this, &CollectionsDialog::onLoadClicked);
    connect(m_cancelButton, &QPushButton::clicked,
            this, &QDialog::reject);
}

void CollectionsDialog::loadCollections()
{
    m_collectionFiles.clear();
    m_collectionList->clear();

    // Look for collections in data directory
    QStringList dataDirs = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);

    for (const QString &dataDir : dataDirs)
    {
        QString collectionsPath = dataDir + "/kstars/taskqueue/collections";
        QDir dir(collectionsPath);

        if (!dir.exists())
            continue;

        QStringList filters;
        filters << "*.json";
        QFileInfoList files = dir.entryInfoList(filters, QDir::Files);

        for (const QFileInfo &fileInfo : files)
        {
            QString filePath = fileInfo.absoluteFilePath();

            // Read collection file to get name
            QFile file(filePath);
            if (!file.open(QIODevice::ReadOnly))
                continue;

            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            file.close();

            if (!doc.isObject())
                continue;

            QJsonObject obj = doc.object();
            QString name = obj["name"].toString();

            if (name.isEmpty())
                name = fileInfo.baseName();

            // Add to list
            auto *item = new QListWidgetItem(QIcon::fromTheme("folder-templates"), name);
            item->setToolTip(filePath);
            m_collectionList->addItem(item);

            m_collectionFiles[name] = filePath;
        }
    }

    if (m_collectionList->count() > 0)
    {
        m_collectionList->setCurrentRow(0);
    }
}

void CollectionsDialog::onCollectionSelected(QListWidgetItem *current, QListWidgetItem *previous)
{
    Q_UNUSED(previous);

    if (!current)
    {
        m_loadButton->setEnabled(false);
        m_nameLabel->clear();
        m_descriptionLabel->clear();
        m_tasksLabel->clear();
        return;
    }

    QString name = current->text();
    QString filePath = m_collectionFiles.value(name);

    if (filePath.isEmpty())
        return;

    m_selectedCollection = filePath;
    m_loadButton->setEnabled(true);

    displayCollectionDetails(filePath);
}

void CollectionsDialog::displayCollectionDetails(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        m_nameLabel->setText(i18n("Error reading collection"));
        m_descriptionLabel->clear();
        m_tasksLabel->clear();
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject())
    {
        m_nameLabel->setText(i18n("Invalid collection format"));
        m_descriptionLabel->clear();
        m_tasksLabel->clear();
        return;
    }

    QJsonObject obj = doc.object();

    // Display name
    QString name = obj["name"].toString();
    m_nameLabel->setText(name);

    // Display description
    QString description = obj["description"].toString();
    m_descriptionLabel->setText(description);

    // Display tasks
    QJsonArray tasks = obj["tasks"].toArray();
    QString tasksText = i18n("<b>Tasks (%1):</b><br/>", tasks.size());

    int taskNum = 1;
    for (const QJsonValue &taskVal : tasks)
    {
        QJsonObject task = taskVal.toObject();
        QString templateId = task["template_id"].toString();

        // Convert template ID to display name
        QString displayName = templateId;
        displayName.replace('_', ' ');
        displayName = displayName.left(1).toUpper() + displayName.mid(1);

        tasksText += QString("%1. %2<br/>").arg(taskNum).arg(displayName);
        taskNum++;
    }

    m_tasksLabel->setText(tasksText);
}

void CollectionsDialog::onLoadClicked()
{
    if (!m_selectedCollection.isEmpty())
    {
        accept();
    }
}

} // namespace Ekos
