/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "auxiliary/ksutils.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <zlib.h>

class TestMPCParser : public QObject
{
        Q_OBJECT

    private Q_SLOTS:
        void testParsePath_data();
        void testParsePath();
};

void TestMPCParser::testParsePath_data()
{
    QTest::addColumn<QString>("dirName");
    QTest::addColumn<bool>("compressed");

    const QString accented = QString::fromUtf8("José");
    const QString cyrillic = QString::fromUtf8("Анна");

    QTest::newRow("ascii, plain") << QString("home") << false;
    QTest::newRow("ascii, gzip") << QString("home") << true;
    // The asteroid and comet data live in the user's data folder, under the home directory.
    QTest::newRow("accented, plain") << accented << false;
    QTest::newRow("accented, gzip") << accented << true;
    QTest::newRow("non-latin1, gzip") << cyrillic << true;
}

void TestMPCParser::testParsePath()
{
    QFETCH(QString, dirName);
    QFETCH(bool, compressed);

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    const QString dir = QDir(tmpDir.path()).filePath(dirName);
    QVERIFY(QDir().mkpath(dir));
    const QString path = QDir(dir).filePath("mpc.json.gz");

    const QByteArray json = R"([{"name":"Ceres","H":3.34},{"name":"Vesta","H":3.2}])";
    if (compressed)
    {
        gzFile out = gzopen(QFile::encodeName(path).constData(), "wb");
        QVERIFY(out != nullptr);
        QCOMPARE(gzwrite(out, json.constData(), json.size()), static_cast<int>(json.size()));
        QCOMPARE(gzclose(out), Z_OK);
    }
    else
    {
        QFile out(path);
        QVERIFY(out.open(QIODevice::WriteOnly));
        QCOMPARE(out.write(json), json.size());
    }

    QStringList names;
    KSUtils::MPCParser parser(path);
    parser.for_each([&](const auto & get)
    {
        names << get("name").toString();
    });

    QCOMPARE(names, QStringList() << "Ceres" << "Vesta");
}

QTEST_GUILESS_MAIN(TestMPCParser)

#include "testmpcparser.moc"
