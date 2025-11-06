/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "templatelibrarywidget.h"
#include "propertytemplatebuilder.h"
#include "../tasktemplate.h"
#include "../templatemanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QSplitter>
#include <QHeaderView>
#include <QMessageBox>
#include <KLocalizedString>

namespace Ekos
{

TemplateLibraryWidget::TemplateLibraryWidget(QWidget *parent)
    : QWidget(parent)
{
    // Set window flags to make this a proper standalone window
    setWindowFlags(Qt::Window);
    setWindowTitle(i18n("Task Queue Templates"));
    resize(800, 600);

    m_templateManager = TemplateManager::Instance();
    setupUI();
    refreshLibrary();
}

TemplateLibraryWidget::~TemplateLibraryWidget()
{
}

void TemplateLibraryWidget::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);

    // Splitter for tree and preview
    auto *splitter = new QSplitter(Qt::Horizontal, this);

    // Template tree
    m_templateTree = new QTreeWidget(splitter);
    m_templateTree->setHeaderLabel(i18n("Templates"));
    m_templateTree->setAlternatingRowColors(true);
    m_templateTree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_templateTree->header()->setStretchLastSection(true);

    // Preview pane
    auto *previewWidget = new QWidget(splitter);
    auto *previewLayout = new QVBoxLayout(previewWidget);

    auto *previewLabel = new QLabel(i18n("Preview"), previewWidget);
    QFont previewFont = previewLabel->font();
    previewFont.setBold(true);
    previewLabel->setFont(previewFont);
    previewLayout->addWidget(previewLabel);

    m_previewText = new QTextEdit(previewWidget);
    m_previewText->setReadOnly(true);
    previewLayout->addWidget(m_previewText);

    // Buttons
    auto *buttonLayout = new QHBoxLayout();
    m_addButton = new QPushButton(QIcon::fromTheme("list-add"), i18n("Add to Queue"), previewWidget);
    m_addButton->setEnabled(false);
    m_createButton = new QPushButton(QIcon::fromTheme("document-new"), i18n("Create Custom Template"), previewWidget);

    m_refreshButton = new QPushButton(QIcon::fromTheme("view-refresh"), "", previewWidget);
    
    m_editButton = new QPushButton(QIcon::fromTheme("document-edit"), "", previewWidget);
    m_editButton->setEnabled(false);
    m_editButton->setToolTip(i18n("Edit Template"));
    
    m_deleteButton = new QPushButton(QIcon::fromTheme("edit-delete"), "", previewWidget);
    m_deleteButton->setEnabled(false);
    m_deleteButton->setToolTip(i18n("Delete Template"));

    buttonLayout->addWidget(m_addButton);
    buttonLayout->addWidget(m_createButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_refreshButton);
    buttonLayout->addWidget(m_editButton);
    buttonLayout->addWidget(m_deleteButton);    
    previewLayout->addLayout(buttonLayout);

    // Set splitter sizes
    splitter->addWidget(m_templateTree);
    splitter->addWidget(previewWidget);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(splitter);

    // Connect signals
    connect(m_templateTree, &QTreeWidget::itemSelectionChanged,
            this, &TemplateLibraryWidget::onSelectionChanged);
    connect(m_templateTree, &QTreeWidget::itemDoubleClicked,
            this, &TemplateLibraryWidget::onItemDoubleClicked);
    connect(m_addButton, &QPushButton::clicked,
            this, &TemplateLibraryWidget::onAddToQueueClicked);
    connect(m_createButton, &QPushButton::clicked,
            this, &TemplateLibraryWidget::onCreateCustomTemplateClicked);
    connect(m_refreshButton, &QPushButton::clicked,
            this, &TemplateLibraryWidget::onRefreshClicked);
    connect(m_editButton, &QPushButton::clicked,
            this, &TemplateLibraryWidget::onEditTemplateClicked);
    connect(m_deleteButton, &QPushButton::clicked,
            this, &TemplateLibraryWidget::onDeleteTemplateClicked);
}

