/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QByteArray>
#include <QHash>
#include <QHostAddress>
#include <QList>
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>

/**
 * @class MiniHttpServer
 * @short A local HTTP server for tests of network fallbacks, listening on 127.0.0.1 only.
 *
 * It answers every GET request in the same way: with a body, with "404 Not Found", or not at all
 * (the connection stays open, like a server that hangs). Requests are processed by the event loop
 * of the thread that created the server, so a test can wait for the client with QSignalSpy or QTRY_*.
 */
class MiniHttpServer : public QTcpServer
{
    public:
        enum Mode
        {
            Reply,      ///< 200 OK with the body
            NotFound,   ///< 404 Not Found
            Silent      ///< Accept the connection and never answer
        };

        explicit MiniHttpServer(Mode mode, const QByteArray &body = QByteArray(),
                                const QByteArray &contentType = "text/plain") :
            m_Mode(mode), m_Body(body), m_ContentType(contentType)
        {
            connect(this, &QTcpServer::newConnection, this, [this]()
            {
                while (QTcpSocket *socket = nextPendingConnection())
                {
                    connect(socket, &QTcpSocket::readyRead, socket, [this, socket]()
                    {
                        handle(socket);
                    });
                }
            });
            listen(QHostAddress::LocalHost, 0);
        }

        /** @brief url URL of the server with the given path, e.g. url("/simbad"). */
        QString url(const QString &path = QString()) const
        {
            return QString("http://127.0.0.1:%1%2").arg(serverPort()).arg(path);
        }

        /** @brief requests Number of complete requests received. */
        int requests() const
        {
            return m_Requests;
        }

        /** @brief lastPath Path and query of the last request. */
        const QByteArray &lastPath() const
        {
            return m_LastPath;
        }

    private:
        void handle(QTcpSocket *socket)
        {
            QByteArray &buffer = m_Buffers[socket];
            buffer += socket->readAll();
            if (!buffer.contains("\r\n\r\n"))
                return;

            m_Requests++;
            const QList<QByteArray> requestLine = buffer.left(buffer.indexOf("\r\n")).split(' ');
            m_LastPath = requestLine.value(1);
            buffer.clear();

            switch (m_Mode)
            {
                case Reply:
                    socket->write("HTTP/1.1 200 OK\r\nContent-Type: " + m_ContentType + "\r\nContent-Length: " +
                                  QByteArray::number(m_Body.size()) + "\r\nConnection: close\r\n\r\n" + m_Body);
                    socket->disconnectFromHost();
                    break;
                case NotFound:
                    socket->write("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                    socket->disconnectFromHost();
                    break;
                case Silent:
                    break;
            }
        }

        Mode m_Mode;
        QByteArray m_Body;
        QByteArray m_ContentType;
        int m_Requests { 0 };
        QByteArray m_LastPath;
        QHash<QTcpSocket *, QByteArray> m_Buffers;
};
