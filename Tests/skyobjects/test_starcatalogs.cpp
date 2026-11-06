/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/*
 * Data checks of the star catalogs (bug 309164 and related errors, see
 * kstars/data/tools/fix-star-catalogs.py):
 *
 * - namedstars.dat and unnamedstars.dat must hold J2000.0 positions, because StarObject
 *   applies proper motion from J2000.0. Part of their records used to hold the J1991.25
 *   Hipparcos positions, which put fast-moving stars up to about 1 arcminute off.
 * - No star may be listed twice (Hipparcos and Tycho copies of one star), and no moving
 *   star may have a "ghost", a copy without proper motion left at its J1991.25 position.
 * - stars.dat, the ASCII master of the bright stars, must not have Dec seconds of 59.05"
 *   and more written as 00.0" of the next minute, nor RA seconds of 60.
 *
 * The J2000.0 reference positions come from the J2000.0 kstars/data/stars.dat
 * (commit cf2f4e250a) and agree with SIMBAD to well under the tolerance used here.
 *
 * Set KSTARS_TEST_DEEPSTARS to the path of the downloadable Tycho-2 deepstars.dat to
 * check it as well.
 */

#include "auxiliary/binfilehelper.h"
#include "skyobjects/stardata.h"
#include "skyobjects/starobject.h"

#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtTest/QTest>
#else
#include <QTest>
#endif

#include <QDir>
#include <QFile>
#include <QHash>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>

#include <cmath>
#include <memory>

namespace
{
constexpr double kArcsec = 1.0 / 3600.0;
constexpr double kEpochShiftYears = 2000.0 - 1991.25;
constexpr int kCellsRa = 360 * 60;

double hours(int h, int m, double s)
{
    return h + m / 60.0 + s / 3600.0;
}

double degrees(int d, int m, double s)
{
    const double value = std::abs(d) + m / 60.0 + s / 3600.0;
    return d < 0 ? -value : value;
}

double raHours(const StarData &star)
{
    return star.RA / 1000000.0;
}

double decDegrees(const StarData &star)
{
    return star.Dec / 100000.0;
}

double mag(const StarData &star)
{
    return star.mag / 100.0;
}

// Proper motion in mas/yr
double properMotion(const StarData &star)
{
    return std::hypot(star.dRA / 10.0, star.dDec / 10.0);
}

// Angular separation in arcseconds (haversine)
double separation(double ra1, double dec1, double ra2, double dec2)
{
    const double a1 = ra1 * 15 * M_PI / 180, d1 = dec1 * M_PI / 180, a2 = ra2 * 15 * M_PI / 180, d2 = dec2 * M_PI / 180;
    const double h = std::pow(std::sin((d2 - d1) / 2), 2) + std::cos(d1) * std::cos(d2) * std::pow(std::sin((a2 - a1) / 2), 2);
    return 2 * std::asin(std::min(1.0, std::sqrt(h))) * 180 / M_PI * 3600;
}

double separation(const StarData &star, double ra, double dec)
{
    return separation(raHours(star), decDegrees(star), ra, dec);
}

QString describe(const StarData &star)
{
    return QStringLiteral("RA %1h Dec %2° pm %3/%4 mas/yr mag %5 HD %6")
           .arg(raHours(star), 0, 'f', 6).arg(decDegrees(star), 0, 'f', 5)
           .arg(star.dRA / 10.0).arg(star.dDec / 10.0).arg(mag(star)).arg(star.HD);
}

/** A star of a catalog: one distinct record, which may be copied into several trixels */
struct CatalogStar
{
    StarData data;
    int catalog;
};

/** Spatial lookup of stars in 1 arcminute cells */
class StarGrid
{
    public:
        void add(int index, double ra, double dec)
        {
            m_cells[key(cellRa(ra), cellDec(dec))].append(index);
        }

        /** Indices of the stars that may lie within 60" of the position */
        QList<int> candidates(double ra, double dec) const
        {
            QList<int> result;
            const int cx = cellRa(ra), cy = cellDec(dec);
            const int span = std::min(int(1.0 / std::max(std::cos(dec * M_PI / 180), 1e-3)) + 1, kCellsRa / 2);
            for (int dx = -span; dx <= span; ++dx)
                for (int dy = -1; dy <= 1; ++dy)
                    result.append(m_cells.value(key((cx + dx + kCellsRa) % kCellsRa, cy + dy)));
            return result;
        }

