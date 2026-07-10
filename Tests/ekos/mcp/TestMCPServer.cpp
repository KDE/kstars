/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "TestMCPServer.h"
#include "MCPTestClient.h"
#include "ekos/mcp/mcpserver.h"
#include "ekos/mcp/mcptoolregistry.h"
#include "Options.h"

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonObject>
#include <QTcpSocket>
#include <QTest>

QTEST_MAIN(TestMCPServer)

void TestMCPServer::initTestCase()
{
    // Critical: the tests call setToken/regenerateToken/start, all of which
    // persist to the shared "kstars" keychain service used by the live app.
    // Without this, running the suite overwrites the user's real MCP tokens
    // (e.g. the full token becomes the literal "full-token"). Isolate it.
    MCP::Server::setKeychainPersistenceEnabled(false);
}

static quint16 startServer(MCP::Server &server)
{
    if (!server.start(0))
        return 0;
    return server.port();
}

void TestMCPServer::testInitialize()
{
    MCP::Server server;
    const quint16 port = startServer(server);
    MCPTestClient client(port, server.token());

    QJsonObject req;
    req["jsonrpc"] = "2.0";
    req["id"]      = 1;
    req["method"]  = "initialize";
    req["params"]  = QJsonObject{};

    QJsonObject resp = client.post(req);
    QVERIFY(!resp.isEmpty());
    QVERIFY(resp.contains("result"));

    QJsonObject result = resp["result"].toObject();
    QVERIFY(result.contains("protocolVersion"));
    QVERIFY(result.contains("serverInfo"));
    QCOMPARE(result["serverInfo"].toObject()["name"].toString(), QString("kstars-mcp"));
}

void TestMCPServer::testToolsList()
{
    MCP::Server server;
    // Register a dummy tool so list is non-trivially testable
    server.registry()->registerTool(
    {
        "test_tool", "A test tool", {}, [](const QJsonObject &, QString &) -> QJsonValue {
            return QJsonObject{ {"ok", true} };
        }
    });

    const quint16 port = startServer(server);
    MCPTestClient client(port, server.token());

    QJsonObject req;
    req["jsonrpc"] = "2.0";
    req["id"]      = 1;
    req["method"]  = "tools/list";

    QJsonObject resp = client.post(req);
    QVERIFY(!resp.isEmpty());
    QVERIFY(resp.contains("result"));

    QJsonObject result = resp["result"].toObject();
    QVERIFY(result.contains("tools"));
    QVERIFY(result["tools"].isArray());
    QVERIFY(result["tools"].toArray().size() > 0);
}

void TestMCPServer::testToolsCallUnknown()
{
    MCP::Server server;
    const quint16 port = startServer(server);
    MCPTestClient client(port, server.token());

    QJsonObject params;
    params["name"]      = "nonexistent_tool";
    params["arguments"] = QJsonObject{};

    QJsonObject req;
    req["jsonrpc"] = "2.0";
    req["id"]      = 1;
    req["method"]  = "tools/call";
    req["params"]  = params;

    QJsonObject resp = client.post(req);
    QVERIFY(!resp.isEmpty());
    QVERIFY(resp.contains("error"));
    QVERIFY(resp["error"].toObject().contains("code"));
}

void TestMCPServer::testToolsCallModuleUnavailable()
{
    MCP::Server server;

    // Register a tool that simulates a null module check
    server.registry()->registerTool(
    {
        "mount_coords", "Get mount coordinates", {},
        [](const QJsonObject &, QString & error) -> QJsonValue {
            error = "Mount module not available";
            return {};
        }
    });

    const quint16 port = startServer(server);
    MCPTestClient client(port, server.token());

    QJsonObject params;
    params["name"]      = "mount_coords";
    params["arguments"] = QJsonObject{};

    QJsonObject req;
    req["jsonrpc"] = "2.0";
    req["id"]      = 1;
    req["method"]  = "tools/call";
    req["params"]  = params;

    QJsonObject resp = client.post(req);
    QVERIFY(!resp.isEmpty());
    QVERIFY(resp.contains("error"));
    QString msg = resp["error"].toObject()["message"].toString();
    QVERIFY(msg.contains("not available") || msg.contains("Mount"));
}

