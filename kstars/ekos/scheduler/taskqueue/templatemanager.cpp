/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "templatemanager.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"
#include "auxiliary/kspaths.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QDateTime>

namespace Ekos
{

TemplateManager *TemplateManager::m_Instance = nullptr;

TemplateManager *TemplateManager::Instance()
{
    if (m_Instance == nullptr)
    {
        m_Instance = new TemplateManager(KStars::Instance());
    }
    return m_Instance;
}

void TemplateManager::release()
{
    delete m_Instance;
    m_Instance = nullptr;
}

TemplateManager::TemplateManager(QObject *parent)
    : QObject(parent)
{
}

TemplateManager::~TemplateManager()
{
    qDeleteAll(m_templates);
    m_templates.clear();
}

bool TemplateManager::initialize()
{
    // Clear existing templates before reloading
    qDeleteAll(m_templates);
    m_templates.clear();
    m_systemTemplateIds.clear();
    m_userTemplateIds.clear();

    // Load system templates
    if (!loadSystemTemplates())
    {
        return false;
    }

    // Load user templates
    loadUserTemplates();

    return !m_templates.isEmpty();
}

bool TemplateManager::loadSystemTemplates()
{
    // Find system template directory using KSPaths
    // Use a unique subdirectory to avoid conflict with user templates
    QString dataPath = KSPaths::locate(QStandardPaths::AppDataLocation,
                                       "taskqueue/templates/system",
                                       QStandardPaths::LocateDirectory);

    if (dataPath.isEmpty() || !QDir(dataPath).exists())
    {
        qWarning() << "System template directory not found";
        return false;
    }

    QDir dir(dataPath);
    QStringList filters;
    filters << "*.json";
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);

    int loadedCount = 0;
    for (const QFileInfo &fileInfo : files)
    {
        if (loadTemplateFile(fileInfo.absoluteFilePath()))
        {
            loadedCount++;
        }
    }

    qInfo() << "Loaded" << loadedCount << "system templates";
    return loadedCount > 0;
}

bool TemplateManager::loadTemplateFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "Failed to open template file:" << filePath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError)
    {
        qWarning() << "JSON parse error in" << filePath << ":" << error.errorString();
        return false;
    }

    if (!doc.isObject())
    {
        qWarning() << "Template file must contain JSON object:" << filePath;
        return false;
    }

    QJsonObject root = doc.object();

    // Check if this is a multi-template file or single template
    if (root.contains("templates") && root["templates"].isArray())
    {
        // Multiple templates in one file
        QJsonArray templatesArray = root["templates"].toArray();
        int loadedCount = 0;

        for (const QJsonValue &val : templatesArray)
        {
            if (val.isObject())
            {
                TaskTemplate *tmpl = new TaskTemplate(this);
                if (tmpl->loadFromJson(val.toObject()))
                {
                    QString id = tmpl->id();
                    if (m_templates.contains(id))
                    {
                        qWarning() << "Duplicate template ID:" << id;
                        delete tmpl;
                    }
                    else
                    {
                        m_templates[id] = tmpl;
                        m_systemTemplateIds.append(id);
                        loadedCount++;
                    }
                }
                else
                {
                    qWarning() << "Failed to load template from" << filePath;
                    delete tmpl;
                }
            }
        }

        return loadedCount > 0;
    }
    else
    {
        // Single template in file
        TaskTemplate *tmpl = new TaskTemplate(this);
        if (tmpl->loadFromJson(root))
        {
            QString id = tmpl->id();
            if (m_templates.contains(id))
            {
                qWarning() << "Duplicate template ID:" << id;
                delete tmpl;
                return false;
            }

            m_templates[id] = tmpl;
            m_systemTemplateIds.append(id);
            return true;
        }
        else
        {
            qWarning() << "Failed to load template from" << filePath;
            delete tmpl;
            return false;
        }
    }
}

bool TemplateManager::loadUserTemplateFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "Failed to open user template file:" << filePath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError)
    {
        qWarning() << "JSON parse error in" << filePath << ":" << error.errorString();
        return false;
    }

    if (!doc.isObject())
    {
        qWarning() << "User template file must contain JSON object:" << filePath;
        return false;
    }

    QJsonObject root = doc.object();

    // Check if this is a multi-template file or single template
    if (root.contains("templates") && root["templates"].isArray())
    {
        // Multiple templates in one file
        QJsonArray templatesArray = root["templates"].toArray();
        int loadedCount = 0;

        for (const QJsonValue &val : templatesArray)
        {
            if (val.isObject())
            {
                TaskTemplate *tmpl = new TaskTemplate(this);
                tmpl->setSystemTemplate(false);

                if (tmpl->loadFromJson(val.toObject()))
                {
                    QString id = tmpl->id();
                    if (m_templates.contains(id))
                    {
                        qWarning() << "Duplicate template ID:" << id;
                        delete tmpl;
                    }
                    else
                    {
                        m_templates[id] = tmpl;
                        m_userTemplateIds.append(id);
                        loadedCount++;
                    }
                }
                else
                {
                    qWarning() << "Failed to load user template from" << filePath;
                    delete tmpl;
                }
            }
        }

        return loadedCount > 0;
    }
    else
    {
        // Single template in file
        TaskTemplate *tmpl = new TaskTemplate(this);
        tmpl->setSystemTemplate(false);

        if (tmpl->loadFromJson(root))
        {
            QString id = tmpl->id();
            if (m_templates.contains(id))
            {
                qWarning() << "Duplicate template ID:" << id;
                delete tmpl;
                return false;
            }

            m_templates[id] = tmpl;
            m_userTemplateIds.append(id);
            return true;
        }
        else
        {
            qWarning() << "Failed to load user template from" << filePath;
            delete tmpl;
            return false;
        }
    }
}