    private:
        static int cellRa(double ra)
        {
            return int(ra * 15 * 60) % kCellsRa;
        }
        static int cellDec(double dec)
        {
            return int((dec + 90) * 60);
        }
        static qint64 key(int x, int y)
        {
            return qint64(y) * kCellsRa + x;
        }

        QHash<qint64, QList<int>> m_cells;
};
}

class TestStarCatalogs :
    public QObject
{
        Q_OBJECT

    private slots:
        void initTestCase();

        void testJ2000Positions_data();
        void testJ2000Positions();
        void testHipparcosEpochReproduced();
        void testProperMotionRestored();

        void testNoDuplicateStars();
        void testNoGhostStars();

        void testStarsDatPositionFormat();

        void testDeepStarsDuplicates();
        void testDeepStarsGhosts();

    private:
        bool loadCatalog(const QString &sourcePath, QList<StarData> &stars, QString & error);
        bool loadDeepStars(QList<CatalogStar> &stars, StarGrid & grid, QString & error);
        const StarData * findStar(const QList<StarData> &stars, double ra, double dec, double magnitude) const;
        void collectStars(const QStringList & catalogs, QList<CatalogStar> &stars, StarGrid & grid) const;
        QStringList findDuplicates(const QList<CatalogStar> &stars, const StarGrid & grid, int catalog) const;
        QStringList findGhosts(const QList<CatalogStar> &stars, const StarGrid & grid, int catalog) const;

        std::unique_ptr<QTemporaryDir> m_home;
        QString m_dataDir;
        QHash<QString, QList<StarData>> m_catalogs;
};

void TestStarCatalogs::initTestCase()
{
    // BinFileHelper looks the catalogs up in the application data folder: use a private copy
    m_home.reset(new QTemporaryDir());
    QVERIFY(m_home->isValid());
    qputenv("HOME", QFile::encodeName(m_home->path()));
    QStandardPaths::setTestModeEnabled(true);

    m_dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QVERIFY(m_dataDir.startsWith(m_home->path()));
    QVERIFY(QDir().mkpath(m_dataDir));

    for (const QString &fileName :
            {
                QStringLiteral("namedstars.dat"), QStringLiteral("unnamedstars.dat")
            })
    {
        QList<StarData> stars;
        QString error;
        QVERIFY2(loadCatalog(QStringLiteral(KSTARS_TEST_SOURCEDIR "/kstars/data/") + fileName, stars, error),
                 qPrintable(error));
        QVERIFY(!stars.isEmpty());
        m_catalogs.insert(fileName, stars);
    }
}

bool TestStarCatalogs::loadCatalog(const QString &sourcePath, QList<StarData> &stars, QString &error)
{
    const QString fileName = QFileInfo(sourcePath).fileName();
    const QString copy = QDir(m_dataDir).filePath(fileName);
    QFile::remove(copy);
    if (!QFile::copy(sourcePath, copy))
    {
        error = QStringLiteral("Cannot copy %1").arg(sourcePath);
        return false;
    }

    BinFileHelper reader;
    FILE *file = reader.openFile(fileName);
    if (file == nullptr)
    {
        error = QStringLiteral("Cannot open %1").arg(fileName);
        return false;
    }
    if (!reader.readHeader())
    {
        error = QStringLiteral("Cannot read %1 header: %2").arg(fileName, reader.getError());
        reader.closeFile();
        return false;
    }
    if (reader.getByteSwap())
    {
        error = QStringLiteral("Byte-swapped catalogs are not supported by this test");
        reader.closeFile();
        return false;
    }

    // Read every trixel's records the way the star components do
    const unsigned long total = reader.getRecordCount();
    for (int trixel = 0; static_cast<unsigned long>(stars.size()) < total; ++trixel)
    {
        const unsigned int count = reader.getRecordCount(trixel);
        if (count == 0)
            continue;
        if (BinFileHelper::unsigned_KDE_fseek(file, reader.getOffset(trixel), SEEK_SET) != 0)
        {
            error = QStringLiteral("Cannot seek to trixel %1 of %2").arg(trixel).arg(fileName);
            reader.closeFile();
            return false;
        }
        for (unsigned int i = 0; i < count; ++i)
        {
            StarData data;
            if (fread(&data, sizeof(StarData), 1, file) != 1)
            {
                error = QStringLiteral("Cannot read record %1 of trixel %2 of %3").arg(i).arg(trixel).arg(fileName);
                reader.closeFile();
                return false;
            }
            stars.append(data);
        }
    }
    reader.closeFile();
    return true;
}

