/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "testekoslivemedia.h"

#include "ekos/ekoslive/commands.h"
#include "ekos/ekoslive/media.h"
#include "ekos/ekoslive/node.h"
#include "ekos/ekoslive/nodemanager.h"

#include <QBuffer>
#include <QHostAddress>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>
#include <QWebSocket>
#include <QWebSocketServer>

QTEST_GUILESS_MAIN(TestEkosLiveMedia)

namespace
{
// Media::METADATA_PACKET: the fixed-size metadata header in front of every image.
constexpr int METADATA_PACKET = 512;
// Media::HB_IMAGE_WIDTH
constexpr int MAX_WIDTH = 1920;
const QByteArray FLUSH_MARKER("flush");

// Same order as EkosLive::Client's managers.
enum { Online, Offline };
}

/// A websocket server that keeps the binary messages it receives.
class FakeServer
{
    public:
        FakeServer() : server(QStringLiteral("fake"), QWebSocketServer::NonSecureMode)
        {
            server.listen(QHostAddress::LocalHost, 0);
            QObject::connect(&server, &QWebSocketServer::newConnection, &server, [this]()
            {
                socket = server.nextPendingConnection();
                QObject::connect(socket, &QWebSocket::binaryMessageReceived, socket, [this](const QByteArray & message)
                {
                    received.append(message);
                });
            });
        }

        QUrl url() const
        {
            return QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.serverPort()));
        }

        // Binary messages received, without the flush markers.
        QList<QByteArray> images() const
        {
            QList<QByteArray> result;
            for (const auto &message : received)
                if (message != FLUSH_MARKER)
                    result.append(message);
            return result;
        }

        QWebSocketServer server;
        QWebSocket *socket { nullptr };
        QList<QByteArray> received;
};

namespace
{
QImage makeImage(int width, int height, QImage::Format format)
{
    QImage image(width, height, format);
    if (format == QImage::Format_Indexed8)
    {
        image.setColorCount(256);
        for (int i = 0; i < 256; i++)
            image.setColor(i, qRgb(i, i, i));
    }
    image.fill(0);
    return image;
}

QJsonObject header(const QByteArray &packet)
{
    QByteArray meta = packet.left(METADATA_PACKET);
    const int end = meta.indexOf('\0');
    if (end >= 0)
        meta.truncate(end);
    return QJsonDocument::fromJson(meta).object();
}

QImage payload(const QByteArray &packet)
{
    return QImage::fromData(packet.mid(METADATA_PACKET), "JPG");
}
}

void TestEkosLiveMedia::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void TestEkosLiveMedia::init()
{
    m_Online = new FakeServer();
    m_Offline = new FakeServer();
    QVERIFY(m_Online->server.isListening());
    QVERIFY(m_Offline->server.isListening());

    for (int i = 0; i < 2; i++)
    {
        QSharedPointer<EkosLive::NodeManager> manager(new EkosLive::NodeManager(EkosLive::NodeManager::Message |
            EkosLive::NodeManager::Media));
        manager->media()->setProperty("url", (i == Online ? m_Online : m_Offline)->url());
        m_Managers.append(manager);
    }
    m_Media = new EkosLive::Media(nullptr, m_Managers);
}

void TestEkosLiveMedia::cleanup()
{
    delete m_Media;
    m_Media = nullptr;
    m_Managers.clear();
    delete m_Online;
    delete m_Offline;
    m_Online = m_Offline = nullptr;
}

bool TestEkosLiveMedia::connectNodes()
{
    for (auto &manager : m_Managers)
    {
        QSignalSpy spy(manager->media(), &EkosLive::Node::connected);
        manager->media()->connectServer();
        if (!manager->media()->isConnected() && !spy.wait(5000))
            return false;
    }
    // Wait for the servers to accept, so flush() can rely on their sockets.
    return QTest::qWaitFor([this]()
    {
        return m_Online->socket && m_Offline->socket;
    }, 5000);
}

bool TestEkosLiveMedia::flush(int index, FakeServer *server)
{
    const int before = server->received.size();
    m_Managers[index]->media()->sendBinaryMessage(FLUSH_MARKER, true);
    return QTest::qWaitFor([server, before]()
    {
        return server->received.size() > before && server->received.last() == FLUSH_MARKER;
    }, 5000);
}

void TestEkosLiveMedia::testNotSentWhenDisconnected()
{
    m_Managers[Online]->media()->setClientState(true);
    QVERIFY(!m_Media->sendLiveStackImage(makeImage(800, 600, QImage::Format_RGB32), {{"frame", 1}}));
}

void TestEkosLiveMedia::testNotSentWithoutClient()
{
    // The device stays connected to the online server around the clock. With nobody on it,
    // the image must not even be encoded, let alone sent.
    QVERIFY(connectNodes());
    QVERIFY(!m_Media->sendLiveStackImage(makeImage(800, 600, QImage::Format_RGB32), {{"frame", 1}}));
    QVERIFY(flush(Online, m_Online));
    QVERIFY(m_Online->images().isEmpty());
}

void TestEkosLiveMedia::testSentToOnlineOnly_data()
{
    QTest::addColumn<int>("format");
    QTest::newRow("color") << static_cast<int>(QImage::Format_RGB32);
    QTest::newRow("mono") << static_cast<int>(QImage::Format_Indexed8);
}

