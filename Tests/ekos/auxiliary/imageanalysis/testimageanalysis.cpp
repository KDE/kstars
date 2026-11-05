/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/*
 * Tests for the analysis of the star shapes in a frame, shared by the Ekos modules to tell whether the frame
 * itself is the problem. Frames with a known fault are rendered, written to FITS, and measured:
 * - good: enough round, focused stars, so the image is not the problem; also large or faint focused stars,
 * - too few stars, out of focus (discs and donuts), trailed (straight, long and curved), doubled by a mechanical jump,
 * - elongated by the optics (radial, like coma): not trailed.
 * Real sky frames from the test fixtures get the verdict they should.
 *
 * Set IMAGE_ANALYSIS_FILES to a list of image files separated by ':' to print the verdict of each,
 * e.g. frames from users.
 */

#include "ekos/auxiliary/imageanalysis.h"
#include "fitsviewer/fitsdata.h"

#include <fitsio.h>

#include <QElapsedTimer>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QtMath>

#include <functional>
#include <random>

using namespace Ekos;

namespace
{
constexpr int WIDTH = 1024;
constexpr int HEIGHT = 768;
constexpr double BACKGROUND = 1000;
constexpr double NOISE = 15;
constexpr double PSF_SIGMA = 1.4;

/** Where the light of one star goes, as points with weights, relative to the star position. */
using Path = std::function<QList<QPair<QPointF, double>>(const QPointF &star)>;

Path point()
{
    return [](const QPointF &)
    {
        return QList<QPair<QPointF, double>>({{QPointF(0, 0), 1.0}});
    };
}

/** A straight trail of the given length and direction, in degrees. */
Path line(double length, double angle)
{
    return [length, angle](const QPointF &)
    {
        QList<QPair<QPointF, double>> points;
        const int samples = std::max(2, static_cast<int>(length * 2));
        const double c = std::cos(qDegreesToRadians(angle)), s = std::sin(qDegreesToRadians(angle));
        for (int i = 0; i < samples; i++)
        {
            const double t = (static_cast<double>(i) / (samples - 1) - 0.5) * length;
            points.append({QPointF(t * c, t * s), 1.0 / samples});
        }
        return points;
    };
}

/** A curved trail: an arc of the given radius and length, like a mount that drifts and turns. */
Path arc(double radius, double length, double angle)
{
    return [radius, length, angle](const QPointF &)
    {
        QList<QPair<QPointF, double>> points;
        const int samples = std::max(2, static_cast<int>(length * 2));
        const double span = length / radius;
        const double start = qDegreesToRadians(angle) - M_PI / 2 - span / 2;
        for (int i = 0; i < samples; i++)
        {
            const double phi = start + span * i / (samples - 1);
            points.append({QPointF(radius * std::cos(phi), radius * std::sin(phi) + radius), 1.0 / samples});
        }
        return points;
    };
}

/** Two images of each star: the mount jumped once during the exposure. */
Path jump(double offset, double angle, double firstShare)
{
    return [offset, angle, firstShare](const QPointF &)
    {
        const QPointF delta(offset * std::cos(qDegreesToRadians(angle)), offset * std::sin(qDegreesToRadians(angle)));
        return QList<QPair<QPointF, double>>({{QPointF(0, 0), firstShare}, {delta, 1.0 - firstShare}});
    };
}

/** A defocused star: light spread evenly over a disc, or over a ring when the telescope has a central obstruction. */
Path disc(double radius, double innerRadius = 0)
{
    return [radius, innerRadius](const QPointF &)
    {
        QList<QPair<QPointF, double>> points;
        for (double y = -radius; y <= radius; y += 0.5)
            for (double x = -radius; x <= radius; x += 0.5)
            {
                const double r = std::hypot(x, y);
                if (r <= radius && r >= innerRadius)
                    points.append({QPointF(x, y), 1.0});
            }
        for (auto &point : points)
            point.second = 1.0 / points.size();
        return points;
    };
}

/** Stars stretched towards the frame centre, like coma or tilt: elongated, but not parallel. */
Path radial(double length)
{
    return [length](const QPointF & star)
    {
        const double angle = qRadiansToDegrees(std::atan2(star.y() - HEIGHT / 2.0, star.x() - WIDTH / 2.0));
        return line(length, angle)(star);
    };
}
}