const StarData * TestStarCatalogs::findStar(const QList<StarData> &stars, double ra, double dec, double magnitude) const
{
    // Nearest record of a similar magnitude within 2 arcminutes: far enough to find a star still at J1991.25
    const StarData *best = nullptr;
    double bestSeparation = 120.0;
    for (const StarData &star : stars)
    {
        if (std::abs(decDegrees(star) - dec) > bestSeparation * kArcsec || std::abs(mag(star) - magnitude) > 0.3)
            continue;
        const double s = separation(star, ra, dec);
        if (s < bestSeparation)
        {
            bestSeparation = s;
            best = &star;
        }
    }
    return best;
}

void TestStarCatalogs::testJ2000Positions_data()
{
    QTest::addColumn<QString>("catalog");
    QTest::addColumn<double>("ra");
    QTest::addColumn<double>("dec");
    QTest::addColumn<double>("mag");

    // J2000.0 positions of stars that used to be off by the J1991.25 epoch, of fast stars that were
    // already right (they must not be corrected twice) and of a slow control star.
    const QString named = QStringLiteral("namedstars.dat"), unnamed = QStringLiteral("unnamedstars.dat");
    QTest::newRow("Sirius") << named << hours(6, 45, 8.92) << degrees(-16, 42, 58.0) << -1.44;
    QTest::newRow("Vega") << named << hours(18, 36, 56.34) << degrees(38, 47, 1.3) << 0.03;
    QTest::newRow("Arcturus") << named << hours(14, 15, 39.67) << degrees(19, 10, 56.7) << -0.05;
    QTest::newRow("alpha Cen A") << named << hours(14, 39, 36.50) << degrees(-60, 50, 2.3) << -0.01;
    QTest::newRow("epsilon Ind") << named << hours(22, 3, 21.66) << degrees(-56, 47, 9.5) << 4.69;
    QTest::newRow("omicron2 Eri") << named << hours(4, 15, 16.32) << degrees(-7, 39, 10.3) << 4.43;
    QTest::newRow("mu Cas") << named << hours(1, 8, 16.39) << degrees(54, 55, 13.2) << 5.17;
    QTest::newRow("beta Hyi") << named << hours(0, 25, 45.07) << degrees(-77, 15, 15.3) << 2.82;
    QTest::newRow("61 Cyg A") << unnamed << hours(21, 6, 53.95) << degrees(38, 44, 57.9) << 5.20;
    QTest::newRow("Groombridge 1830") << unnamed << hours(11, 52, 58.77) << degrees(37, 43, 7.2) << 6.42;
    QTest::newRow("Lacaille 9352") << unnamed << hours(23, 5, 52.04) << degrees(-35, 51, 11.1) << 7.35;
    QTest::newRow("61 Cyg B (was J2000)") << unnamed << hours(21, 6, 55.26) << degrees(38, 44, 31.4) << 6.05;
    QTest::newRow("Lalande 21185 (was J2000)") << unnamed << hours(11, 3, 20.19) << degrees(35, 58, 11.6) << 7.49;
    QTest::newRow("Rigel (slow)") << named << hours(5, 14, 32.27) << degrees(-8, 12, 5.9) << 0.18;
    // Algieba (gamma Leo AB) held a 1990 position; J2000.0 position from stars.dat
    QTest::newRow("Algieba") << named << hours(10, 19, 58.35) << degrees(19, 50, 29.4) << 2.23;
    // Records at the epoch of their Tycho-1 observation (about 1990.3); J2000.0 positions from XHIP
    QTest::newRow("HD 39881") << unnamed << hours(5, 56, 3.43) << degrees(13, 55, 29.7) << 6.60;
    QTest::newRow("HD 161612") << unnamed << hours(17, 47, 57.55) << degrees(-34, 1, 7.9) << 7.16;
    QTest::newRow("HD 148704") << unnamed << hours(16, 31, 30.03) << degrees(-39, 0, 44.1) << 7.30;
    // HD 101207 (HIP 56827 AB) had lost its proper motion; J2000.0 position from Tycho-2
    QTest::newRow("HD 101207") << unnamed << hours(11, 38, 57.21) << degrees(41, 8, 34.8) << 8.00;
}

