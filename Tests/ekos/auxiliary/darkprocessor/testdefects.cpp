/*
    SPDX-FileCopyrightText: 2021 Jasem Mutlaq

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtTest/QTest>
#else
#include <QTest>
#endif

#include <memory>

#include <QObject>
#include "fitsviewer/fitsdata.h"
#include "ekos/auxiliary/darkprocessor.h"
#include "ekos/auxiliary/defectmap.h"
#include "Options.h"
#include "darkframes.h"

#include <QScopeGuard>
#include "../../../testhelpers.h"

using DarkFrames::makeFrame;
using DarkFrames::pixel;

class TestDefects : public QObject
{
        Q_OBJECT

    public:
        /** @short Constructor */
        TestDefects();

        /** @short Destructor */
        ~TestDefects() override = default;

    private Q_SLOTS:
        void initTestCase();
        void cleanupTestCase();

        void basicTest();
        /** @brief Every defect of a frame above the old 500 000 pixel sampling limit must be found and fixed */
        void largeFrameTest();
        /** @brief A defect on a raw colour (CFA) frame must be replaced from neighbours of its own colour */
        void bayerTest();
        /** @brief Defects outside a subframe must be skipped, without touching any other pixel */
        void subframeTest();
        /** @brief All three planes of a debayered frame must be corrected */
        void debayeredTest();
        /** @brief A noisy dark must not make the defect map keep millions of candidate pixels */
        void candidateLimitTest();
        /** @brief A colour guide frame is already debayered: its defects are replaced from adjacent pixels */
        void guideModeColourTest();

    private:
        bool m_AutoDebayer { true };
};

#include "testdefects.moc"

TestDefects::TestDefects() : QObject()
{
}

void TestDefects::initTestCase()
{
    KTEST_BEGIN();
    // Keep raw colour frames raw, as on a Raspberry Pi
    m_AutoDebayer = Options::autoDebayer();
    Options::setAutoDebayer(false);
}

void TestDefects::cleanupTestCase()
{
    Options::setAutoDebayer(m_AutoDebayer);
    KTEST_END();
}

void TestDefects::largeFrameTest()
{
    // 1200 x 1000 = 1.2 M pixels: the old code only examined every 3rd pixel of every 3rd row
    const int width = 1200, height = 1000;
    QVector<quint16> values(width * height);
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
            values[y * width + x] = 1000 + (x * 7 + y * 13) % 5;

    // 200 hot and 50 cold pixels, never adjacent, away from the 4 pixel border
    QList<QPoint> hot, cold;
    for (int i = 0; i < 250; i++)
    {
        const QPoint p(10 + (i * 97) % (width - 20), 10 + (i * 61) % (height - 20));
        values[p.y() * width + p.x()] = (i < 200) ? 5000 : 200;
        (i < 200 ? hot : cold) << p;
    }

    auto dark = makeFrame(width, height, values);
    auto light = makeFrame(width, height, values);
    QVERIFY(dark && light);

    QSharedPointer<DefectMap> map(new DefectMap());
    map->setDarkData(dark);
    QCOMPARE(map->hotCount(), static_cast<uint32_t>(hot.size()));
    QCOMPARE(map->coldCount(), static_cast<uint32_t>(cold.size()));

    Ekos::DarkProcessor processor;
    processor.normalizeDefects(map, light, 0, 0);

    int left = 0;
    for (const auto &p : hot + cold)
        if (std::abs(static_cast<int>(pixel(light, 0, p.x(), p.y())) - 1002) > 5)
            left++;
    QVERIFY2(left == 0, qPrintable(QString("%1 of %2 defects were not corrected").arg(left).arg(hot.size() + cold.size())));
}

void TestDefects::bayerTest()
{
    // RGGB light with a different level per colour, as with a real sky background
    const int width = 64, height = 64;
    auto level = [](int x, int y)
    {
        if (y % 2 == 0)
            return x % 2 == 0 ? 500 : 1000; // R G
        return x % 2 == 0 ? 1000 : 300;     // G B
    };

    // A dark has no colour: flat, with a hot pixel on a red, a green and a blue site
    const QList<QPoint> defects { {20, 20}, {31, 20}, {41, 41} };
    QVector<quint16> darkValues(width * height, 100), lightValues(width * height);
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
            lightValues[y * width + x] = level(x, y);
    for (const auto &p : defects)
    {
        darkValues[p.y() * width + p.x()] = 4000;
        lightValues[p.y() * width + p.x()] = 4000;
    }

    auto dark = makeFrame(width, height, darkValues, 1, "RGGB");
    auto light = makeFrame(width, height, lightValues, 1, "RGGB");
    QVERIFY(dark && light);
    QCOMPARE(light->channels(), 1);

    QSharedPointer<DefectMap> map(new DefectMap());
    map->setDarkData(dark);
    QCOMPARE(map->hotCount(), static_cast<uint32_t>(defects.size()));

    Ekos::DarkProcessor processor;
    processor.normalizeDefects(map, light, 0, 0);

    for (const auto &p : defects)
        QVERIFY2(pixel(light, 0, p.x(), p.y()) == level(p.x(), p.y()),
                 qPrintable(QString("Defect at %1,%2 became %3 instead of its colour level %4")
                            .arg(p.x()).arg(p.y()).arg(pixel(light, 0, p.x(), p.y())).arg(level(p.x(), p.y()))));
}

