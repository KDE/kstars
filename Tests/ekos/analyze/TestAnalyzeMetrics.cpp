/*
    SPDX-FileCopyrightText: 2026 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ekos/analyze/analyzemetrics.h"
#include "ekos/analyze/openmetricsserver.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QHostAddress>
#include <QTcpSocket>
#include <QTest>
#include <QtAlgorithms>

using Ekos::AnalyzeMetrics;
using Ekos::OpenMetricsServer;

class TestAnalyzeMetrics : public QObject
{
        Q_OBJECT

    private Q_SLOTS:
        void testEmptyAndReset();
        void testDescriptors();
        void testCaptureAndFocus();
        void testGuideMountAndScheduler();
        void testEscapingAndFilterBound();
        void testEndpointLifecycle();
        void testEndpointProtocol();
        void testFragmentedAndOversizedRequests();
        void testConnectionLimitAndTimeout();

    private:
        static QByteArray request(quint16 port, const QList<QByteArray> &fragments);
};

QByteArray TestAnalyzeMetrics::request(quint16 port, const QList<QByteArray> &fragments)
{
    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, port);
    if (!socket.waitForConnected(3000))
        return QByteArray();
    for (const auto &fragment : fragments)
    {
        socket.write(fragment);
        socket.flush();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }

    QElapsedTimer timer;
    timer.start();
    while (socket.bytesAvailable() == 0 && timer.elapsed() < 3000)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        socket.waitForReadyRead(20);
    }
    QByteArray response;
    while (timer.elapsed() < 3000)
    {
        response += socket.readAll();
        if (socket.state() == QAbstractSocket::UnconnectedState)
            break;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        socket.waitForReadyRead(20);
    }
    response += socket.readAll();
    return response;
}

void TestAnalyzeMetrics::testEmptyAndReset()
{
    AnalyzeMetrics metrics(QStringLiteral("test-version"));
    QByteArray output = metrics.render(2000);
    QVERIFY(output.contains("kstars_info{version=\"test-version\"} 1"));
    QVERIFY(!output.contains("kstars_ekos_session_start_time_seconds 0"));
    QVERIFY(output.endsWith("# EOF\n"));

    metrics.reset(1000);
    metrics.captureStarting(1100, 30, QStringLiteral("L"));
    metrics.captureComplete(2100, 30, QStringLiteral("L"), 2.1, 20, 1200, 0.42);
    output = metrics.render(3000);
    QVERIFY(output.contains("kstars_ekos_capture_results_total{filter=\"L\",result=\"completed\"} 1"));

    metrics.reset(4000);
    output = metrics.render(5000);
    QVERIFY(!output.contains("filter=\"L\""));
    QVERIFY(output.contains("kstars_ekos_session_start_time_seconds 4"));
    QVERIFY(output.contains("kstars_ekos_session_elapsed_seconds 1"));
}

void TestAnalyzeMetrics::testDescriptors()
{
    AnalyzeMetrics metrics;
    metrics.reset(0);
    metrics.captureStarting(10, 1, QStringLiteral("L"));
    metrics.captureComplete(20, 1, QStringLiteral("L"), 1, 1, 1, 1);
    const QByteArray output = metrics.render(100);
    QCOMPARE(output.count("# EOF\n"), 1);

    const QList<QByteArray> lines = output.split('\n');
    for (const auto &line : lines)
    {
        if (line.isEmpty() || line.startsWith('#'))
            continue;
        QByteArray name = line.left(line.indexOf(' '));
        const int labels = name.indexOf('{');
        if (labels >= 0)
            name.truncate(labels);
        if (name.endsWith("_total"))
            name.chop(6);
        QVERIFY2(output.contains("# TYPE " + name + ' '), line.constData());
        QVERIFY2(output.contains("# HELP " + name + ' '), line.constData());
    }
}

void TestAnalyzeMetrics::testCaptureAndFocus()
{
    AnalyzeMetrics metrics;
    metrics.reset(100000);
    metrics.captureStarting(101000, 60, QStringLiteral("Ha"));
    QByteArray output = metrics.render(106000);
    QVERIFY(output.contains("kstars_ekos_capture_active 1"));
    QVERIFY(output.contains("kstars_ekos_capture_elapsed_seconds 5"));

    metrics.guideStats(102000, 1, -1, 20, -30, 12, 450, 9);
    metrics.guideStats(103000, -1, 1, -20, 30, 11, 451, 8);
    metrics.captureComplete(161000, 60, QStringLiteral("Ha"), 1.9, 42, 1300, 0.38);
    metrics.captureStarting(162000, 30, QStringLiteral("OIII"));
    metrics.captureAborted(165000, QStringLiteral("OIII"));

    metrics.autofocusStarting(170000, QStringLiteral("Ha"));
    metrics.autofocusComplete(176000, QStringLiteral("Ha"), 24100);
    metrics.autofocusStarting(180000, QStringLiteral("OIII"));
    metrics.autofocusAborted(184000, QStringLiteral("OIII"));
    metrics.adaptiveFocusComplete(185000, QStringLiteral("Ha"), -12, 24088, true);
    metrics.temperature(-2.5);

    output = metrics.render(190000);
    QVERIFY(output.contains("kstars_ekos_capture_results_total{filter=\"Ha\",result=\"completed\"} 1"));
    QVERIFY(output.contains("kstars_ekos_capture_results_total{filter=\"OIII\",result=\"aborted\"} 1"));
    QVERIFY(output.contains("kstars_ekos_capture_exposure_seconds_total{filter=\"Ha\"} 60"));
    QVERIFY(output.contains("kstars_ekos_capture_hfr 1.9"));
    QVERIFY(output.contains("kstars_ekos_capture_detected_stars 42"));
    QVERIFY(output.contains("kstars_ekos_guide_capture_rms_arcseconds"));
    QVERIFY(output.contains("kstars_ekos_autofocus_runs_total{filter=\"Ha\",result=\"completed\"} 1"));
    QVERIFY(output.contains("kstars_ekos_focus_position_steps 24088"));
    QVERIFY(output.contains("kstars_ekos_focuser_temperature_celsius -2.5"));
}

void TestAnalyzeMetrics::testGuideMountAndScheduler()
{
    AnalyzeMetrics metrics;
    metrics.reset(0);
    metrics.guideState(QStringLiteral("guiding"));
    metrics.guideStats(1000, 0.5, -0.25, 100, -80, 15, 500, 12);
    metrics.guideStats(2000, -0.5, 0.25, -100, 80, 14, 501, 11);
    metrics.alignState(QStringLiteral("In Progress"));
    metrics.alignState(QStringLiteral("Complete"));
    metrics.mountState(QStringLiteral("Tracking"));
    metrics.mountCoordinates(12, -30, 180, 45, 0, 15);
    metrics.meridianFlipState(QStringLiteral("MOUNT_FLIP_RUNNING"));
    metrics.meridianFlipState(QStringLiteral("MOUNT_FLIP_COMPLETED"));
    metrics.schedulerJobStarted(3000, QStringLiteral("M42 sequence"));

    QByteArray output = metrics.render(8000);
    QVERIFY(output.contains("kstars_ekos_guide_state{state=\"guiding\"} 1"));
    QVERIFY(output.contains("kstars_ekos_guide_state_transitions_total{state=\"guiding\"} 1"));
    QVERIFY(output.contains("kstars_ekos_guide_rms_arcseconds"));
    QVERIFY(output.contains("kstars_ekos_align_results_total{result=\"completed\"} 1"));
    QVERIFY(output.contains("kstars_ekos_mount_state{state=\"tracking\"} 1"));
    QVERIFY(output.contains("kstars_ekos_mount_pier_side{side=\"west_to_east\"} 1"));
    QVERIFY(output.contains("kstars_ekos_meridian_flip_results_total{result=\"completed\"} 1"));
    QVERIFY(output.contains("kstars_ekos_scheduler_job_info{job_name=\"M42 sequence\"} 1"));
    QVERIFY(output.contains("kstars_ekos_scheduler_job_elapsed_seconds 5"));

    metrics.schedulerJobEnded(9000);
    output = metrics.render(10000);
    QVERIFY(!output.contains("job_name=\"M42 sequence\""));
    QVERIFY(output.contains("kstars_ekos_scheduler_job_last_duration_seconds 6"));
    QVERIFY(output.contains("kstars_ekos_scheduler_jobs_ended_total 1"));
}

void TestAnalyzeMetrics::testEscapingAndFilterBound()
{
    AnalyzeMetrics metrics;
    metrics.reset(0);
    metrics.schedulerJobStarted(0, QString::fromUtf8("M31 é\\\"\njob"));
    for (int index = 0; index < 40; ++index)
    {
        const QString filter = QStringLiteral("filter-%1").arg(index);
        metrics.captureStarting(index * 10, 1, filter);
        metrics.captureComplete(index * 10 + 1, 1, filter, 1, 1, 1, 1);
    }
    const QByteArray output = metrics.render(1000);
    QByteArray escapedJob = "job_name=\"M31 ";
    escapedJob += QString::fromUtf8("é").toUtf8();
    escapedJob += "\\\\\\\"\\njob\"";
    QVERIFY(output.contains(escapedJob));
    QVERIFY(output.contains("filter=\"other\""));
    QVERIFY(output.endsWith("# EOF\n"));
}

void TestAnalyzeMetrics::testEndpointLifecycle()
{
    OpenMetricsServer server([]()
    {
        return QByteArray("sample 1\n# EOF\n");
    });
    QString error;
    QVERIFY(server.start(QHostAddress::LocalHost, 0, &error));
    QVERIFY(server.isListening());
    QVERIFY(server.port() > 0);

    OpenMetricsServer conflict([]()
    {
        return QByteArray("# EOF\n");
    });
    QVERIFY(!conflict.start(QHostAddress::LocalHost, server.port(), &error));

    const quint16 firstPort = server.port();
    OpenMetricsServer occupied([]()
    {
        return QByteArray("# EOF\n");
    });
    QVERIFY(occupied.start(QHostAddress::LocalHost, 0));
    QVERIFY(!server.restart(QHostAddress::LocalHost, occupied.port(), &error));
    QVERIFY(server.isListening());
    QCOMPARE(server.port(), firstPort);
    server.stop();
    QVERIFY(!server.isListening());
}

void TestAnalyzeMetrics::testEndpointProtocol()
{
    OpenMetricsServer server([]()
    {
        return QByteArray("sample 1\n# EOF\n");
    });
    QVERIFY(server.start(QHostAddress::LocalHost, 0));

    QByteArray response = request(server.port(), { "GET /metrics HTTP/1.1\r\nHost: localhost\r\n\r\n" });
    QVERIFY(response.startsWith("HTTP/1.1 200 OK"));
    QVERIFY(response.contains("Content-Type: application/openmetrics-text; version=1.0.0; charset=utf-8"));
    QVERIFY(response.endsWith("sample 1\n# EOF\n"));

    response = request(server.port(), { "HEAD /metrics HTTP/1.1\r\nHost: localhost\r\n\r\n" });
    QVERIFY(response.startsWith("HTTP/1.1 200 OK"));
    QVERIFY(!response.endsWith("# EOF\n"));

    response = request(server.port(), { "GET /healthz HTTP/1.1\r\nHost: localhost\r\n\r\n" });
    QVERIFY(response.endsWith("ok\n"));
    response = request(server.port(), { "GET /missing HTTP/1.1\r\nHost: localhost\r\n\r\n" });
    QVERIFY(response.startsWith("HTTP/1.1 404 Not Found"));
    response = request(server.port(), { "POST /metrics HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n" });
    QVERIFY(response.startsWith("HTTP/1.1 405 Method Not Allowed"));
    QVERIFY(response.contains("Allow: GET, HEAD"));
}

void TestAnalyzeMetrics::testFragmentedAndOversizedRequests()
{
    OpenMetricsServer server([]()
    {
        return QByteArray("sample 1\n# EOF\n");
    });
    QVERIFY(server.start(QHostAddress::LocalHost, 0));
    QByteArray response = request(server.port(), { "GET /met", "rics HTTP/1.1\r\nHost: local", "host\r\n\r\n" });
    QVERIFY(response.startsWith("HTTP/1.1 200 OK"));

    QByteArray large = "GET /metrics HTTP/1.1\r\nX-Large: ";
    large += QByteArray(9000, 'x');
    response = request(server.port(), { large });
    QVERIFY(response.startsWith("HTTP/1.1 431 Request Header Fields Too Large"));
}

void TestAnalyzeMetrics::testConnectionLimitAndTimeout()
{
    OpenMetricsServer server([]()
    {
        return QByteArray("sample 1\n# EOF\n");
    });
    QVERIFY(server.start(QHostAddress::LocalHost, 0));

    QList<QTcpSocket *> waiting;
    for (int index = 0; index < 8; ++index)
    {
        auto *socket = new QTcpSocket(this);
        socket->connectToHost(QHostAddress::LocalHost, server.port());
        QVERIFY(socket->waitForConnected(3000));
        waiting.append(socket);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    const QByteArray busy = request(server.port(), { "GET /metrics HTTP/1.1\r\nHost: localhost\r\n\r\n" });
    QVERIFY(busy.startsWith("HTTP/1.1 503 Service Unavailable"));

    QTcpSocket timeoutSocket;
    waiting.first()->abort();
    delete waiting.takeFirst();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    timeoutSocket.connectToHost(QHostAddress::LocalHost, server.port());
    QVERIFY(timeoutSocket.waitForConnected(3000));
    QElapsedTimer timer;
    timer.start();
    while (timeoutSocket.bytesAvailable() == 0 && timer.elapsed() < 6500)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        timeoutSocket.waitForReadyRead(20);
    }
    QVERIFY(timeoutSocket.readAll().startsWith("HTTP/1.1 408 Request Timeout"));
    qDeleteAll(waiting);
}

QTEST_MAIN(TestAnalyzeMetrics)

#include "TestAnalyzeMetrics.moc"