void TestStarCatalogs::testJ2000Positions()
{
    QFETCH(QString, catalog);
    QFETCH(double, ra);
    QFETCH(double, dec);
    QFETCH(double, mag);

    const StarData *star = findStar(m_catalogs.value(catalog), ra, dec, mag);
    QVERIFY2(star != nullptr, "star not found within 2 arcminutes");

    // Positions are stored with 0.036" resolution; the reference has 0.15" (RA) and 0.1" (Dec)
    const double s = separation(*star, ra, dec);
    QVERIFY2(s < 1.0, qPrintable(QStringLiteral("catalog position is %1\" from J2000.0").arg(s, 0, 'f', 1)));

    // The star must be listed once: a stale copy would be drawn a few arcseconds away. Fast stars are
    // copied into every trixel they cross, so identical records count once.
    QSet<QByteArray> copies;
    for (const StarData &other : m_catalogs.value(catalog))
        if (std::abs(other.mag / 100.0 - mag) <= 0.3 && separation(other, ra, dec) < 120.0)
            copies.insert(QByteArray(reinterpret_cast<const char *>(&other), sizeof(StarData)));
    QVERIFY2(copies.size() == 1, qPrintable(QStringLiteral("%1 records within 2 arcminutes").arg(copies.size())));
}

void TestStarCatalogs::testHipparcosEpochReproduced()
{
    // Proper motion applied back to J1991.25 must give the Hipparcos catalogue position again
    const StarData *data = findStar(m_catalogs.value(QStringLiteral("namedstars.dat")), hours(14, 15, 39.67),
                                    degrees(19, 10, 56.7), -0.05);
    QVERIFY(data != nullptr);

    StarObject arcturus;
    arcturus.init(data);
    double ra = 0, dec = 0;
    QVERIFY(arcturus.getIndexCoords(-kEpochShiftYears / 1000.0, &ra, &dec));

    const double s = separation(ra / 15.0, dec, hours(14, 15, 40.35), degrees(19, 11, 14.2));
    QVERIFY2(s < 0.3, qPrintable(QStringLiteral("J1991.25 position is %1\" from Hipparcos").arg(s, 0, 'f', 2)));
}

void TestStarCatalogs::testProperMotionRestored()
{
    // HD 101207 (HIP 56827 AB) was a Hipparcos entry without its proper motion, frozen at J1991.25
    const StarData *star = findStar(m_catalogs.value(QStringLiteral("unnamedstars.dat")), hours(11, 38, 57.21),
                                    degrees(41, 8, 34.8), 8.00);
    QVERIFY(star != nullptr);
    QCOMPARE(star->HD, 101207);
    // Hipparcos-2 (XHIP) and Tycho-2 both give -80.5 / -38.3 mas/yr
    QVERIFY2(std::hypot(star->dRA / 10.0 + 80.5, star->dDec / 10.0 + 38.3) < 2.0,
             qPrintable(QStringLiteral("proper motion %1 / %2 mas/yr").arg(star->dRA / 10.0).arg(star->dDec / 10.0)));
}

void TestStarCatalogs::collectStars(const QStringList &catalogs, QList<CatalogStar> &stars, StarGrid &grid) const
{
    // Fast stars are copied into every trixel they cross: count each distinct record once
    QSet<QByteArray> seen;
    for (int c = 0; c < catalogs.size(); ++c)
    {
        for (const StarData &data : m_catalogs.value(catalogs.at(c)))
        {
            const QByteArray raw(reinterpret_cast<const char *>(&data), sizeof(StarData));
            if (seen.contains(raw))
                continue;
            seen.insert(raw);
            grid.add(stars.size(), raHours(data), decDegrees(data));
            stars.append({data, c});
        }
    }
}