void TestDefects::subframeTest()
{
    // The defect map covers the whole 200 x 200 sensor, the light is a 50 x 40 subframe at (60, 70)
    const int width = 200, height = 200;
    const int subX = 60, subY = 70, subW = 50, subH = 40;
    QVector<quint16> darkValues(width * height, 100);
    const QList<QPoint> inside { {80, 90}, {100, 105} };
    // Right of the subframe within its rows, left of it, and above it
    const QList<QPoint> outside { {120, 80}, {130, 100}, {30, 90}, {80, 20} };
    for (const auto &p : inside + outside)
        darkValues[p.y() * width + p.x()] = 4000;

    // A textured background, so that a median written to the wrong pixel changes its value
    auto background = [](int x, int y)
    {
        return static_cast<quint16>(500 + (x * 7 + y * 13) % 11);
    };
    QVector<quint16> lightValues(subW * subH);
    for (int y = 0; y < subH; y++)
        for (int x = 0; x < subW; x++)
            lightValues[y * subW + x] = background(x, y);
    for (const auto &p : inside)
        lightValues[(p.y() - subY) * subW + (p.x() - subX)] = 4000;

    auto dark = makeFrame(width, height, darkValues);
    auto light = makeFrame(subW, subH, lightValues);
    QVERIFY(dark && light);

    QSharedPointer<DefectMap> map(new DefectMap());
    map->setDarkData(dark);
    QCOMPARE(map->hotCount(), static_cast<uint32_t>(inside.size() + outside.size()));

    Ekos::DarkProcessor processor;
    processor.normalizeDefects(map, light, subX, subY);

    // The defects inside are fixed, and the defects outside must not have been written anywhere into it
    int wrong = 0;
    for (int y = 0; y < subH; y++)
        for (int x = 0; x < subW; x++)
        {
            const bool defect = inside.contains(QPoint(x + subX, y + subY));
            const int value = pixel(light, 0, x, y);
            if (defect ? std::abs(value - 505) > 6 : value != background(x, y))
                wrong++;
        }
    QVERIFY2(wrong == 0, qPrintable(QString("%1 subframe pixels are wrong").arg(wrong)));
}

void TestDefects::debayeredTest()
{
    // A debayered light has three planes; a hot sensor pixel shows up in all of them
    const int width = 64, height = 64;
    const QList<QPoint> defects { {20, 20}, {33, 40} };
    QVector<quint16> darkValues(width * height * 3, 100), lightValues(width * height * 3);
    for (int plane = 0; plane < 3; plane++)
        for (int i = 0; i < width * height; i++)
            lightValues[plane * width * height + i] = 1000 * (plane + 1);
    for (int plane = 0; plane < 3; plane++)
        for (const auto &p : defects)
        {
            darkValues[plane * width * height + p.y() * width + p.x()] = 9000;
            lightValues[plane * width * height + p.y() * width + p.x()] = 9000;
        }

    auto dark = makeFrame(width, height, darkValues, 3);
    auto light = makeFrame(width, height, lightValues, 3);
    QVERIFY(dark && light);
    QCOMPARE(light->channels(), 3);

    QSharedPointer<DefectMap> map(new DefectMap());
    map->setDarkData(dark);
    QCOMPARE(map->hotCount(), static_cast<uint32_t>(defects.size()));

    Ekos::DarkProcessor processor;
    processor.normalizeDefects(map, light, 0, 0);

    for (int plane = 0; plane < 3; plane++)
        for (const auto &p : defects)
            QVERIFY2(pixel(light, plane, p.x(), p.y()) == 1000 * (plane + 1),
                     qPrintable(QString("Plane %1 still has the defect at %2,%3").arg(plane).arg(p.x()).arg(p.y())));
}