void TestMCPServer::testInvalidJSON()
{
    MCP::Server server;
    startServer(server);
    const QString token = server.token();

    // Send raw invalid JSON via QTcpSocket (MCPTestClient doesn't support this directly)
    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, server.port());
    QVERIFY(socket.waitForConnected(3000));

    QByteArray body    = "this is not json {{{";
    QByteArray request = "POST /mcp HTTP/1.1\r\nHost: localhost\r\n"
                         "Authorization: Bearer " + token.toLatin1() + "\r\n"
                         "Content-Type: application/json\r\nContent-Length: "
                         + QByteArray::number(body.size()) + "\r\n\r\n" + body;
    socket.write(request);
    QTRY_VERIFY_WITH_TIMEOUT(socket.bytesAvailable() > 0, 3000);

    QByteArray response = socket.readAll();
    // Extract JSON body
    int sep = response.indexOf("\r\n\r\n");
    QVERIFY(sep >= 0);
    QJsonDocument doc = QJsonDocument::fromJson(response.mid(sep + 4));
    QVERIFY(doc.isObject());
    QJsonObject obj = doc.object();
    QVERIFY(obj.contains("error"));
    QCOMPARE(obj["error"].toObject()["code"].toInt(), -32700);
}

void TestMCPServer::testMissingMethod()
{
    MCP::Server server;
    const quint16 port = startServer(server);
    MCPTestClient client(port, server.token());

    QJsonObject req;
    req["jsonrpc"] = "2.0";
    req["id"]      = 1;
    // No "method" field — must be -32600 Invalid Request

    QJsonObject resp = client.post(req);
    QVERIFY(!resp.isEmpty());
    QVERIFY(resp.contains("error"));
    QCOMPARE(resp["error"].toObject()["code"].toInt(), -32600);
}

void TestMCPServer::testInvalidJsonRpcVersion()
{
    MCP::Server server;
    const quint16 port = startServer(server);
    MCPTestClient client(port, server.token());

    QJsonObject req;
    req["jsonrpc"] = "1.0";
    req["id"]      = 1;
    req["method"]  = "tools/list";

    QJsonObject resp = client.post(req);
    QVERIFY(!resp.isEmpty());
    QVERIFY(resp.contains("error"));
    QCOMPARE(resp["error"].toObject()["code"].toInt(), -32600);
}

void TestMCPServer::testTokenRegeneration()
{
    MCP::Server server;
    QVERIFY(server.start(0));

    const QString original = server.token();
    QVERIFY(!original.isEmpty());

    // A request with the original token must succeed
    MCPTestClient ok(server.port(), original);
    QJsonObject listReq{ {"jsonrpc", "2.0"}, {"id", 1}, {"method", "tools/list"} };
    QJsonObject resp = ok.post(listReq);
    QVERIFY(!resp.isEmpty());
    QVERIFY(resp.contains("result"));

    // Rotate the token
    server.regenerateToken();
    const QString rotated = server.token();
    QVERIFY(rotated != original);

    // A raw request with the OLD token must now get 401
    QTcpSocket s;
    s.connectToHost(QHostAddress::LocalHost, server.port());
    QVERIFY(s.waitForConnected(3000));
    QByteArray body = "{}";
    QByteArray r = "POST /mcp HTTP/1.1\r\nHost: localhost\r\n"
                   "Authorization: Bearer " + original.toLatin1() + "\r\n"
                   "Content-Type: application/json\r\n"
                   "Content-Length: " + QByteArray::number(body.size())
                   + "\r\n\r\n" + body;
    s.write(r);
    QTRY_VERIFY_WITH_TIMEOUT(s.bytesAvailable() > 0, 3000);
    QVERIFY(s.readAll().startsWith("HTTP/1.1 401"));
}