QStringList TestStarCatalogs::findDuplicates(const QList<CatalogStar> &stars, const StarGrid &grid, int catalog) const
{
    // Two records of the same star: same position, proper motion and magnitude. Resolved double stars
    // (flagged multiple) can have components 0.5" to 1" apart with the same motion, so they are kept.
    // Records with bit-identical astrometry are the components of a close double sharing one
    // astrometric solution (two catalogs never agree to the last milliarcsecond), so they are kept too.
    QStringList duplicates;
    for (int i = 0; i < stars.size(); ++i)
    {
        const StarData &a = stars.at(i).data;
        for (int j : grid.candidates(raHours(a), decDegrees(a)))
        {
            const StarData &b = stars.at(j).data;
            if (j <= i || (stars.at(i).catalog < catalog && stars.at(j).catalog < catalog))
                continue;
            if (a.RA == b.RA && a.Dec == b.Dec && a.dRA == b.dRA && a.dDec == b.dDec && a.parallax == b.parallax)
                continue;
            const bool multiple = (a.flags & 0x02) || (b.flags & 0x02);
            const double pmTolerance = std::max(10.0, 0.1 * std::max(properMotion(a), properMotion(b)));
            if (separation(a, raHours(b), decDegrees(b)) <= (multiple ? 0.5 : 1.0)
                    && std::hypot((a.dRA - b.dRA) / 10.0, (a.dDec - b.dDec) / 10.0) <= pmTolerance
                    && std::abs(mag(a) - mag(b)) <= 0.5)
                duplicates << describe(a) + QStringLiteral(" == ") + describe(b);
        }
    }
    return duplicates;
}

QStringList TestStarCatalogs::findGhosts(const QList<CatalogStar> &stars, const StarGrid &grid, int catalog) const
{
    // A copy of a moving star without proper motion, left at its J1991.25 position
    QStringList ghosts;
    for (const CatalogStar &moving : stars)
    {
        if (properMotion(moving.data) * kEpochShiftYears / 1000.0 < 0.3)
            continue;
        StarObject star;
        star.init(&moving.data);
        double ra = 0, dec = 0;
        star.getIndexCoords(-kEpochShiftYears / 1000.0, &ra, &dec);
        ra /= 15.0;
        for (int j : grid.candidates(ra, dec))
        {
            const CatalogStar &ghost = stars.at(j);
            if (ghost.catalog < catalog || ghost.data.dRA != 0 || ghost.data.dDec != 0)
                continue;
            if (separation(ghost.data, ra, dec) <= 0.3 && std::abs(mag(ghost.data) - mag(moving.data)) <= 0.75)
                ghosts << describe(ghost.data) + QStringLiteral(" is a ghost of ") + describe(moving.data);
        }
    }
    return ghosts;
}

void TestStarCatalogs::testNoDuplicateStars()
{
    QList<CatalogStar> stars;
    StarGrid grid;
    collectStars({QStringLiteral("namedstars.dat"), QStringLiteral("unnamedstars.dat")}, stars, grid);

    const QStringList duplicates = findDuplicates(stars, grid, 0);
    for (const QString &duplicate : duplicates.mid(0, 10))
        qWarning() << duplicate;
    QVERIFY2(duplicates.isEmpty(), qPrintable(QStringLiteral("%1 stars are listed twice").arg(duplicates.size())));
}

void TestStarCatalogs::testNoGhostStars()
{
    QList<CatalogStar> stars;
    StarGrid grid;
    collectStars({QStringLiteral("namedstars.dat"), QStringLiteral("unnamedstars.dat")}, stars, grid);

    const QStringList ghosts = findGhosts(stars, grid, 0);
    for (const QString &ghost : ghosts.mid(0, 10))
        qWarning() << ghost;
    QVERIFY2(ghosts.isEmpty(), qPrintable(QStringLiteral("%1 ghost stars").arg(ghosts.size())));
}

