/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "tasktemplate.h"

#include <QObject>
#include <QString>
#include <QMap>
#include <QList>

namespace Ekos
{

/**
 * @class TemplateManager
 * @brief Manages loading, storing, and accessing task templates
 *
 * TemplateManager is responsible for:
 * - Loading system templates from JSON files
 * - Loading user templates from configuration
 * - Providing template library access
 * - Creating new user templates
 * - Validating templates
 */
class TemplateManager : public QObject
{
        Q_OBJECT

    public:
        static TemplateManager *Instance();
        static void release();

        /**
         * @brief Initialize manager and load templates
         * @return true if initialization succeeded
         */
        bool initialize();

        /**
         * @brief Get all templates
         * @return List of all loaded templates
         */
        QList<TaskTemplate *> allTemplates() const;

        /**
         * @brief Get templates by category
         * @param category Category name (e.g., "Camera", "Mount")
         * @return List of templates in category
         */
        QList<TaskTemplate *> templatesByCategory(const QString &category) const;

        /**
         * @brief Get all categories
         * @return List of unique category names
         */
        QStringList categories() const;

        /**
         * @brief Get template by ID
         * @param id Template ID
         * @return Template pointer or nullptr if not found
         */
        TaskTemplate *getTemplate(const QString &id) const;

        /**
         * @brief Get templates supporting a device interface
         * @param deviceInterface Device's driver interface flags
         * @return List of compatible templates
         */
        QList<TaskTemplate *> templatesForDevice(uint32_t deviceInterface) const;

        /**
         * @brief Create a new user template from a system template
         * @param systemTemplateId ID of system template to copy
         * @param newName Name for the new template
         * @param parameters Modified parameters
         * @return New template ID, or empty string on failure
         */
        QString createUserTemplate(const QString &systemTemplateId,
                                   const QString &newName,
                                   const QMap<QString, QVariant> &parameters);

        /**
         * @brief Delete a user template
         * @param templateId ID of template to delete
         * @return true if deleted successfully
         */
        bool deleteUserTemplate(const QString &templateId);

        /**
         * @brief Save user templates to configuration
         * @return true if saved successfully
         */
        bool saveUserTemplates();

    signals:
        void templatesChanged();
        void templateAdded(const QString &templateId);
        void templateRemoved(const QString &templateId);

    private:
        explicit TemplateManager(QObject *parent = nullptr);
        ~TemplateManager() override;

        /**
         * @brief Load system templates from JSON files
         * @return true if at least one template loaded
         */
        bool loadSystemTemplates();

        /**
         * @brief Load a single template file
         * @param filePath Path to JSON file
         * @return true if loaded successfully
         */
        bool loadTemplateFile(const QString &filePath);

        /**
         * @brief Load a single user template file
         * @param filePath Path to JSON file
         * @return true if loaded successfully
         */
        bool loadUserTemplateFile(const QString &filePath);

        /**
         * @brief Load user templates from configuration
         * @return true if loaded successfully
         */
        bool loadUserTemplates();

        /**
         * @brief Generate unique template ID
         * @param baseName Base name for ID
         * @return Unique ID
         */
        QString generateUniqueId(const QString &baseName);

        static TemplateManager *m_Instance;

        // Templates organized by ID for quick lookup
        QMap<QString, TaskTemplate *> m_templates;

        // Track system vs user templates
        QStringList m_systemTemplateIds;
        QStringList m_userTemplateIds;
};

} // namespace Ekos