class TestImageAnalysis :
    public QObject
{
        Q_OBJECT

    private slots:
        void initTestCase();

        void testSiderealDrift();
        void testMeasurementRegion_data();
        void testMeasurementRegion();

        void testRenderedFrames_data();
        void testRenderedFrames();

        void testHotPixels();
        void testFewDiscsAmongHotPixels();
        void testColourFrame();
        void testCroppedStarCount();
        void testTinyFrame();
        void testSiderealTrail();
        void testRealFrames_data();
        void testRealFrames();
        void testExtraFiles();

    private:
        QString render(const QString &name, int stars, double sigma, const Path &path, double flux = 1.0,
                       int hotPixels = 0, int channels = 1, bool debayered = false);
        bool measure(const QString &file, ImageStats &stats, const QRect &region = QRect());

        QTemporaryDir m_Dir;
};

void TestImageAnalysis::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QVERIFY(m_Dir.isValid());
}

QString TestImageAnalysis::render(const QString &name, int stars, double sigma, const Path &path, double flux,
                                  int hotPixels, int channels, bool debayered)
{
    std::mt19937 random(42);
    std::uniform_real_distribution<double> x(40, WIDTH - 40), y(40, HEIGHT - 40), peak(0.05, 1.0);
    std::normal_distribution<double> noise(0, NOISE);

    std::vector<double> image(WIDTH * HEIGHT, BACKGROUND);
    for (int i = 0; i < stars; i++)
    {
        const QPointF star(x(random), y(random));
        // Total flux of a star, spread along its path
        const double total = flux * 2 * M_PI * PSF_SIGMA * PSF_SIGMA * 20000 * peak(random) * peak(random);
        for (const auto &sample : path(star))
        {
            const double cx = star.x() + sample.first.x(), cy = star.y() + sample.first.y();
            const double amplitude = total * sample.second / (2 * M_PI * sigma * sigma);
            const int radius = static_cast<int>(std::ceil(4 * sigma));
            for (int py = static_cast<int>(cy) - radius; py <= static_cast<int>(cy) + radius; py++)
                for (int px = static_cast<int>(cx) - radius; px <= static_cast<int>(cx) + radius; px++)
                {
                    if (px < 0 || py < 0 || px >= WIDTH || py >= HEIGHT)
                        continue;
                    const double r2 = (px - cx) * (px - cx) + (py - cy) * (py - cy);
                    image[py * WIDTH + px] += amplitude * std::exp(-r2 / (2 * sigma * sigma));
                }
        }
    }

    std::vector<unsigned short> pixels(image.size() * channels);
    for (size_t i = 0; i < pixels.size(); i++)
        pixels[i] = static_cast<unsigned short>(std::clamp(image[i % image.size()] + noise(random), 0.0, 65535.0));
    // Hot pixels: single bright pixels of an uncalibrated frame, not stars. Debayering spreads each over 3 x 3 pixels,
    // with a peak of about 400 above the background, as in a real frame of an uncooled colour camera.
    std::uniform_int_distribution<int> anywhere(WIDTH + 1, WIDTH * (HEIGHT - 1) - 2);
    const double spread[3][3] = {{1, 2, 1}, {2, 4, 2}, {1, 2, 1}};
    for (int i = 0; i < hotPixels; i++)
    {
        const int pixel = anywhere(random);
        for (int c = 0; c < channels; c++)
        {
            if (!debayered)
                pixels[c * image.size() + pixel] = 60000;
            else
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++)
                    {
                        auto &value = pixels[c * image.size() + pixel + dy * WIDTH + dx];
                        value = static_cast<unsigned short>(std::min(65535.0, value + 400 * spread[dy + 1][dx + 1] / 4));
                    }
        }
    }

    const QString file = m_Dir.filePath(name + ".fits");
    fitsfile *fptr = nullptr;
    int status = 0;
    long naxes[3] = {WIDTH, HEIGHT, channels};
    fits_create_file(&fptr, ("!" + file).toLocal8Bit().constData(), &status);
    fits_create_img(fptr, USHORT_IMG, channels > 1 ? 3 : 2, naxes, &status);
    fits_write_img(fptr, TUSHORT, 1, pixels.size(), pixels.data(), &status);
    fits_close_file(fptr, &status);
    return status == 0 ? file : QString();
}

