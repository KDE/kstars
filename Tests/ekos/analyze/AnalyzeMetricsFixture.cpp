/*
    SPDX-FileCopyrightText: 2026 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ekos/analyze/analyzemetrics.h"
#include "ekos/analyze/openmetricsserver.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QHostAddress>
#include <QTextStream>

#include <cstdio>

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    const quint16 port = application.arguments().size() > 1 ? application.arguments().at(1).toUShort() : 9108;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    Ekos::AnalyzeMetrics metrics(QStringLiteral("integration-test"));
    metrics.reset(now - 60000);
    metrics.captureStarting(now - 50000, 30, QStringLiteral("L"));
    metrics.guideState(QStringLiteral("guiding"));
    metrics.guideStats(now - 49000, 0.4, -0.2, 80, -60, 14, 400, 10);
    metrics.guideStats(now - 48000, -0.3, 0.1, -70, 50, 15, 398, 11);
    metrics.captureComplete(now - 20000, 30, QStringLiteral("L"), 1.8, 35, 1200, 0.4);
    metrics.schedulerJobStarted(now - 10000, QStringLiteral("Integration target"));

    Ekos::OpenMetricsServer server([&metrics]()
    {
        return metrics.render(QDateTime::currentMSecsSinceEpoch());
    });
    QString error;
    if (!server.start(QHostAddress::AnyIPv4, port, &error))
    {
        QTextStream stream(stderr);
        stream << error << '\n';
        stream.flush();
        return 1;
    }
    QTextStream stream(stdout);
    stream << "http://0.0.0.0:" << server.port() << "/metrics\n";
    stream.flush();
    return application.exec();
}
