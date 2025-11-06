/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "queuemanager.h"
#include "../task.h"
#include "../tasktemplate.h"
#include "../templatemanager.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>

namespace Ekos
{

QueueManager::QueueManager(QObject *parent)
    : QObject(parent)
{
}

QueueManager::~QueueManager()
{
    clear();
}

void QueueManager::addItem(QueueItem *item)
{
    if (!item)
        return;

    item->setParent(this);
    m_items.append(item);
    emit itemAdded(item, m_items.size() - 1);
}

void QueueManager::insertItem(int index, QueueItem *item)
{
    if (!item || index < 0 || index > m_items.size())
        return;

    item->setParent(this);
    m_items.insert(index, item);
    emit itemAdded(item, index);
}

bool QueueManager::removeItem(int index)
{
    if (index < 0 || index >= m_items.size())
        return false;

    QueueItem *item = m_items.at(index);

    // Don't remove currently executing item
    if (item == m_currentItem && m_state == RUNNING)
        return false;

    m_items.removeAt(index);
    emit itemRemoved(item, index);

    // Delete the item
    item->deleteLater();

    return true;
}

bool QueueManager::removeItem(QueueItem *item)
{
    int index = m_items.indexOf(item);
    if (index < 0)
        return false;

    return removeItem(index);
}

void QueueManager::clear()
{
    // Don't clear while running
    if (m_state == RUNNING)
        return;

    // Use deleteLater() to allow proper cleanup of device connections
    // and shared pointers in TaskActions before destruction
    for (QueueItem *item : m_items)
    {
        if (item)
            item->deleteLater();
    }
    
    m_items.clear();
    m_currentItem = nullptr;

    emit queueCleared();
}

bool QueueManager::moveItem(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= m_items.size() ||
            toIndex < 0 || toIndex >= m_items.size())
        return false;

    if (fromIndex == toIndex)
        return true;

    // Don't move currently executing item
    if (m_items.at(fromIndex) == m_currentItem && m_state == RUNNING)
        return false;

    m_items.move(fromIndex, toIndex);
    emit itemMoved(fromIndex, toIndex);

    return true;
}

bool QueueManager::moveUp(int index)
{
    if (index <= 0)
        return false;

    return moveItem(index, index - 1);
}

bool QueueManager::moveDown(int index)
{
    if (index < 0 || index >= m_items.size() - 1)
        return false;

    return moveItem(index, index + 1);
}

QueueItem *QueueManager::itemAt(int index) const
{
    if (index < 0 || index >= m_items.size())
        return nullptr;

    return m_items.at(index);
}

int QueueManager::indexOf(QueueItem *item) const
{
    return m_items.indexOf(item);
}

void QueueManager::setState(QueueState state)
{
    if (m_state != state)
    {
        m_state = state;
        emit stateChanged(state);
    }
}

void QueueManager::setCurrentItem(QueueItem *item)
{
    if (m_currentItem != item)
    {
        m_currentItem = item;
        emit currentItemChanged(item);
    }
}

int QueueManager::currentIndex() const
{
    return m_items.indexOf(m_currentItem);
}

bool QueueManager::saveQueue(const QString &filePath)
{
    QJsonObject json = toJson();
    QJsonDocument doc(json);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly))
    {
        return false;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    return true;
}

bool QueueManager::loadQueue(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError)
    {
        return false;
    }

    if (!doc.isObject())
    {
        return false;
    }

    QJsonObject json = doc.object();
    
    // Check if this is a collection file (has "tasks" array) or queue file (has "items" array)
    if (json.contains("tasks"))
    {
        return loadCollectionFromJson(json);
    }
    else
    {
        return fromJson(json);
    }
}

QJsonObject QueueManager::toJson() const
{
    QJsonObject json;

    json["version"] = "1.0";
    json["state"] = static_cast<int>(m_state);

    if (m_currentItem)
    {
        json["current_index"] = m_items.indexOf(m_currentItem);
    }

    QJsonArray itemsArray;
    for (QueueItem *item : m_items)
    {
        itemsArray.append(item->toJson());
    }
    json["items"] = itemsArray;

    return json;
}