bool TestImageAnalysis::measure(const QString &file, ImageStats &stats, const QRect &region)
{
    QElapsedTimer timer;
    timer.start();
    QSharedPointer<FITSData> data(new FITSData());
    auto loading = data->loadFromFile(file);
    loading.waitForFinished();
    if (!loading.result())
        return false;
    const qint64 loaded = timer.restart();

    stats = ImageAnalysis::analyse(*data, region);
    const qint64 analysed = timer.elapsed();
    qInfo().noquote() << QString("%1: %2, stars %3 (profiled %4), HFR %5, R80/R20 %6, centre dip %7, elongation %8, "
                                 "coherence %9, angle %10, trail %11 px, double %12 px x %13")
                      .arg(QFileInfo(file).fileName(), imageVerdictKey(stats.verdict))
                      .arg(stats.stars).arg(stats.profiledStars).arg(stats.hfr, 0, 'f', 2)
                      .arg(stats.concentration, 0, 'f', 2).arg(stats.centreDip, 0, 'f', 2)
                      .arg(stats.elongation, 0, 'f', 2).arg(stats.coherence, 0, 'f', 2).arg(stats.angle, 0, 'f', 0)
                      .arg(stats.trailLength, 0, 'f', 1).arg(stats.doubleOffset, 0, 'f', 1)
                      .arg(stats.doubleFraction, 0, 'f', 2);
    const QRect measured = region.isValid() ? region : ImageAnalysis::measurementRegion(data->width(), data->height());
    qInfo().noquote() << QString("%1 x %2 px, measured %3 x %4: load %5 ms, analyse %6 ms")
                      .arg(data->width()).arg(data->height()).arg(measured.width()).arg(measured.height())
                      .arg(loaded).arg(analysed);
    return true;
}

void TestImageAnalysis::testSiderealDrift()
{
    // 15.041"/s at the equator, half of it at declination 60
    QVERIFY(std::abs(ImageAnalysis::siderealDrift(10, 0, 1.0) - 150.41) < 0.01);
    QVERIFY(std::abs(ImageAnalysis::siderealDrift(10, 60, 1.0) - 75.2) < 0.1);
    QCOMPARE(ImageAnalysis::siderealDrift(10, 0, 0), 0.0);
}

void TestImageAnalysis::testMeasurementRegion_data()
{
    QTest::addColumn<int>("width");
    QTest::addColumn<int>("height");
    QTest::addColumn<QRect>("region");

    QTest::newRow("small frame, whole") << 1024 << 768 << QRect(0, 0, 1024, 768);
    QTest::newRow("at the limit, whole") << 2048 << 2048 << QRect(0, 0, 2048, 2048);
    QTest::newRow("26 MP, centre") << 6252 << 4176 << QRect(2102, 1064, 2048, 2048);
    QTest::newRow("wide strip, centre of its width") << 4000 << 1000 << QRect(976, 0, 2048, 1000);
    QTest::newRow("no frame") << 0 << 0 << QRect();
}

void TestImageAnalysis::testMeasurementRegion()
{
    QFETCH(int, width);
    QFETCH(int, height);
    QFETCH(QRect, region);
    QCOMPARE(ImageAnalysis::measurementRegion(width, height), region);
}

void TestImageAnalysis::testRenderedFrames_data()
{
    QTest::addColumn<int>("stars");
    QTest::addColumn<double>("sigma");
    QTest::addColumn<int>("shape");
    QTest::addColumn<double>("flux");
    QTest::addColumn<int>("verdict");

    enum { Point, Line20, Line60, Arc, Jump, Radial, SmallDisc, SmallDonut, LargeDisc, LargeDonut };
    QTest::newRow("good") << 150 << PSF_SIGMA << int(Point) << 1.0 << int(ImageVerdict::Good);
    QTest::newRow("no stars") << 0 << PSF_SIGMA << int(Point) << 1.0 << int(ImageVerdict::FewStars);
    QTest::newRow("few stars") << 5 << PSF_SIGMA << int(Point) << 1.0 << int(ImageVerdict::FewStars);
    // Large stars with a bright core, e.g. a long focal length with small pixels: focused
    QTest::newRow("large focused stars") << 150 << 5.0 << int(Point) << 1.0 << int(ImageVerdict::Good);
    // Faint stars are measured with their wings: focused, not flat
    QTest::newRow("faint focused stars") << 150 << 1.6 << int(Point) << 0.03 << int(ImageVerdict::Good);
    QTest::newRow("short trails") << 150 << PSF_SIGMA << int(Line20) << 1.0 << int(ImageVerdict::Trailed);
    QTest::newRow("long trails") << 150 << PSF_SIGMA << int(Line60) << 1.0 << int(ImageVerdict::Trailed);
    QTest::newRow("curved trails") << 150 << PSF_SIGMA << int(Arc) << 1.0 << int(ImageVerdict::Trailed);
    QTest::newRow("double images") << 150 << PSF_SIGMA << int(Jump) << 1.0 << int(ImageVerdict::DoubleImages);
    QTest::newRow("radial elongation") << 150 << PSF_SIGMA << int(Radial) << 1.0 << int(ImageVerdict::Good);
    // Defocused stars are discs, or donuts behind a central obstruction, small or large
    QTest::newRow("small defocused discs") << 150 << 1.0 << int(SmallDisc) << 1.0 << int(ImageVerdict::OutOfFocus);
    QTest::newRow("small defocused donuts") << 150 << 1.0 << int(SmallDonut) << 1.0 << int(ImageVerdict::OutOfFocus);
    QTest::newRow("large defocused discs") << 150 << 1.0 << int(LargeDisc) << 1.0 << int(ImageVerdict::OutOfFocus);
    QTest::newRow("large defocused donuts") << 150 << 1.0 << int(LargeDonut) << 1.0 << int(ImageVerdict::OutOfFocus);
}