void TestMCPServer::testAnnotationsEmitted()
{
    MCP::Server server;
    server.registry()->registerTool(
    {
        "annotated_tool", "A test tool", {}, [](const QJsonObject &, QString &) -> QJsonValue {
            return QJsonObject{};
        }
    });
    server.registry()->classify("annotated_tool", /*ro*/true, /*destr*/false, /*idemp*/true);

    const quint16 port = startServer(server);
    MCPTestClient client(port, server.token());
    QJsonObject req{ {"jsonrpc", "2.0"}, {"id", 1}, {"method", "tools/list"} };
    QJsonObject resp = client.post(req);
    QVERIFY(resp.contains("result"));
    QJsonArray tools = resp["result"].toObject()["tools"].toArray();
    QVERIFY(!tools.isEmpty());

    // Find our tool and verify its annotations
    QJsonObject found;
    for (const auto &t : tools)
    {
        if (t.toObject()["name"].toString() == "annotated_tool")
        {
            found = t.toObject();
            break;
        }
    }
    QVERIFY2(!found.isEmpty(), "annotated_tool not found in tools/list");
    QVERIFY(found.contains("annotations"));
    QJsonObject ann = found["annotations"].toObject();
    QCOMPARE(ann["readOnlyHint"].toBool(),    true);
    QCOMPARE(ann["destructiveHint"].toBool(), false);
    QCOMPARE(ann["idempotentHint"].toBool(),  true);
    QCOMPARE(ann["openWorldHint"].toBool(),   false);
}

void TestMCPServer::testReadOnlyModeBlocks()
{
    MCP::Server server;

    // Register a read-only tool and a mutating tool
    server.registry()->registerTool(
    {
        "ro_tool", "Read-only tool", {}, [](const QJsonObject &, QString &) -> QJsonValue {
            return QJsonObject{{"ok", true}};
        }
    });
    server.registry()->classify("ro_tool", /*ro*/true);

    server.registry()->registerTool(
    {
        "mut_tool", "Mutating tool", {}, [](const QJsonObject &, QString &) -> QJsonValue {
            return QJsonObject{{"ok", true}};
        }
    });
    server.registry()->classify("mut_tool", /*ro*/false);

    Options::setMCPReadOnlyMode(true);
    const quint16 port = startServer(server);
    MCPTestClient client(port, server.token());

    auto callTool = [&](const QString & name) -> QJsonObject
    {
        QJsonObject params;
        params["name"]      = name;
        params["arguments"] = QJsonObject{};
        QJsonObject req{ {"jsonrpc", "2.0"}, {"id", 1}, {"method", "tools/call"}, {"params", params} };
        return client.post(req);
    };

    // Read-only tool must succeed
    QJsonObject roResp = callTool("ro_tool");
    QVERIFY2(roResp.contains("result"), "read-only tool should succeed in read-only mode");

    // Mutating tool must be blocked
    QJsonObject mutResp = callTool("mut_tool");
    QVERIFY2(mutResp.contains("error"), "mutating tool should be blocked in read-only mode");
    QCOMPARE(mutResp["error"].toObject()["code"].toInt(), -32601);

    Options::setMCPReadOnlyMode(false);
}

void TestMCPServer::testReadOnlyTokenGates()
{
    MCP::Server server;

    server.registry()->registerTool(
    {
        "ro_tool2", "Read-only tool", {}, [](const QJsonObject &, QString &) -> QJsonValue {
            return QJsonObject{{"ok", true}};
        }
    });
    server.registry()->classify("ro_tool2", /*ro*/true);

    server.registry()->registerTool(
    {
        "mut_tool2", "Mutating tool", {}, [](const QJsonObject &, QString &) -> QJsonValue {
            return QJsonObject{{"ok", true}};
        }
    });
    server.registry()->classify("mut_tool2", /*ro*/false);

    // Configure tokens explicitly (before start, so loadFromKeychain doesn't
    // overwrite them with a stored or fresh value).
    server.setToken("full-token");
    server.setReadOnlyToken("ro-token");
    const quint16 port = startServer(server);

    auto callWithToken = [&](const QString & token, const QString & toolName) -> QJsonObject
    {
        QJsonObject params;
        params["name"]      = toolName;
        params["arguments"] = QJsonObject{};
        QJsonObject req{ {"jsonrpc", "2.0"}, {"id", 1}, {"method", "tools/call"}, {"params", params} };
        MCPTestClient c(port, token);
        return c.post(req);
    };

    // Read-only token: can call ro_tool2, blocked from mut_tool2
    QVERIFY(callWithToken("ro-token",   "ro_tool2").contains("result"));
    QVERIFY(callWithToken("ro-token",   "mut_tool2").contains("error"));

    // Full token: can call both
    QVERIFY(callWithToken("full-token", "ro_tool2").contains("result"));
    QVERIFY(callWithToken("full-token", "mut_tool2").contains("result"));
}

