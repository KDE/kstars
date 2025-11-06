/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QWidget>
#include <QString>

class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;
class QLabel;
class QTextEdit;

namespace Ekos
{

class TaskTemplate;
class TemplateManager;

/**
 * @class TemplateLibraryWidget
 * @brief Widget for browsing and selecting task queue templates
 *
 * This widget displays a tree view of available templates organized by
 * category, with a preview pane showing template details.
 */
class TemplateLibraryWidget : public QWidget
{
        Q_OBJECT

    public:
        explicit TemplateLibraryWidget(QWidget *parent = nullptr);
        ~TemplateLibraryWidget() override;

        /**
         * @brief Get the currently selected template
         * @return Selected template, or nullptr if none selected
         */
        TaskTemplate* selectedTemplate() const;

        /**
         * @brief Refresh the template library
         */
        void refreshLibrary();

    signals:
        /**
         * @brief Emitted when user wants to add a template to queue
         * @param tmpl Template to add
         */
        void addTemplateRequested(TaskTemplate *tmpl);

        /**
         * @brief Emitted when template selection changes
         * @param tmpl Selected template, or nullptr
         */
        void selectionChanged(TaskTemplate *tmpl);

    private slots:
        void onSelectionChanged();
        void onItemDoubleClicked(QTreeWidgetItem *item, int column);
        void onAddToQueueClicked();
        void onCreateCustomTemplateClicked();
        void onRefreshClicked();
        void onEditTemplateClicked();
        void onDeleteTemplateClicked();

    private:
        void setupUI();
        void populateTemplates();
        void updatePreview();
        TaskTemplate* getTemplateFromItem(QTreeWidgetItem *item) const;

        TemplateManager *m_templateManager = nullptr;
        TaskTemplate *m_selectedTemplate = nullptr;

        // UI widgets
        QTreeWidget *m_templateTree = nullptr;
        QTextEdit *m_previewText = nullptr;
        QPushButton *m_addButton = nullptr;
        QPushButton *m_createButton = nullptr;
        QPushButton *m_refreshButton = nullptr;
        QPushButton *m_editButton = nullptr;
        QPushButton *m_deleteButton = nullptr;
};

} // namespace Ekos