void TestImageAnalysis::testRenderedFrames()
{
    QFETCH(int, stars);
    QFETCH(double, sigma);
    QFETCH(int, shape);
    QFETCH(double, flux);
    QFETCH(int, verdict);

    const QList<Path> paths({point(), line(20, 25), line(60, 25), arc(60, 40, 25), jump(9, 70, 0.6), radial(6), disc(5),
                             disc(6, 2.5), disc(15), disc(15, 7)});
    const QString file = render(QTest::currentDataTag(), stars, sigma, paths[shape], flux);
    QVERIFY(!file.isEmpty());

    ImageStats stats;
    QVERIFY(measure(file, stats));
    QCOMPARE(imageVerdictKey(stats.verdict), imageVerdictKey(static_cast<ImageVerdict>(verdict)));

    // The measured trail matches the drawn one
    if (shape == 1 || shape == 2)
    {
        const double length = shape == 1 ? 20 : 60;
        QVERIFY2(std::abs(stats.trailLength - length) < 0.25 * length, qPrintable(QString::number(stats.trailLength)));
        // 25 degrees in image coordinates, whichever way the y axis points
        const double angle = std::fmod(stats.angle, 180);
        QVERIFY2(std::abs(angle - 25) < 5 || std::abs(angle - 155) < 5, qPrintable(QString::number(angle)));
    }
    if (shape == 4)
        QVERIFY2(std::abs(stats.doubleOffset - 9) < 1.5, qPrintable(QString::number(stats.doubleOffset)));
    // Donuts are told by their dark centre
    if (shape == 7 || shape == 9)
        QVERIFY2(stats.centreDip < ImageAnalysis::MAX_FOCUSED_CENTRE_DIP, qPrintable(QString::number(stats.centreDip)));
}

void TestImageAnalysis::testHotPixels()
{
    // An uncalibrated frame with the cap on: hot pixels are not stars
    const QString file = render("hot pixels", 0, PSF_SIGMA, point(), 1.0, 300);
    ImageStats stats;
    QVERIFY(measure(file, stats));
    QCOMPARE(imageVerdictKey(stats.verdict), QString("few_stars"));

    // ... and they do not hide the stars of a good frame
    QVERIFY(measure(render("stars and hot pixels", 150, PSF_SIGMA, point(), 1.0, 300), stats));
    QCOMPARE(imageVerdictKey(stats.verdict), QString("good"));
}

void TestImageAnalysis::testFewDiscsAmongHotPixels()
{
    // A defocused frame with only a few large, faint discs, and the hot pixels of an uncalibrated colour camera,
    // spread by debayering into small sources that look like focused stars. They outnumber the discs, but the
    // discs hold the light: the frame is out of focus.
    const QString file = render("few discs among hot pixels", 12, 1.0, disc(25), 2.0, 200, 1, true);
    ImageStats stats;
    QVERIFY(measure(file, stats));
    QCOMPARE(imageVerdictKey(stats.verdict), QString("out_of_focus"));
    QVERIFY2(stats.hfr > 10, qPrintable(QString::number(stats.hfr)));
}

void TestImageAnalysis::testColourFrame()
{
    // A debayered frame is measured on the mean of its channels
    const QString file = render("colour trails", 150, PSF_SIGMA, line(20, 25), 1.0, 0, 3);
    QSharedPointer<FITSData> data(new FITSData());
    auto loading = data->loadFromFile(file);
    loading.waitForFinished();
    QVERIFY(loading.result());
    QCOMPARE(data->channels(), 3);

    const ImageStats stats = ImageAnalysis::analyse(*data);
    QCOMPARE(imageVerdictKey(stats.verdict), QString("trailed"));
    QVERIFY2(std::abs(stats.trailLength - 20) < 5, qPrintable(QString::number(stats.trailLength)));
}

