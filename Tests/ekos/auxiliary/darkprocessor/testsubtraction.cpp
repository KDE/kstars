/*
    SPDX-FileCopyrightText: 2021 Jasem Mutlaq

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QTest>
#include <memory>

#include <QObject>
#include "fitsviewer/fitsdata.h"
#include "ekos/auxiliary/darkprocessor.h"

class TestSubtraction : public QObject
{
        Q_OBJECT

    public:
        TestSubtraction();
        ~TestSubtraction() override = default;

    private slots:
        void basicTest();
};

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
