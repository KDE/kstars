/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

class QTcpSocket;

class MCPTestClient
{
    public:
        explicit MCPTestClient(quint16 port, const QString &token = {});
        ~MCPTestClient();

        QJsonObject post(const QJsonObject &request);

        bool openSSE();
        QList<QJsonObject> readSSEEvents(int waitMs = 500);

    private:
        quint16 m_port;
        QString m_token;
        QTcpSocket *m_sseSocket { nullptr };

        QByteArray buildRequest(const QString &method, const QString &path,
                                const QByteArray &body = {}) const;
        QByteArray extractBody(const QByteArray &response) const;
};
