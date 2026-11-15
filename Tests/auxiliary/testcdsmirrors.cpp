/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "auxiliary/cdsmirrors.h"
#include "../shared/minihttpserver.h"

#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QSignalSpy>
#include <QtTest>

/**
 * @brief Tests the CDS host names, the mirror order and the mirror failover.
 * See bug 528778: the old CDS host names no longer resolve and the CDS servers were down for days.
 */
class TestCDSMirrors : public QObject
{
        Q_OBJECT

    private Q_SLOTS:
        void initTestCase();
        void cleanup();

        void testUpgradeLegacyUrl_data();
        void testUpgradeLegacyUrl();

        void testDefaultBaseUrlsUseCurrentHosts_data();
        void testDefaultBaseUrlsUseCurrentHosts();
        void testReportSuccessMovesMirrorFirst();
        void testReportSuccessIgnoresUnknownUrl();
        void testSetBaseUrlsForTesting();

        void testSesameUrl();
        void testSimbadUrl();
        void testDefaultHipsMirrors();

        void testFailoverSwitchesAfterThreshold();
        void testFailoverSuccessResetsCount();
        void testFailoverIgnoresStaleResults();
        void testFailoverWrapsAround();
        void testFailoverSingleUrlNeverSwitches();
        void testFailoverDropsEmptyAndDuplicateUrls();

        void testAbortAfterCancelsHangingReply();
        void testAbortAfterKeepsFinishedReply();
};

void TestCDSMirrors::initTestCase()
{
    // Tests only talk to 127.0.0.1
    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
}

void TestCDSMirrors::cleanup()
{
    CDSMirrors::setBaseUrlsForTesting(CDSMirrors::Service::Sesame, {});
    CDSMirrors::setBaseUrlsForTesting(CDSMirrors::Service::Simbad, {});
    CDSMirrors::setBaseUrlsForTesting(CDSMirrors::Service::MocServer, {});
    CDSMirrors::setRequestTimeoutForTesting(0);
}

void TestCDSMirrors::testUpgradeLegacyUrl_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QString>("expected");

    // Default surveys of KStars 3.8.5 and older
    QTest::newRow("dss u-strasbg") << "http://alasky.u-strasbg.fr/DSS/DSSColor"
                                   << "https://alasky.cds.unistra.fr/DSS/DSSColor";
    QTest::newRow("2mass alaskybis") << "http://alaskybis.u-strasbg.fr/2MASS/Color"
                                     << "https://alaskybis.cds.unistra.fr/2MASS/Color";
    QTest::newRow("fermi alaskybis") << "http://alaskybis.u-strasbg.fr/Fermi/Color"
                                     << "https://alaskybis.cds.unistra.fr/Fermi/Color";
    // Host of the old MocServer URL
    QTest::newRow("alasky unistra") << "http://alasky.unistra.fr/MocServer/query?get=record"
                                    << "https://alasky.cds.unistra.fr/MocServer/query?get=record";
    QTest::newRow("upper case host") << "http://ALASKY.U-STRASBG.FR/DSS/DSSColor"
                                     << "https://alasky.cds.unistra.fr/DSS/DSSColor";
    QTest::newRow("explicit http port") << "http://alasky.u-strasbg.fr:80/DSS/DSSColor"
                                        << "https://alasky.cds.unistra.fr/DSS/DSSColor";
    QTest::newRow("current host over http") << "http://alasky.cds.unistra.fr/DSS/DSSColor"
                                            << "https://alasky.cds.unistra.fr/DSS/DSSColor";
    QTest::newRow("simbad") << "https://simbad.u-strasbg.fr/simbad/sim-id"
                            << "https://simbad.cds.unistra.fr/simbad/sim-id";
    // URLs that must not change
    QTest::newRow("current host") << "https://alasky.cds.unistra.fr/DSS/DSSColor"
                                  << "https://alasky.cds.unistra.fr/DSS/DSSColor";
    QTest::newRow("irsa mirror") << "https://irsa.ipac.caltech.edu/data/hips/CDS/DSS2/color"
                                 << "https://irsa.ipac.caltech.edu/data/hips/CDS/DSS2/color";
    QTest::newRow("other http server") << "http://example.org/hips/DSS" << "http://example.org/hips/DSS";
    QTest::newRow("local file") << "file:///opt/hips" << "file:///opt/hips";
    QTest::newRow("empty") << "" << "";
}