void TemplateLibraryWidget::refreshLibrary()
{
    if (m_templateManager)
    {
        m_templateManager->initialize();
    }
    populateTemplates();
}

void TemplateLibraryWidget::populateTemplates()
{
    // Block signals to prevent onSelectionChanged from firing with stale pointers
    // during tree rebuild when templates are being deleted/recreated
    m_templateTree->blockSignals(true);

    m_templateTree->clear();
    m_selectedTemplate = nullptr;

    if (!m_templateManager)
    {
        m_templateTree->blockSignals(false);
        return;
    }

    // Get all categories
    QStringList categories = m_templateManager->categories();

    // Create category nodes
    QMap<QString, QTreeWidgetItem*> categoryItems;

    for (const QString &category : categories)
    {
        auto *categoryItem = new QTreeWidgetItem(m_templateTree);
        categoryItem->setText(0, category);
        categoryItem->setExpanded(true);
        QFont font = categoryItem->font(0);
        font.setBold(true);
        categoryItem->setFont(0, font);
        categoryItem->setData(0, Qt::UserRole, QVariant()); // No template for category
        categoryItems[category] = categoryItem;
    }

    // Add templates under their categories
    QList<TaskTemplate*> templates = m_templateManager->allTemplates();

    for (TaskTemplate *tmpl : templates)
    {
        QString category = tmpl->category();
        QTreeWidgetItem *categoryItem = categoryItems.value(category);

        if (categoryItem)
        {
            auto *templateItem = new QTreeWidgetItem(categoryItem);
            templateItem->setText(0, tmpl->name());

            // Store template pointer
            templateItem->setData(0, Qt::UserRole, QVariant::fromValue(static_cast<void*>(tmpl)));

            // Add icon based on whether it's system or user template
            if (tmpl->isSystemTemplate())
            {
                templateItem->setIcon(0, QIcon::fromTheme("folder-template"));
            }
            else
            {
                templateItem->setIcon(0, QIcon::fromTheme("document-edit"));
            }
        }
    }

    m_previewText->clear();
    m_addButton->setEnabled(false);

    // Re-enable signals after tree is fully populated
    m_templateTree->blockSignals(false);
}

void TemplateLibraryWidget::onSelectionChanged()
{
    QTreeWidgetItem *item = m_templateTree->currentItem();
    m_selectedTemplate = getTemplateFromItem(item);

    updatePreview();
    m_addButton->setEnabled(m_selectedTemplate != nullptr);

    // Enable edit/delete only for user templates
    bool isUserTemplate = m_selectedTemplate && !m_selectedTemplate->isSystemTemplate();
    m_editButton->setEnabled(isUserTemplate);
    m_deleteButton->setEnabled(isUserTemplate);

    emit selectionChanged(m_selectedTemplate);
}

void TemplateLibraryWidget::onItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);

    TaskTemplate *tmpl = getTemplateFromItem(item);
    if (tmpl)
    {
        emit addTemplateRequested(tmpl);
    }
}

void TemplateLibraryWidget::onAddToQueueClicked()
{
    if (m_selectedTemplate)
    {
        emit addTemplateRequested(m_selectedTemplate);
    }
}

void TemplateLibraryWidget::onCreateCustomTemplateClicked()
{
    // Open the Property Template Builder dialog
    PropertyTemplateBuilderDialog dialog(this);

    if (dialog.exec() == QDialog::Accepted)
    {
        // Template was created successfully
        // Refresh the library to show the new template
        refreshLibrary();

        // Optionally, try to select the newly created template
        TaskTemplate *newTemplate = dialog.generatedTemplate();
        if (newTemplate && m_templateManager)
        {
            // The template should now be loaded by the template manager
            // Try to find and select it in the tree
            QTreeWidgetItemIterator it(m_templateTree);
            while (*it)
            {
                TaskTemplate *tmpl = getTemplateFromItem(*it);
                if (tmpl && tmpl->id() == newTemplate->id())
                {
                    m_templateTree->setCurrentItem(*it);
                    break;
                }
                ++it;
            }
        }
    }
}

