/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mcptoolregistry.h"

namespace MCP
{

ToolRegistry::ToolRegistry(QObject *parent) : QObject(parent)
{
}

void ToolRegistry::registerTool(const ToolDefinition &tool)
{
    m_tools.append(tool);
}

void ToolRegistry::classify(const QString &name, bool readOnly,
                            bool destructive, bool idempotent, bool openWorld)
{
    for (auto &tool : m_tools)
    {
        if (tool.name == name)
        {
            tool.readOnly    = readOnly;
            tool.destructive = destructive;
            tool.idempotent  = idempotent;
            tool.openWorld   = openWorld;
            return;
        }
    }
}

QJsonArray ToolRegistry::toolsList() const
{
    QJsonArray result;
    for (const auto &tool : m_tools)
    {
        QJsonObject properties;
        QJsonArray required;

        for (const auto &param : tool.params)
        {
            QJsonObject propDef;
            propDef["type"]        = param.type;
            propDef["description"] = param.description;
            properties[param.name] = propDef;

            if (param.required)
                required.append(param.name);
        }

        QJsonObject schema;
        schema["type"]       = "object";
        schema["properties"] = properties;
        if (!required.isEmpty())
            schema["required"] = required;

        QJsonObject annotations;
        if (!tool.title.isEmpty())
            annotations["title"]         = tool.title;
        annotations["readOnlyHint"]    = tool.readOnly;
        annotations["destructiveHint"] = tool.destructive;
        annotations["idempotentHint"]  = tool.idempotent;
        annotations["openWorldHint"]   = tool.openWorld;

        QJsonObject toolObj;
        toolObj["name"]        = tool.name;
        toolObj["description"] = tool.description;
        toolObj["inputSchema"] = schema;
        toolObj["annotations"] = annotations;

        result.append(toolObj);
    }
    return result;
}

QJsonValue ToolRegistry::dispatch(const QString &name, const QJsonObject &args, QString &error) const
{
    for (const auto &tool : m_tools)
    {
        if (tool.name == name)
            return tool.handler(args, error);
    }
    error = QString("Tool not found: %1").arg(name);
    return QJsonValue();
}

const ToolDefinition *ToolRegistry::find(const QString &name) const
{
    for (const auto &tool : m_tools)
    {
        if (tool.name == name)
            return &tool;
    }
    return nullptr;
}

} // namespace MCP