bool QueueManager::fromJson(const QJsonObject &json)
{
    // Clear existing queue
    clear();

    // Load items
    QJsonArray itemsArray = json["items"].toArray();
    for (const QJsonValue &val : itemsArray)
    {
        QueueItem *item = new QueueItem(this);
        if (item->loadFromJson(val.toObject()))
        {
            m_items.append(item);
        }
        else
        {
            delete item;
        }
    }

    // Restore state
    m_state = static_cast<QueueState>(json["state"].toInt());

    // Restore current item
    if (json.contains("current_index"))
    {
        int index = json["current_index"].toInt();
        if (index >= 0 && index < m_items.size())
        {
            m_currentItem = m_items.at(index);
        }
    }

    // Return true if the JSON was successfully processed, regardless of whether any items were added.
    // Actual loading errors (file not found, malformed JSON) are handled earlier.
    return true;
}

int QueueManager::completedCount() const
{
    int count = 0;
    for (QueueItem *item : m_items)
    {
        if (item->status() == QueueItem::COMPLETED)
            count++;
    }
    return count;
}

int QueueManager::failedCount() const
{
    int count = 0;
    for (QueueItem *item : m_items)
    {
        if (item->status() == QueueItem::FAILED)
            count++;
    }
    return count;
}

int QueueManager::pendingCount() const
{
    int count = 0;
    for (QueueItem *item : m_items)
    {
        if (item->status() == QueueItem::PENDING ||
                item->status() == QueueItem::SCHEDULED)
            count++;
    }
    return count;
}

Task *QueueManager::createTaskFromTemplate(const QString &templateId, const QMap<QString, QVariant> &parameters)
{
    // Get the template manager instance
    TemplateManager *templateMgr = TemplateManager::Instance();
    if (!templateMgr)
        return nullptr;

    // Load the template
    TaskTemplate *tmpl = templateMgr->getTemplate(templateId);
    if (!tmpl)
        return nullptr;

    // Create a new task instance
    Task *task = new Task(this);

    // Instantiate the task from template with "Mount" device and provided parameters
    if (!task->instantiateFromTemplate(tmpl, "Mount", parameters))
    {
        delete task;
        return nullptr;
    }

    return task;
}

bool QueueManager::addMountParkTask(int timeout)
{
    QMap<QString, QVariant> params;
    params["wait_timeout"] = timeout;

    Task *task = createTaskFromTemplate("mount_park", params);
    if (!task)
        return false;

    QueueItem *item = new QueueItem(task, this);
    addItem(item);

    return true;
}

bool QueueManager::addMountUnparkTask(int timeout)
{
    QMap<QString, QVariant> params;
    params["wait_timeout"] = timeout;

    Task *task = createTaskFromTemplate("mount_unpark", params);
    if (!task)
        return false;

    QueueItem *item = new QueueItem(task, this);
    addItem(item);

    return true;
}

bool QueueManager::loadCollectionFromJson(const QJsonObject &json)
{    
    // Clear existing queue
    clear();

    // Get template manager and ensure it's initialized
    TemplateManager *templateMgr = TemplateManager::Instance();
    if (!templateMgr)
        return false;
    
    // Initialize template manager if not already done
    if (templateMgr->allTemplates().isEmpty())
    {
        if (!templateMgr->initialize())
            return false;
    }

    // Load tasks from collection
    QJsonArray tasksArray = json["tasks"].toArray();
    for (const QJsonValue &val : tasksArray)
    {
        QJsonObject taskJson = val.toObject();
        QString templateId = taskJson["template_id"].toString();
        QString deviceName = taskJson["device"].toString();
        
        // Get parameters
        QMap<QString, QVariant> params;
        QJsonObject paramsJson = taskJson["parameters"].toObject();
        for (const QString &key : paramsJson.keys())
        {
            params[key] = paramsJson[key].toVariant();
        }
                
        // Find the template
        TaskTemplate *tmpl = templateMgr->getTemplate(templateId);
        if (!tmpl)
            continue;
        
        // Create task from template
        Task *task = new Task(this);
        if (task->instantiateFromTemplate(tmpl, deviceName, params))
        {
            QueueItem *item = new QueueItem(task, this);
            m_items.append(item);
        }
        else
        {
            delete task;
        }
    }

    return true;
}

} // namespace Ekos