bool TemplateManager::loadUserTemplates()
{
    // Load user templates from individual files in templates/user directory
    QString configPath = KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QString userTemplatesDir = QDir(configPath).filePath("taskqueue/templates/user");

    QDir dir(userTemplatesDir);
    if (!dir.exists())
    {
        // No user templates yet - not an error
        qInfo() << "User templates directory does not exist yet";
        return true;
    }

    QStringList filters;
    filters << "*.json";
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);

    int loadedCount = 0;
    for (const QFileInfo &fileInfo : files)
    {
        if (loadUserTemplateFile(fileInfo.absoluteFilePath()))
        {
            loadedCount++;
        }
    }

    qInfo() << "Loaded" << loadedCount << "user templates";
    return true;
}

QList<TaskTemplate *> TemplateManager::allTemplates() const
{
    return m_templates.values();
}

QList<TaskTemplate *> TemplateManager::templatesByCategory(const QString &category) const
{
    QList<TaskTemplate *> result;
    for (TaskTemplate *tmpl : m_templates)
    {
        if (tmpl->category() == category)
        {
            result.append(tmpl);
        }
    }
    return result;
}

QStringList TemplateManager::categories() const
{
    QStringList cats;
    for (TaskTemplate *tmpl : m_templates)
    {
        QString cat = tmpl->category();
        if (!cats.contains(cat))
        {
            cats.append(cat);
        }
    }
    cats.sort();
    return cats;
}

TaskTemplate *TemplateManager::getTemplate(const QString &id) const
{
    return m_templates.value(id, nullptr);
}

QList<TaskTemplate *> TemplateManager::templatesForDevice(uint32_t deviceInterface) const
{
    QList<TaskTemplate *> result;
    for (TaskTemplate *tmpl : m_templates)
    {
        if (tmpl->supportsDeviceInterface(deviceInterface))
        {
            result.append(tmpl);
        }
    }
    return result;
}

QString TemplateManager::createUserTemplate(const QString &systemTemplateId,
        const QString &newName,
        const QMap<QString, QVariant> &parameters)
{
    // Get source template
    TaskTemplate *source = getTemplate(systemTemplateId);
    if (!source || !source->isSystemTemplate())
    {
        qWarning() << "Source template not found or not a system template:" << systemTemplateId;
        return QString();
    }

    // Create new template
    TaskTemplate *newTemplate = new TaskTemplate(this);

    // Copy from source
    QJsonObject sourceJson = source->toJson();

    // Modify for user template
    sourceJson["id"] = generateUniqueId("user_" + source->id());
    sourceJson["name"] = newName;
    sourceJson["parent_template"] = systemTemplateId;

    // Update default parameter values
    QJsonArray paramsArray = sourceJson["parameters"].toArray();
    QJsonArray updatedParams;
    for (const QJsonValue &val : paramsArray)
    {
        QJsonObject paramObj = val.toObject();
        QString paramName = paramObj["name"].toString();

        if (parameters.contains(paramName))
        {
            paramObj["default"] = QJsonValue::fromVariant(parameters[paramName]);
        }

        updatedParams.append(paramObj);
    }
    sourceJson["parameters"] = updatedParams;

    // Load into new template
    if (!newTemplate->loadFromJson(sourceJson))
    {
        delete newTemplate;
        return QString();
    }

    newTemplate->setSystemTemplate(false);

    // Add to manager
    QString newId = newTemplate->id();
    m_templates[newId] = newTemplate;
    m_userTemplateIds.append(newId);

    // Save user templates
    saveUserTemplates();

    emit templateAdded(newId);
    emit templatesChanged();

    return newId;
}

bool TemplateManager::deleteUserTemplate(const QString &templateId)
{
    TaskTemplate *tmpl = getTemplate(templateId);
    if (!tmpl || tmpl->isSystemTemplate())
    {
        return false;
    }

    // Delete the individual template file from disk
    QString configPath = KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QString userTemplatesDir = QDir(configPath).filePath("taskqueue/templates/user");
    QString filePath = QDir(userTemplatesDir).filePath(templateId + ".json");
    
    if (QFile::exists(filePath))
    {
        if (!QFile::remove(filePath))
        {
            qWarning() << "Failed to delete template file:" << filePath;
        }
    }

    // Remove from memory
    m_templates.remove(templateId);
    m_userTemplateIds.removeAll(templateId);

    delete tmpl;

    // Update the combined user templates file (for backward compatibility)
    saveUserTemplates();

    emit templateRemoved(templateId);
    emit templatesChanged();

    return true;
}

bool TemplateManager::saveUserTemplates()
{
    QJsonObject root;
    QJsonArray templatesArray;

    // Save only user templates
    for (const QString &id : m_userTemplateIds)
    {
        TaskTemplate *tmpl = m_templates.value(id);
        if (tmpl)
        {
            templatesArray.append(tmpl->toJson());
        }
    }

    root["templates"] = templatesArray;
    root["version"] = "1.0";
    root["last_modified"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    QJsonDocument doc(root);

    // Ensure directory exists using KSPaths
    QString configPath = KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QString dirPath = QDir(configPath).filePath("taskqueue");
    QDir().mkpath(dirPath);

    QString filePath = QDir(dirPath).filePath("user_templates.json");
    QFile file(filePath);

    if (!file.open(QIODevice::WriteOnly))
    {
        qWarning() << "Failed to save user templates to" << filePath;
        return false;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    return true;
}

QString TemplateManager::generateUniqueId(const QString &baseName)
{
    QString id = baseName;
    int counter = 1;

    while (m_templates.contains(id))
    {
        id = QString("%1_%2").arg(baseName).arg(counter++);
    }

    return id;
}

} // namespace Ekos