void TestCDSMirrors::testUpgradeLegacyUrl()
{
    QFETCH(QString, url);
    QFETCH(QString, expected);
    QCOMPARE(CDSMirrors::upgradeLegacyUrl(url), expected);
}

void TestCDSMirrors::testDefaultBaseUrlsUseCurrentHosts_data()
{
    QTest::addColumn<int>("service");
    QTest::newRow("sesame") << static_cast<int>(CDSMirrors::Service::Sesame);
    QTest::newRow("simbad") << static_cast<int>(CDSMirrors::Service::Simbad);
    QTest::newRow("mocserver") << static_cast<int>(CDSMirrors::Service::MocServer);
}

void TestCDSMirrors::testDefaultBaseUrlsUseCurrentHosts()
{
    QFETCH(int, service);
    const QStringList urls = CDSMirrors::baseUrls(static_cast<CDSMirrors::Service>(service));

    // A CDS server first, then at least one mirror
    QVERIFY(urls.count() >= 2);
    QVERIFY2(QUrl(urls.first()).host().endsWith(".cds.unistra.fr"), qPrintable(urls.first()));
    for (const auto &url : urls)
    {
        QCOMPARE(QUrl(url).scheme(), QString("https"));
        QVERIFY2(!url.contains("u-strasbg.fr"), qPrintable(url));
        QCOMPARE(CDSMirrors::upgradeLegacyUrl(url), url);
    }
}

void TestCDSMirrors::testReportSuccessMovesMirrorFirst()
{
    const QStringList defaults = CDSMirrors::baseUrls(CDSMirrors::Service::Sesame);
    QCOMPARE(CDSMirrors::preferredBaseUrl(CDSMirrors::Service::Sesame), defaults.first());

    CDSMirrors::reportSuccess(CDSMirrors::Service::Sesame, defaults.last());
    const QStringList reordered = CDSMirrors::baseUrls(CDSMirrors::Service::Sesame);
    QCOMPARE(reordered.first(), defaults.last());
    QCOMPARE(reordered.count(), defaults.count());
    QCOMPARE(CDSMirrors::preferredBaseUrl(CDSMirrors::Service::Sesame), defaults.last());

    // Other services keep their order
    QCOMPARE(CDSMirrors::baseUrls(CDSMirrors::Service::Simbad).first(),
             QString("https://simbad.cds.unistra.fr"));

    // Once CDS works again, it comes first again
    CDSMirrors::reportSuccess(CDSMirrors::Service::Sesame, defaults.first());
    QCOMPARE(CDSMirrors::baseUrls(CDSMirrors::Service::Sesame), defaults);
}

void TestCDSMirrors::testReportSuccessIgnoresUnknownUrl()
{
    const QStringList defaults = CDSMirrors::baseUrls(CDSMirrors::Service::Simbad);
    CDSMirrors::reportSuccess(CDSMirrors::Service::Simbad, "https://example.org");
    QCOMPARE(CDSMirrors::baseUrls(CDSMirrors::Service::Simbad), defaults);
}

