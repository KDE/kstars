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
#include "darkframes.h"
#include "Options.h"

#include <QScopeGuard>
#include "../../../testhelpers.h"

using DarkFrames::makeFrame;
using DarkFrames::pixel;

class TestSubtraction : public QObject
{
        Q_OBJECT

    public:
        TestSubtraction();
        ~TestSubtraction() override = default;

    private Q_SLOTS:
        void initTestCase();
        void cleanupTestCase();

        void basicTest();
        /** @brief A debayered light must have the dark subtracted from all three planes */
        void multiChannelTest();
        /** @brief A dark with a different number of channels than the light must not be subtracted */
        void channelMismatchTest();
        /** @brief A colour guide frame (debayered, one plane in use) must still get its 3-plane master dark subtracted */
        void guideModeColourTest();
};

void TestSubtraction::guideModeColourTest()
{
    // Guide, Focus and Align debayer a colour frame but only use its first plane, while the master dark
    // is loaded for calibration with all three planes
    // With the DC1394 engine they keep only the first plane; the OpenCV engine keeps all three.
    const bool autoDebayer = Options::autoDebayer();
    const int engine = Options::imageDebayerEngine();
    Options::setAutoDebayer(true);
    Options::setImageDebayerEngine(static_cast<int>(DebayerEngine::DC1394));
    auto restore = qScopeGuard([autoDebayer, engine]()
    {
        Options::setAutoDebayer(autoDebayer);
        Options::setImageDebayerEngine(engine);
    });

    const int width = 64, height = 48;
    auto dark = makeFrame(width, height, QVector<quint16>(width * height, 100), 1, "RGGB", FITS_CALIBRATE);
    auto light = makeFrame(width, height, QVector<quint16>(width * height, 1000), 1, "RGGB", FITS_GUIDE);
    QVERIFY(dark && light);
    QCOMPARE(dark->channels(), 3);
    QCOMPARE(light->channels(), 1);

    Ekos::DarkProcessor processor;
    QVERIFY(processor.subtractDarkData(dark, light, 0, 0));
    QCOMPARE(static_cast<int>(pixel(light, 0, 20, 20)), 900);
}

void TestSubtraction::initTestCase()
{
    KTEST_BEGIN();
}

void TestSubtraction::cleanupTestCase()
{
    KTEST_END();
}

void TestSubtraction::multiChannelTest()
{
    const int width = 32, height = 24;
    QVector<quint16> darkValues(width * height * 3), lightValues(width * height * 3);
    for (int plane = 0; plane < 3; plane++)
        for (int i = 0; i < width * height; i++)
        {
            darkValues[plane * width * height + i] = 100 + plane;
            lightValues[plane * width * height + i] = 1000 * (plane + 1);
        }

    auto dark = makeFrame(width, height, darkValues, 3);
    auto light = makeFrame(width, height, lightValues, 3);
    QVERIFY(dark && light);
    QCOMPARE(light->channels(), 3);

    Ekos::DarkProcessor processor;
    QVERIFY(processor.subtractDarkData(dark, light, 0, 0));

    const QList<QPoint> points { QPoint(0, 0), QPoint(17, 9), QPoint(width - 1, height - 1) };
    for (int plane = 0; plane < 3; plane++)
        for (const auto &p : points)
            QCOMPARE(static_cast<int>(pixel(light, plane, p.x(), p.y())), 1000 * (plane + 1) - (100 + plane));
}

void TestSubtraction::channelMismatchTest()
{
    const int width = 32, height = 24;
    auto dark = makeFrame(width, height, QVector<quint16>(width * height, 100), 1);
    auto light = makeFrame(width, height, QVector<quint16>(width * height * 3, 1000), 3);
    QVERIFY(dark && light);

    Ekos::DarkProcessor processor;
    QVERIFY(!processor.subtractDarkData(dark, light, 0, 0));

    // the light is left as it was
    for (int plane = 0; plane < 3; plane++)
        QCOMPARE(static_cast<int>(pixel(light, plane, 5, 5)), 1000);
}

#include "testsubtraction.moc"

TestSubtraction::TestSubtraction() : QObject()
{
}

void TestSubtraction::basicTest()
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
    uint8_t const *buffer = defectiveData->getImageBuffer();

    // Subtract from self
    QPointer<Ekos::DarkProcessor> processor = new Ekos::DarkProcessor();
    processor->subtractDarkData(defectiveData, defectiveData, 0, 0);

    // Verify that the buffer was indeed updated and zeroed, only check every 1000 pixels.
    for (uint32_t i = 0; i < defectiveData->samplesPerChannel(); i += 1000)
        QCOMPARE(buffer[i], 0);
}


QTEST_GUILESS_MAIN(TestSubtraction)