void TestImageAnalysis::testCroppedStarCount()
{
    // A quarter of the frame was measured: the star count is for the whole frame
    const QString file = render("cropped", 150, PSF_SIGMA, point());
    ImageStats whole, centre;
    QVERIFY(measure(file, whole));
    QVERIFY(measure(file, centre, QRect(WIDTH / 4, HEIGHT / 4, WIDTH / 2, HEIGHT / 2)));

    QCOMPARE(centre.measuredFraction, 0.25);
    QCOMPARE(centre.stars, static_cast<int>(std::lround(centre.measuredStars / 0.25)));
    QVERIFY2(centre.measuredStars < whole.measuredStars / 2,
             qPrintable(QString("%1 of %2").arg(centre.measuredStars).arg(whole.measuredStars)));
    // The stars are spread at random: the estimate is near the whole count
    QVERIFY2(centre.stars > whole.stars / 2 && centre.stars < whole.stars * 2,
             qPrintable(QString("%1, whole %2").arg(centre.stars).arg(whole.stars)));
    QCOMPARE(imageVerdictKey(centre.verdict), QString("good"));
}

void TestImageAnalysis::testTinyFrame()
{
    // Too small to hold a star with its background: not measured
    const QString file = render("tiny", 150, PSF_SIGMA, point());
    ImageStats stats;
    QVERIFY(measure(file, stats, QRect(0, 0, 40, 40)));
    QCOMPARE(stats.verdict, ImageVerdict::Unknown);
    QCOMPARE(stats.measuredStars, 0);
}

void TestImageAnalysis::testSiderealTrail()
{
    // 2"/px, 10 s at declination 40: a star drifts 57.6 px when the mount does not track. The measured trail
    // must match it, so that a module can tell a mount that stopped from one that tracks badly.
    const double drift = ImageAnalysis::siderealDrift(10, 40, 2.0);
    const QString file = render("untracked", 150, PSF_SIGMA, line(drift, 0));

    ImageStats stats;
    QVERIFY(measure(file, stats));
    QCOMPARE(stats.verdict, ImageVerdict::Trailed);
    QVERIFY2(std::abs(stats.trailLength - drift) < 0.25 * drift, qPrintable(QString::number(stats.trailLength)));
}

void TestImageAnalysis::testRealFrames_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<QString>("verdict");

    // Undersampled stars: too small to tell their profile, and focused
    QTest::newRow("Kocab") << "../../../kstars_ui/Kocab.fits" << "good";
    // An autofocus run towards focus (see Tests/fitsviewer/testfitsdata.cpp). The first two are a little out of
    // focus, but their stars keep a bright core, as with an unobstructed telescope near focus: only their size
    // against a focused frame tells, not their shape.
    QTest::newRow("autofocus 1") << "../../../fitsviewer/ngc4535-autofocus1.fits" << "good";
    QTest::newRow("autofocus 2") << "../../../fitsviewer/ngc4535-autofocus2.fits" << "good";
    QTest::newRow("autofocus 3, focused") << "../../../fitsviewer/ngc4535-autofocus3.fits" << "good";
    QTest::newRow("simulated M47") << "../../../fitsviewer/m47_sim_stars.fits" << "good";
}

void TestImageAnalysis::testRealFrames()
{
    QFETCH(QString, file);
    QFETCH(QString, verdict);

    const QString path = QFINDTESTDATA(file);
    if (path.isEmpty())
        QSKIP(qPrintable(file + " not found"));
    ImageStats stats;
    QVERIFY(measure(path, stats));
    QCOMPARE(imageVerdictKey(stats.verdict), verdict);
}

void TestImageAnalysis::testExtraFiles()
{
    const QString files = qEnvironmentVariable("IMAGE_ANALYSIS_FILES");
    if (files.isEmpty())
        QSKIP("Set IMAGE_ANALYSIS_FILES to analyse more frames.");
    for (const auto &file : files.split(':', Qt::SkipEmptyParts))
    {
        ImageStats stats;
        QVERIFY2(measure(file, stats), qPrintable(file));
    }
}

QTEST_GUILESS_MAIN(TestImageAnalysis)

#include "testimageanalysis.moc"