void TestCDSMirrors::testSetBaseUrlsForTesting()
{
    const QStringList defaults = CDSMirrors::baseUrls(CDSMirrors::Service::MocServer);
    const QStringList custom { "http://127.0.0.1:1/a", "http://127.0.0.1:2/b" };

    CDSMirrors::setBaseUrlsForTesting(CDSMirrors::Service::MocServer, custom);
    QCOMPARE(CDSMirrors::baseUrls(CDSMirrors::Service::MocServer), custom);
    CDSMirrors::reportSuccess(CDSMirrors::Service::MocServer, custom.last());
    QCOMPARE(CDSMirrors::baseUrls(CDSMirrors::Service::MocServer).first(), custom.last());

    CDSMirrors::setBaseUrlsForTesting(CDSMirrors::Service::MocServer, {});
    QCOMPARE(CDSMirrors::baseUrls(CDSMirrors::Service::MocServer), defaults);

    CDSMirrors::setRequestTimeoutForTesting(250);
    QCOMPARE(CDSMirrors::requestTimeoutMs(), 250);
    CDSMirrors::setRequestTimeoutForTesting(0);
    QCOMPARE(CDSMirrors::requestTimeoutMs(), CDSMirrors::DefaultRequestTimeoutMs);
}

void TestCDSMirrors::testSesameUrl()
{
    const QUrl url = CDSMirrors::sesameUrl("https://vizier.cfa.harvard.edu/viz-bin/nph-sesame", "NGC 7000");
    QCOMPARE(url.host(), QString("vizier.cfa.harvard.edu"));
    QCOMPARE(url.path(), QString("/viz-bin/nph-sesame/-oxpFI/SNV"));
    QCOMPARE(url.query(QUrl::FullyDecoded), QString("NGC 7000"));
}

void TestCDSMirrors::testSimbadUrl()
{
    const QUrl url = CDSMirrors::simbadUrl("https://simbad.cfa.harvard.edu", "/simbad/sim-coo");
    QCOMPARE(url.toString(), QString("https://simbad.cfa.harvard.edu/simbad/sim-coo"));
}

void TestCDSMirrors::testDefaultHipsMirrors()
{
    for (const QString &id : QStringList{"CDS/P/DSS2/color", "CDS/P/2MASS/color", "CDS/P/Fermi/color"})
    {
        const QStringList mirrors = CDSMirrors::defaultHipsMirrors(id);
        QVERIFY2(mirrors.count() >= 2, qPrintable(id));
        // At least one mirror that is not run by CDS
        bool external = false;
        for (const auto &mirror : mirrors)
        {
            QCOMPARE(QUrl(mirror).scheme(), QString("https"));
            QVERIFY2(!mirror.contains(CDSMirrors::HipsMirrorSeparator), qPrintable(mirror));
            external |= !QUrl(mirror).host().endsWith("unistra.fr");
        }
        QVERIFY2(external, qPrintable(id));
    }
    QVERIFY(CDSMirrors::defaultHipsMirrors("CDS/P/Mellinger/color").isEmpty());
}

void TestCDSMirrors::testFailoverSwitchesAfterThreshold()
{
    CDSMirrors::MirrorFailover failover(3);
    failover.setUrls({"https://a", "https://b"});
    QCOMPARE(failover.current(), QString("https://a"));

    QVERIFY(!failover.reportResult("https://a", false));
    QVERIFY(!failover.reportResult("https://a", false));
    QVERIFY(failover.reportResult("https://a", false));
    QCOMPARE(failover.current(), QString("https://b"));
    QCOMPARE(failover.currentIndex(), 1);
}

void TestCDSMirrors::testFailoverSuccessResetsCount()
{
    CDSMirrors::MirrorFailover failover(3);
    failover.setUrls({"https://a", "https://b"});

    // A partial outage, where some tiles still load, does not switch
    for (int i = 0; i < 10; i++)
    {
        QVERIFY(!failover.reportResult("https://a", false));
        QVERIFY(!failover.reportResult("https://a", false));
        QVERIFY(!failover.reportResult("https://a", true));
    }
    QCOMPARE(failover.current(), QString("https://a"));
}