void TestStarCatalogs::testStarsDatPositionFormat()
{
    QFile file(QStringLiteral(KSTARS_TEST_SOURCEDIR "/kstars/data/stars.dat"));
    QVERIFY(file.open(QIODevice::ReadOnly));

    int rows = 0, decSeconds00 = 0, decSeconds01 = 0;
    QString betaCas;
    while (!file.atEnd())
    {
        const QString line = QString::fromLatin1(file.readLine());
        if (line.startsWith(QLatin1Char('#')) || line.trimmed().isEmpty())
            continue;
        ++rows;
        QVERIFY2(line.mid(4, 5).toDouble() < 60.0, qPrintable(QStringLiteral("RA seconds of 60: ") + line));
        QVERIFY2(line.mid(15, 4).toDouble() < 60.0, qPrintable(QStringLiteral("Dec seconds of 60: ") + line));
        const QString decSeconds = line.mid(15, 4);
        decSeconds00 += decSeconds == QLatin1String("00.0");
        decSeconds01 += decSeconds == QLatin1String("00.1");
        if (line.contains(QLatin1String(", bet Cas,")))
            betaCas = line;
    }
    QVERIFY(rows > 100000);

    // The 2012 conversion wrote 59.05" and more as 00.0" of the next minute, about ten times too many
    QVERIFY2(decSeconds00 < 2 * decSeconds01,
             qPrintable(QStringLiteral("%1 rows with Dec seconds 00.0, %2 with 00.1").arg(decSeconds00).arg(decSeconds01)));
    // beta Cas, J2000.0: 00h09m10.69s +59°08'59.2"
    QCOMPARE(betaCas.left(19), QStringLiteral("000910.69 +590859.2"));
}

bool TestStarCatalogs::loadDeepStars(QList<CatalogStar> &stars, StarGrid &grid, QString &error)
{
    if (!m_catalogs.contains(QStringLiteral("deepstars.dat")))
    {
        QList<StarData> deepStars;
        if (!loadCatalog(qEnvironmentVariable("KSTARS_TEST_DEEPSTARS"), deepStars, error))
            return false;
        m_catalogs.insert(QStringLiteral("deepstars.dat"), deepStars);
    }
    collectStars({QStringLiteral("namedstars.dat"), QStringLiteral("unnamedstars.dat"), QStringLiteral("deepstars.dat")},
                 stars, grid);
    return true;
}

void TestStarCatalogs::testDeepStarsDuplicates()
{
    if (qEnvironmentVariable("KSTARS_TEST_DEEPSTARS").isEmpty())
        QSKIP("Set KSTARS_TEST_DEEPSTARS to the path of the Tycho-2 deepstars.dat to check it");

    QList<CatalogStar> stars;
    StarGrid grid;
    QString error;
    QVERIFY2(loadDeepStars(stars, grid, error), qPrintable(error));

    // Only duplicates involving deepstars.dat count: it is the file being checked
    const QStringList duplicates = findDuplicates(stars, grid, 2);
    for (const QString &duplicate : duplicates.mid(0, 10))
        qWarning() << duplicate;
    QVERIFY2(duplicates.isEmpty(), qPrintable(QStringLiteral("%1 stars are listed twice").arg(duplicates.size())));
}

void TestStarCatalogs::testDeepStarsGhosts()
{
    if (qEnvironmentVariable("KSTARS_TEST_DEEPSTARS").isEmpty())
        QSKIP("Set KSTARS_TEST_DEEPSTARS to the path of the Tycho-2 deepstars.dat to check it");

    QList<CatalogStar> stars;
    StarGrid grid;
    QString error;
    QVERIFY2(loadDeepStars(stars, grid, error), qPrintable(error));

    // Barnard's Star, J2000.0 17h57m48.50s +04°41'36.2", must keep its moving record. Its ghost, the
    // Hipparcos entry without proper motion (V 9.54 at the J1991.25 position), used to sit 90" south.
    const QList<StarData> &deepStars = m_catalogs[QStringLiteral("deepstars.dat")];
    const StarData *barnard = findStar(deepStars, hours(17, 57, 48.50), degrees(4, 41, 36.2), 9.6);
    QVERIFY(barnard != nullptr);
    QVERIFY(properMotion(*barnard) > 10000);
    QVERIFY(separation(*barnard, hours(17, 57, 48.50), degrees(4, 41, 36.2)) < 1.0);

    // Only ghosts in deepstars.dat count: it is the file being checked
    const QStringList ghosts = findGhosts(stars, grid, 2);
    for (const QString &ghost : ghosts.mid(0, 10))
        qWarning() << ghost;
    QVERIFY2(ghosts.isEmpty(), qPrintable(QStringLiteral("%1 ghost stars").arg(ghosts.size())));
}

QTEST_GUILESS_MAIN(TestStarCatalogs)

#include "test_starcatalogs.moc"