void TemplateLibraryWidget::onRefreshClicked()
{
    refreshLibrary();
}

void TemplateLibraryWidget::updatePreview()
{
    if (!m_selectedTemplate)
    {
        m_previewText->clear();
        return;
    }

    QString html;
    html += QString("<h2>%1</h2>").arg(m_selectedTemplate->name());
    html += QString("<p><b>%1:</b> %2</p>").arg(i18n("Category")).arg(m_selectedTemplate->category());
    html += QString("<p><b>%1:</b> %2</p>").arg(i18n("Version")).arg(m_selectedTemplate->version());

    if (!m_selectedTemplate->description().isEmpty())
    {
        html += QString("<p>%1</p>").arg(m_selectedTemplate->description());
    }

    // Parameters
    QList<TaskTemplate::Parameter> params = m_selectedTemplate->parameters();
    if (!params.isEmpty())
    {
        html += QString("<h3>%1</h3>").arg(i18n("Parameters"));
        html += "<ul>";
        for (const TaskTemplate::Parameter &param : params)
        {
            QString paramInfo = QString("<b>%1</b> (%2)").arg(param.name).arg(param.type);

            if (!param.description.isEmpty())
            {
                paramInfo += QString(": %1").arg(param.description);
            }

            if (!param.unit.isEmpty())
            {
                paramInfo += QString(" [%1]").arg(param.unit);
            }

            html += QString("<li>%1</li>").arg(paramInfo);
        }
        html += "</ul>";
    }

    // Actions
    QJsonArray actions = m_selectedTemplate->actionDefinitions();
    if (!actions.isEmpty())
    {
        html += QString("<h3>%1</h3>").arg(i18n("Actions"));
        html += QString("<p>%1: %2</p>").arg(i18n("Number of actions")).arg(actions.size());
    }

    // Template type
    if (m_selectedTemplate->isSystemTemplate())
    {
        html += QString("<p><i>%1</i></p>").arg(i18n("System Template"));
    }
    else
    {
        html += QString("<p><i>%1</i></p>").arg(i18n("User Template"));
        if (!m_selectedTemplate->parentTemplate().isEmpty())
        {
            html += QString("<p>%1: %2</p>")
                    .arg(i18n("Based on"))
                    .arg(m_selectedTemplate->parentTemplate());
        }
    }

    m_previewText->setHtml(html);
}

TaskTemplate* TemplateLibraryWidget::getTemplateFromItem(QTreeWidgetItem *item) const
{
    if (!item)
        return nullptr;

    QVariant data = item->data(0, Qt::UserRole);
    if (!data.isValid() || data.isNull())
        return nullptr;

    return static_cast<TaskTemplate*>(data.value<void*>());
}

TaskTemplate* TemplateLibraryWidget::selectedTemplate() const
{
    return m_selectedTemplate;
}

void TemplateLibraryWidget::onEditTemplateClicked()
{
    if (!m_selectedTemplate)
        return;

    PropertyTemplateBuilderDialog dialog(m_selectedTemplate, this);

    if (dialog.exec() == QDialog::Accepted)
    {
        refreshLibrary();

        // Try to re-select the edited template
        TaskTemplate *editedTemplate = dialog.generatedTemplate();
        if (editedTemplate)
        {
            QTreeWidgetItemIterator it(m_templateTree);
            while (*it)
            {
                TaskTemplate *tmpl = getTemplateFromItem(*it);
                if (tmpl && tmpl->id() == editedTemplate->id())
                {
                    m_templateTree->setCurrentItem(*it);
                    break;
                }
                ++it;
            }
        }
    }
}

void TemplateLibraryWidget::onDeleteTemplateClicked()
{
    if (!m_selectedTemplate || !m_templateManager)
        return;

    QString templateId = m_selectedTemplate->id();

    if (m_templateManager->deleteUserTemplate(templateId))
    {
        refreshLibrary();
    }
    else
    {
        QMessageBox::warning(this, i18n("Delete Failed"),
                             i18n("Failed to delete template."));
    }
}

} // namespace Ekos