void TestCDSMirrors::testFailoverIgnoresStaleResults()
{
    CDSMirrors::MirrorFailover failover(2);
    failover.setUrls({"https://a", "https://b", "https://c"});

    QVERIFY(!failover.reportResult("https://a", false));
    QVERIFY(failover.reportResult("https://a", false));
    QCOMPARE(failover.current(), QString("https://b"));

    // Many tiles were still on their way from the first server when it was dropped
    for (int i = 0; i < 20; i++)
        QVERIFY(!failover.reportResult("https://a", false));
    QCOMPARE(failover.current(), QString("https://b"));

    // Unknown servers are ignored as well
    QVERIFY(!failover.reportResult("https://x", false));
    QVERIFY(!failover.reportResult("https://x", false));
    QCOMPARE(failover.current(), QString("https://b"));
}

void TestCDSMirrors::testFailoverWrapsAround()
{
    CDSMirrors::MirrorFailover failover(1);
    failover.setUrls({"https://a", "https://b"});

    QVERIFY(failover.reportResult("https://a", false));
    QCOMPARE(failover.current(), QString("https://b"));
    QVERIFY(failover.reportResult("https://b", false));
    QCOMPARE(failover.current(), QString("https://a"));
}

void TestCDSMirrors::testFailoverSingleUrlNeverSwitches()
{
    CDSMirrors::MirrorFailover failover(1);
    failover.setUrls({"https://a"});
    for (int i = 0; i < 5; i++)
        QVERIFY(!failover.reportResult("https://a", false));
    QCOMPARE(failover.current(), QString("https://a"));

    // Offline sources have no mirror at all
    failover.setUrls({});
    QVERIFY(failover.current().isEmpty());
    QVERIFY(!failover.reportResult(QString(), false));
}

void TestCDSMirrors::testFailoverDropsEmptyAndDuplicateUrls()
{
    CDSMirrors::MirrorFailover failover(1);
    failover.setUrls({"https://a", "", " https://b ", "https://a"});
    QCOMPARE(failover.urls(), QStringList({"https://a", "https://b"}));

    // setUrls() starts again with the first mirror
    QVERIFY(failover.reportResult("https://a", false));
    failover.setUrls({"https://a", "https://b"});
    QCOMPARE(failover.current(), QString("https://a"));
}

void TestCDSMirrors::testAbortAfterCancelsHangingReply()
{
    MiniHttpServer server(MiniHttpServer::Silent);
    QVERIFY(server.isListening());

    QNetworkAccessManager manager;
    QNetworkReply *reply = manager.get(QNetworkRequest(QUrl(server.url("/hang"))));
    CDSMirrors::abortAfter(reply, 300);

    QElapsedTimer timer;
    timer.start();
    QSignalSpy finished(reply, &QNetworkReply::finished);
    QVERIFY(finished.wait(5000));
    QCOMPARE(reply->error(), QNetworkReply::OperationCanceledError);
    QVERIFY(timer.elapsed() < 4000);
    QCOMPARE(server.requests(), 1);
    reply->deleteLater();
}

void TestCDSMirrors::testAbortAfterKeepsFinishedReply()
{
    MiniHttpServer server(MiniHttpServer::Reply, "hello");
    QNetworkAccessManager manager;
    QNetworkReply *reply = manager.get(QNetworkRequest(QUrl(server.url("/ok"))));
    CDSMirrors::abortAfter(reply, 300);

    QSignalSpy finished(reply, &QNetworkReply::finished);
    QVERIFY(finished.wait(5000));
    QCOMPARE(reply->error(), QNetworkReply::NoError);
    QCOMPARE(reply->readAll(), QByteArray("hello"));

    // The timer firing later must not touch the finished reply
    QTest::qWait(500);
    QCOMPARE(finished.count(), 1);
    QCOMPARE(reply->error(), QNetworkReply::NoError);
    reply->deleteLater();
}

QTEST_GUILESS_MAIN(TestCDSMirrors)

#include "testcdsmirrors.moc"
