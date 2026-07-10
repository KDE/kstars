/*
    SPDX-FileCopyrightText: 2026 Christian Kemper <ckemper@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QtGlobal>
#include <QTest>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QDir>

#include "fitsviewer/fitsdata.h"
#include "ekos/auxiliary/solverutils.h"
#include "ekos/auxiliary/stellarsolverprofile.h"
#include "Options.h"

class TestSolverBenchmark : public QObject
{
        Q_OBJECT

    private:
        static bool ensureIndexFiles();
        bool solveField(QSharedPointer<FITSData> image, double ra, double dec, double pixscale,
                        const SSolver::Parameters &parameters,
                        int algo, bool hintScale, bool hintPosition,
                        int timeoutSecs, const QString &label);

    private Q_SLOTS:
        void initTestCase();
        void benchmarkRenderedField_data();
        void benchmarkRenderedField();
};

bool TestSolverBenchmark::ensureIndexFiles()
{
    if (Options::astrometryIndexFolderList().isEmpty())
    {
        const QStringList candidates =
        {
            QDir::homePath() + "/Library/Application Support/kstars/astrometry",
            QDir::homePath() + "/.local/share/kstars/astrometry",
        };
        for (const auto &d : candidates)
            if (QDir(d).exists())
            {
                Options::setAstrometryIndexFolderList(QStringList() << d);
                break;
            }
    }
    return !Options::astrometryIndexFolderList().isEmpty();
}

void TestSolverBenchmark::initTestCase()
{
    Options::setAutoDebayer(false);
}

bool TestSolverBenchmark::solveField(QSharedPointer<FITSData> image,
                                     double ra, double dec, double pixscale,
                                     const SSolver::Parameters &parameters,
                                     int algo, bool hintScale, bool hintPosition,
                                     int timeoutSecs, const QString &label)
{
    SolverUtils::setMultiAlgorithmOverride(algo);
    Options::setSolverType(0);

    QSharedPointer<SolverUtils> solver(new SolverUtils(parameters, timeoutSecs),
                                       &QObject::deleteLater);

    if (hintScale)
        solver->useScale(true, pixscale * 0.9, pixscale * 1.1, ARCSEC_PER_PIX);
    else
        solver->useScale(false, 0, 0, ARCSEC_PER_PIX);

    if (hintPosition)
        solver->usePosition(true, ra, dec);
    else
        solver->usePosition(false, 0, 0);

    bool finished = false;
    bool success = false;
    FITSImage::Solution solution;
    double elapsed = 0;

    connect(solver.get(), &SolverUtils::done, this,
            [&](bool timedOut, bool ok, const FITSImage::Solution & sol, double secs)
    {
        finished = true;
        success = !timedOut && ok;
        solution = sol;
        elapsed = secs;
    });

    solver->runSolver(image);

    QElapsedTimer waitTimer;
    waitTimer.start();
    long waitMs = static_cast<long>(timeoutSecs) * 1000 + 5000;
    while (!finished && waitTimer.elapsed() < waitMs)
        QTest::qWait(100);

    SolverUtils::clearMultiAlgorithmOverride();

    if (!finished || !success)
    {
        qInfo() << QString("  %1  TIMEOUT (%2s)")
                .arg(label, -14)
                .arg(timeoutSecs);
        return false;
    }

    double raDiff = std::abs(solution.ra - ra);
    if (raDiff > 180.0) raDiff = 360.0 - raDiff;
    double decDiff = std::abs(solution.dec - dec);

    qInfo() << QString("  %1  %2s  scale %3  error %4 deg")
            .arg(label, -14)
            .arg(elapsed, 5, 'f', 2)
            .arg(solution.pixscale, 0, 'f', 3)
            .arg(std::sqrt(raDiff * raDiff + decDiff * decDiff), 0, 'f', 4);

    return (raDiff < 0.5 && decDiff < 0.5);
}

void TestSolverBenchmark::benchmarkRenderedField_data()
{
    QTest::addColumn<QString>("filename");
    QTest::addColumn<double>("ra");
    QTest::addColumn<double>("dec");
    QTest::addColumn<double>("pixscale");
    QTest::addColumn<QString>("label");

    QTest::newRow("ngc4535")       << "ngc4535-autofocus1.fits" << 188.1260 << 7.8433   << 2.6489 << "NGC 4535 (sparse)";
    QTest::newRow("ngc4535-dense") << "ngc4535-autofocus3.fits" << 188.1010 << 8.1161   << 2.6665 << "NGC 4535 (dense)";
    QTest::newRow("m47")           << "m47_sim_stars.fits"       << 114.1437 << -14.4840 << 1.1924 << "M47 Sim Stars";
}

void TestSolverBenchmark::benchmarkRenderedField()
{
    QFETCH(QString, filename);
    QFETCH(double, ra);
    QFETCH(double, dec);
    QFETCH(double, pixscale);
    QFETCH(QString, label);

    if (!ensureIndexFiles())
        QSKIP("No astrometry index files found");

    QString filePath = QFINDTESTDATA(filename);
    if (filePath.isEmpty())
        QSKIP(qPrintable(QString("Could not find %1").arg(filename)));

    auto image = QSharedPointer<FITSData>(new FITSData());
    QVERIFY(image->loadFromFile(filePath).result());

    // Detectable-star baseline with default SEP settings, full frame. The
    // solver applies the profile's own SEP tweaks (fwhm, maxEllipse,
    // initialKeep, keepNum etc.) and ends up feeding astrometry.net a
    // heavily filtered subset -- typically just the keepNum brightest --
    // so this is an upper bound on what the solver sees, not the count
    // itself. The explicit full-frame QRect bypasses FITSData's NORMAL-
    // mode quickHFR center-25% shortcut.
    const auto stats = image->getStatistics();
    auto findFuture = image->findStars(ALGORITHM_SEP,
                                       QRect(0, 0, stats.width, stats.height));
    findFuture.waitForFinished();
    const int sepStars = image->getDetectedStars();

    qInfo() << QString("%1 (%2): RA %3, Dec %4, scale %5 arcsec/px, %6 SEP stars (default settings)")
            .arg(label)
            .arg(filename)
            .arg(ra, 0, 'f', 4)
            .arg(dec, 0, 'f', 4)
            .arg(pixscale, 0, 'f', 4)
            .arg(sepStars);

    auto profiles = Ekos::getDefaultAlignOptionsProfiles();
    auto base = profiles.at(3);

    struct
    {
        int keepNum;
        double minwidth;
        double maxwidth;
        QString name;
    } configs[] =
    {
        { 50, 0.1, 10.0, "keepNum=50 0.1-10deg" },
        {  0, 0.1, 10.0, "keepNum=0  0.1-10deg" },
        { 50, 0.0,  0.0, "keepNum=50 no-scale"  },
    };

    struct
    {
        int algo;
        QString name;
    } algorithms[] =
    {
        { SSolver::MULTI_SCALES,    "MULTI_SCALES" },
        { SSolver::MULTI_DEPTHS,    "MULTI_DEPTHS" },
    };

    struct
    {
        bool scale;
        bool position;
        int timeout;
        QString name;
    } hints[] =
    {
        { true,  true,  20, "scale+pos" },
        { true,  false, 30, "scale"     },
        { false, true,  30, "pos"       },
    };

    for (const auto &cfg : configs)
    {
        auto params = base;
        params.keepNum = cfg.keepNum;
        params.minwidth = cfg.minwidth;
        params.maxwidth = cfg.maxwidth;

        qInfo() << QString("  == %1 ==").arg(cfg.name);

        for (const auto &h : hints)
        {
            bool baseline = (cfg.keepNum > 0 && cfg.maxwidth > 0);
            // Short timeout only when the scale window is off (negative
            // control we expect to always exceed it). Other configs --
            // including keepNum=0 with the scale window set -- get the
            // full timeout so the regression on dense fields is measurable
            // rather than masked.
            int timeout = (cfg.maxwidth > 0) ? h.timeout : 5;
            qInfo() << QString("    -- %1 (timeout %2s) --").arg(h.name).arg(timeout);
            for (const auto &a : algorithms)
            {
                bool ok = solveField(image, ra, dec, pixscale, params, a.algo,
                                     h.scale, h.position, timeout, a.name);
                if (baseline)
                    QVERIFY2(ok, qPrintable(QString("%1 / %2 / %3 / %4 failed")
                                            .arg(label, cfg.name, a.name, h.name)));
            }
        }
    }
}

QTEST_GUILESS_MAIN(TestSolverBenchmark)

#include "testsolverbenchmark.moc"

