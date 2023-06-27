/*
    SPDX-FileCopyrightText: 2021 Jasem Mutlaq

    SPDX-License-Identifier: GPL-2.0-or-later
*/


#include <QTest>
#include <memory>

#include <QObject>
#include "fitsviewer/fitsdata.h"
#include "ekos/auxiliary/darkprocessor.h"
#include "ekos/auxiliary/defectmap.h"

// At this point, only the methods in focusalgorithms.h are tested.

class TestDefects : public QObject
{
        Q_OBJECT

    public:
        /** @short Constructor */
        TestDefects();

        /** @short Destructor */
        ~TestDefects() override = default;

    private slots:
        void basicTest();
};

#include "testdefects.moc"

TestDefects::TestDefects() : QObject()
{
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
    for (const auto &onePixel : qAsConst(hot))
    {
        const int value = onePixel.first + onePixel.second * defectiveData->width();
        QVERIFY(buffer[value] < 100);
    }
}

QTEST_GUILESS_MAIN(TestDefects)
