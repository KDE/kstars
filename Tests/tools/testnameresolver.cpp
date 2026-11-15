/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "tools/nameresolver.h"
#include "skyobjects/catalogobject.h"
#include "auxiliary/cdsmirrors.h"
#include "../shared/minihttpserver.h"

#include <QNetworkProxy>
#include <QtTest>

#include <memory>

namespace
{
// Shortened reply of https://vizier.cfa.harvard.edu/viz-bin/nph-sesame/-oxpFI/SNV?M31
const QByteArray M31Reply =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<Sesame xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
    " xsi:noNamespaceSchemaLocation=\"https://vizier.cds.unistra.fr/xml/sesame_4x.xsd\">\n"
    "<Target option=\"SNV\">\n"
    "  <name>M31</name>\n"
    "  <Resolver name=\"Sc=Simbad (CDS, via client/server)\">\n"
    "    <INFO>from cache</INFO>\n"
    "    <oid>1575544</oid>\n"
    "    <otype>G</otype>\n"
    "    <jpos>00:42:44.32 +41:16:07.5</jpos>\n"
    "    <jradeg>10.68470833</jradeg>\n"
    "    <jdedeg>41.26875</jdedeg>\n"
    "    <mag band=\"B\"><v>4.36</v><e>0.02</e><q>D</q></mag>\n"
    "    <mag band=\"V\"><v>3.44</v><e>0.03</e><q>D</q></mag>\n"
    "    <oname>M  31</oname>\n"
    "    <alias>NGC 224</alias>\n"
    "  </Resolver>\n"
    "</Target>\n"
    "</Sesame>\n";

const QByteArray NotFoundReply =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<Sesame xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\n"
    "<Target option=\"SNV\">\n"
    "  <name>NoSuchObject</name>\n"
    "  <INFO>*** Nothing found *** </INFO>\n"
    "</Target>\n"
    "</Sesame>\n";

const QString SesamePath = QStringLiteral("/viz-bin/nph-sesame");
}

/**
 * @brief Tests the CDS Sesame name resolver and its fallback to mirrors when a server fails (bug 528778).
 * The servers are local, the tests never use the real network.
 */
class TestNameResolver : public QObject
{
        Q_OBJECT

    private Q_SLOTS:
        void initTestCase();
        void cleanup();

        void testParseResolvedReply();
        void testParseNotFoundReply();
        void testParseInvalidReply_data();
        void testParseInvalidReply();

        void testResolveFallsBackToMirror();
        void testResolveStartsWithLastWorkingMirror();
        void testResolveStopsOnWellFormedNotFound();
        void testResolveFailsWhenAllMirrorsFail();
};

void TestNameResolver::initTestCase()
{
    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
}

void TestNameResolver::cleanup()
{
    CDSMirrors::setBaseUrlsForTesting(CDSMirrors::Service::Sesame, {});
    CDSMirrors::setRequestTimeoutForTesting(0);
}

void TestNameResolver::testParseResolvedReply()
{
    bool wellFormed = false;
    const auto result = NameResolver::NameResolverInternals::parseSesameReply(M31Reply, "M31", &wellFormed);
    QVERIFY(wellFormed);
    QVERIFY(result.first);
    QCOMPARE(result.second.ra0().Degrees(), 10.68470833);
    QCOMPARE(result.second.dec0().Degrees(), 41.26875);
    QCOMPARE(result.second.mag(), 3.44f);
    QCOMPARE(result.second.catalogIdentifier(), QString("M  31"));
    QCOMPARE(result.second.type(), static_cast<int>(SkyObject::GALAXY));
}

void TestNameResolver::testParseNotFoundReply()
{
    bool wellFormed = false;
    const auto result = NameResolver::NameResolverInternals::parseSesameReply(NotFoundReply, "NoSuchObject",
                        &wellFormed);
    QVERIFY(wellFormed);
    QVERIFY(!result.first);
}

void TestNameResolver::testParseInvalidReply_data()
{
    QTest::addColumn<QByteArray>("reply");
    QTest::newRow("empty") << QByteArray();
    QTest::newRow("maintenance page") << QByteArray("<html><body><h1>Service unavailable</h1></body></html>");
    QTest::newRow("plain text") << QByteArray("Service temporarily unavailable");
    QTest::newRow("truncated") << M31Reply.left(M31Reply.indexOf("<Resolver"));
}

