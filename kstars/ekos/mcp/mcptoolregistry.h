/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QList>
#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <functional>

namespace MCP
{

struct ToolParam
{
    QString name;
    QString type;        // "string", "number", "boolean", "integer"
    QString description;
    bool required { true };
};

struct ToolDefinition
{
    QString name;
    QString description;
    QList<ToolParam> params;
    std::function<QJsonValue(const QJsonObject &args, QString &error)> handler;

    // MCP annotation hints (spec 2025-03-26) — all default to false
    QString title {};                 // optional human-readable title
    bool readOnly    { false };       // readOnlyHint: tool only observes, never mutates
    bool destructive { false };       // destructiveHint: mutation is hard to undo
    bool idempotent  { false };       // idempotentHint: safe to call multiple times
    bool openWorld   { false };       // openWorldHint: reaches network/disk beyond the rig
};

class ToolRegistry : public QObject
{
        Q_OBJECT

    public:
        explicit ToolRegistry(QObject *parent = nullptr);

        void registerTool(const ToolDefinition &tool);
        void classify(const QString &name, bool readOnly,
                      bool destructive = false, bool idempotent = false, bool openWorld = false);
        // Tools whose name appears in @p excluded are omitted from the list.
        QJsonArray toolsList(const QStringList &excluded = QStringList()) const;
        QJsonValue dispatch(const QString &name, const QJsonObject &args, QString &error) const;
        const ToolDefinition *find(const QString &name) const;

        // Read-only view of every registered tool, in registration order.
        // Used by the Ekos settings dialog to render the discoverable tool
        // tree. Iteration only — do not store references across server
        // restarts.
        const QList<ToolDefinition> &tools() const
        {
            return m_tools;
        }

    private:
        QList<ToolDefinition> m_tools;
};

} // namespace MCP