void TestEkosLiveMedia::testSentToOnlineOnly()
{
    QFETCH(int, format);

    QVERIFY(connectNodes());
    // The App on the LAN is connected too. It gets stacked images from ekoslive-offline,
    // which reads them from disk, so a "+LS" packet there would show each one twice.
    m_Managers[Online]->media()->setClientState(true);
    m_Managers[Offline]->media()->setClientState(true);

    const QJsonObject metadata{{"frame", 7}, {"target", "M 31"}, {"filter", "L"}};
    QVERIFY(m_Media->sendLiveStackImage(makeImage(4000, 3000, static_cast<QImage::Format>(format)), metadata));

    QVERIFY(flush(Online, m_Online));
    QVERIFY(flush(Offline, m_Offline));
    QCOMPARE(m_Offline->images().size(), 0);
    QCOMPARE(m_Online->images().size(), 1);

    const QByteArray packet = m_Online->images().first();
    const QJsonObject meta = header(packet);
    QCOMPARE(meta["uuid"].toString(), QStringLiteral("+LS"));
    QCOMPARE(meta["ext"].toString(), QStringLiteral("jpg"));
    QCOMPARE(meta["frame"].toInt(), 7);
    QCOMPARE(meta["target"].toString(), QStringLiteral("M 31"));

    const QImage image = payload(packet);
    QVERIFY(!image.isNull());
    QCOMPARE(image.width(), MAX_WIDTH);
    QCOMPARE(image.height(), 1440);
}

void TestEkosLiveMedia::testNotSentToOfflineAlone()
{
    // Only the App on the LAN is watching: the stacked files on disk already reach it.
    QVERIFY(connectNodes());
    m_Managers[Offline]->media()->setClientState(true);

    QVERIFY(!m_Media->sendLiveStackImage(makeImage(800, 600, QImage::Format_RGB32), {{"frame", 1}}));
    QVERIFY(flush(Offline, m_Offline));
    QVERIFY(m_Offline->images().isEmpty());
}

void TestEkosLiveMedia::testNotSentWhenBlobsDisabled()
{
    QVERIFY(connectNodes());
    m_Managers[Online]->media()->setClientState(true);

    auto setBlobs = [this](bool enabled)
    {
        QSignalSpy spy(m_Managers[Online]->media(), &EkosLive::Node::onTextReceived);
        m_Online->socket->sendTextMessage(QJsonDocument(QJsonObject{{"type", EkosLive::commands[EkosLive::SET_BLOBS]},
            {"payload", enabled}}).toJson(QJsonDocument::Compact));
        return spy.wait(5000);
    };

    QVERIFY(setBlobs(false));
    QVERIFY(!m_Media->sendLiveStackImage(makeImage(800, 600, QImage::Format_RGB32), {{"frame", 1}}));

    QVERIFY(setBlobs(true));
    QVERIFY(m_Media->sendLiveStackImage(makeImage(800, 600, QImage::Format_RGB32), {{"frame", 2}}));

    QVERIFY(flush(Online, m_Online));
    QCOMPARE(m_Online->images().size(), 1);
    QCOMPARE(header(m_Online->images().first())["frame"].toInt(), 2);
}

void TestEkosLiveMedia::testSmallImageNotUpscaled()
{
    QVERIFY(connectNodes());
    m_Managers[Online]->media()->setClientState(true);

    QVERIFY(m_Media->sendLiveStackImage(makeImage(800, 600, QImage::Format_RGB32), {{"frame", 1}}));
    QVERIFY(flush(Online, m_Online));
    QCOMPARE(m_Online->images().size(), 1);
    QCOMPARE(payload(m_Online->images().first()).size(), QSize(800, 600));
}

void TestEkosLiveMedia::testWorstCaseMetadataFits()
{
    // What Message::sendLiveStackerImage() sends at its largest: buildPreviewMetadata() with
    // full-precision statistics, plus a 64-character target and a 32-character filter.
    QVERIFY(connectNodes());
    m_Managers[Online]->media()->setClientState(true);

    const QJsonObject metadata
    {
        {"resolution", "14208x10656"},
        {"size", static_cast<qint64>(605708288)},
        {"channels", 3},
        {"bpp", -32},
        {"mean", 1234.5678901234567},
        {"median", 1234.5678901234567},
        {"stddev", 123.45678901234567},
        {"min", -0.12345678901234567},
        {"max", 65535.123456789012},
        {"hasWCS", false},
        {"frame", 9999},
        {"filename", "stacked_frame_9999.fits"},
        {"target", QString(64, QLatin1Char('T'))},
        {"filter", QString(32, QLatin1Char('F'))}
    };
    QVERIFY(m_Media->sendLiveStackImage(makeImage(800, 600, QImage::Format_RGB32), metadata));
    QVERIFY(flush(Online, m_Online));
    QCOMPARE(m_Online->images().size(), 1);
    QCOMPARE(header(m_Online->images().first())["target"].toString(), QString(64, QLatin1Char('T')));
}

void TestEkosLiveMedia::testOversizedMetadataNotSent()
{
    // A header past METADATA_PACKET would run into the JPEG and the server could not parse either.
    QVERIFY(connectNodes());
    m_Managers[Online]->media()->setClientState(true);

    const QJsonObject metadata{{"frame", 1}, {"target", QString(METADATA_PACKET, QLatin1Char('T'))}};
    QVERIFY(!m_Media->sendLiveStackImage(makeImage(800, 600, QImage::Format_RGB32), metadata));
    QVERIFY(flush(Online, m_Online));
    QVERIFY(m_Online->images().isEmpty());
}