void TestNameResolver::testParseInvalidReply()
{
    QFETCH(QByteArray, reply);
    bool wellFormed = true;
    const auto result = NameResolver::NameResolverInternals::parseSesameReply(reply, "M31", &wellFormed);
    QVERIFY(!wellFormed);
    QVERIFY(!result.first);
}

void TestNameResolver::testResolveFallsBackToMirror()
{
    // CDS hangs, the second server is broken, the third one works
    MiniHttpServer hanging(MiniHttpServer::Silent);
    MiniHttpServer broken(MiniHttpServer::NotFound);
    MiniHttpServer working(MiniHttpServer::Reply, M31Reply, "text/xml");
    QVERIFY(hanging.isListening() && broken.isListening() && working.isListening());

    CDSMirrors::setRequestTimeoutForTesting(500);
    CDSMirrors::setBaseUrlsForTesting(CDSMirrors::Service::Sesame,
    {hanging.url(SesamePath), broken.url(SesamePath), working.url(SesamePath)});

    const auto result = NameResolver::resolveName("M31");
    QVERIFY(result.first);
    QCOMPARE(result.second.ra0().Degrees(), 10.68470833);
    QCOMPARE(hanging.requests(), 1);
    QCOMPARE(broken.requests(), 1);
    QCOMPARE(working.requests(), 1);
    QCOMPARE(working.lastPath(), QByteArray("/viz-bin/nph-sesame/-oxpFI/SNV?M31"));

    // The working server is remembered
    QCOMPARE(CDSMirrors::preferredBaseUrl(CDSMirrors::Service::Sesame), working.url(SesamePath));
}

void TestNameResolver::testResolveStartsWithLastWorkingMirror()
{
    MiniHttpServer hanging(MiniHttpServer::Silent);
    MiniHttpServer working(MiniHttpServer::Reply, M31Reply, "text/xml");

    CDSMirrors::setRequestTimeoutForTesting(500);
    CDSMirrors::setBaseUrlsForTesting(CDSMirrors::Service::Sesame, {hanging.url(SesamePath), working.url(SesamePath)});

    QVERIFY(NameResolver::resolveName("M31").first);
    QCOMPARE(hanging.requests(), 1);
    QCOMPARE(working.requests(), 1);

    // The next search does not wait for the hanging server again
    QElapsedTimer timer;
    timer.start();
    QVERIFY(NameResolver::resolveName("M31").first);
    QCOMPARE(hanging.requests(), 1);
    QCOMPARE(working.requests(), 2);
    QVERIFY(timer.elapsed() < 400);
}

void TestNameResolver::testResolveStopsOnWellFormedNotFound()
{
    // All mirrors serve the same data, so an unknown name is not looked up on the other mirrors
    MiniHttpServer first(MiniHttpServer::Reply, NotFoundReply, "text/xml");
    MiniHttpServer second(MiniHttpServer::Reply, M31Reply, "text/xml");

    CDSMirrors::setBaseUrlsForTesting(CDSMirrors::Service::Sesame, {first.url(SesamePath), second.url(SesamePath)});

    QVERIFY(!NameResolver::resolveName("NoSuchObject").first);
    QCOMPARE(first.requests(), 1);
    QCOMPARE(second.requests(), 0);
}

void TestNameResolver::testResolveFailsWhenAllMirrorsFail()
{
    MiniHttpServer broken(MiniHttpServer::NotFound);
    MiniHttpServer maintenance(MiniHttpServer::Reply, "<html><body>Down for maintenance</body></html>", "text/html");

    CDSMirrors::setBaseUrlsForTesting(CDSMirrors::Service::Sesame, {broken.url(SesamePath), maintenance.url(SesamePath)});

    QVERIFY(!NameResolver::resolveName("M31").first);
    QCOMPARE(broken.requests(), 1);
    QCOMPARE(maintenance.requests(), 1);

    // No server is remembered after a failure
    QCOMPARE(CDSMirrors::preferredBaseUrl(CDSMirrors::Service::Sesame), broken.url(SesamePath));
}

QTEST_GUILESS_MAIN(TestNameResolver)

#include "testnameresolver.moc"