void TestDefects::guideModeColourTest()
{
    // Guide, Focus and Align debayer a colour frame and keep only its first plane. The header still has
    // BAYERPAT, but the plane holds a value for every pixel, so the adjacent pixels are the right neighbours.
    // With the DC1394 engine they keep only the first plane; the OpenCV engine keeps all three.
    const int engine = Options::imageDebayerEngine();
    Options::setAutoDebayer(true);
    Options::setImageDebayerEngine(static_cast<int>(DebayerEngine::DC1394));
    auto restore = qScopeGuard([engine]()
    {
        Options::setAutoDebayer(false);
        Options::setImageDebayerEngine(engine);
    });

    const int width = 64, height = 64;
    const QPoint defect(20, 20);
    QVector<quint16> darkValues(width * height, 100);
    darkValues[defect.y() * width + defect.x()] = 9000;
    auto dark = makeFrame(width, height, darkValues);
    auto light = makeFrame(width, height, QVector<quint16>(width * height, 1000), 1, "RGGB", FITS_GUIDE);
    QVERIFY(dark && light);
    QCOMPARE(light->channels(), 1);
    QVERIFY(light->hasDebayer());

    // The defect, its adjacent pixels at the background, and the pixels two away much brighter
    auto *buffer = reinterpret_cast<quint16 *>(light->getWritableImageBuffer());
    for (int dy = -2; dy <= 2; dy++)
        for (int dx = -2; dx <= 2; dx++)
            buffer[(defect.y() + dy) * width + defect.x() + dx] = (std::abs(dx) == 2 || std::abs(dy) == 2) ? 5000 : 1000;
    buffer[defect.y() * width + defect.x()] = 9000;

    QSharedPointer<DefectMap> map(new DefectMap());
    map->setDarkData(dark);
    QCOMPARE(map->hotCount(), 1u);

    Ekos::DarkProcessor processor;
    processor.normalizeDefects(map, light, 0, 0);
    QCOMPARE(static_cast<int>(pixel(light, 0, defect.x(), defect.y())), 1000);
}

void TestDefects::candidateLimitTest()
{
    // Uniform noise: most pixels lie beyond the most aggressive threshold (0.26 sigma)
    const int width = 1000, height = 1000;
    QVector<quint16> values(width * height);
    uint32_t state = 12345;
    for (auto &value : values)
    {
        state = state * 1664525u + 1013904223u;
        value = 1000 + (state >> 24) % 64;
    }
    // and 100 real hot pixels
    for (int i = 0; i < 100; i++)
        values[(20 + (i * 37) % 960) * width + 20 + (i * 53) % 960] = 8000;

    auto dark = makeFrame(width, height, values);
    QVERIFY(dark);

    QSharedPointer<DefectMap> map(new DefectMap());
    map->setDarkData(dark);

    // At most 1% of the pixels are kept as candidates of each kind, and the real defects are among them
    QVERIFY2(map->hotPixels().size() <= static_cast<size_t>(width * height / 100),
             qPrintable(QString("%1 hot candidates kept").arg(map->hotPixels().size())));
    QVERIFY2(map->coldPixels().size() <= static_cast<size_t>(width * height / 100),
             qPrintable(QString("%1 cold candidates kept").arg(map->coldPixels().size())));
    QVERIFY(map->hotPixels().size() >= 100);
    const auto realHot = std::count_if(map->hotPixels().cbegin(), map->hotPixels().cend(), [](const BadPixel & p)
    {
        return p.value >= 8000;
    });
    QCOMPARE(static_cast<int>(realHot), 100);
}


void TestDefects::basicTest()
{
    const QString filename = "../Tests/ekos/auxiliary/darkprocessor/hotpixels.fits";
    if (!QFileInfo::exists(filename))
        QSKIP(QString("Failed to locate file %1, skipping test.").arg(filename).toLatin1());

    // Load FITS Data
    QSharedPointer<FITSData> defectiveData;
    defectiveData.reset(new FITSData());
    QFuture<bool> result = defectiveData->loadFromFile(filename);
    result.waitForFinished();

    if (result.result() == false)
        QSKIP("Failed to load image, skipping test.");

    // Get Image Buffer
    uint8_t *buffer = defectiveData->getWritableImageBuffer();

    // Inject noise in a couple of places
    // 50, 10
    buffer[50 + 10 * 100] = 255;
    // 80, 80
    buffer[80 + 80 * 100] = 255;

    // Look for hot pixels above 200 value
    // Crop by 3 pixels on all sides since we need to ignore this region as
    // it cannot be cleaned up by the algorithm
    QList<QPair<int, int>> hot;
    for (int y = 3; y < defectiveData->height() - 3; y++)
    {
        for (int x = 3; x < defectiveData->width() - 3; x++)
        {
            if (buffer[x + y * defectiveData->width()] > 200)
                hot << qMakePair(x, y);
        }

    }

    // Verify we have 3+2 (injected) = 5 above 200
    QVERIFY(hot.size() == 5);

    // Now set the dark frame to the defect map
    // It should automatically create a list of hot pixels
    QSharedPointer<DefectMap> map;
    map.reset(new DefectMap());
    map->setDarkData(defectiveData);

    // Now fix the hot pixels
    QPointer<Ekos::DarkProcessor> processor = new Ekos::DarkProcessor();
    processor->normalizeDefects(map, defectiveData, 0, 0);

    // Verify that the buffer was indeed updated and fixed
    for (const auto &onePixel : std::as_const(hot))
    {
        const int value = onePixel.first + onePixel.second * defectiveData->width();
        QVERIFY(buffer[value] < 100);
    }
}

QTEST_GUILESS_MAIN(TestDefects)
