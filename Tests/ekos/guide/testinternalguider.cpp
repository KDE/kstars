/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ekos/guide/internalguide/internalguider.h"
#include "ekos/guide/guideview.h"
#include "ekos/ekos.h"
#include "fitsviewer/fitsdata.h"
#include "config-kstars.h"
#include "Options.h"

#include "../../testhelpers.h"

#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtTest/QTest>
#else
#include <QTest>
#endif
#include <QRandomGenerator>
#include <QSignalSpy>

#include <fitsio.h>

#include <cmath>

class TestInternalGuider : public QObject
{
        Q_OBJECT

    private Q_SLOTS:
        void initTestCase();
        void cleanupTestCase();

        void testSuspendResume();
        void testRestartWhileSuspended();
        void testLocalSearchColour();
        void testFirstGuidingFrameColour();

    private:
        QSharedPointer<GuideView> m_View;
};

void TestInternalGuider::initTestCase()
{
    KTEST_BEGIN();
    m_View.reset(new GuideView());
}

void TestInternalGuider::cleanupTestCase()
{
    m_View.reset();
    KTEST_END();
}

// The regular capture-driven path: suspend for a non-light frame, then resume.
void TestInternalGuider::testSuspendResume()
{
    Ekos::InternalGuider guider;
    guider.setGuideView(m_View);

    QVERIFY(guider.guide());
    QVERIFY(!guider.isProcessingSuspended());

    QVERIFY(guider.suspend());
    QVERIFY(guider.isProcessingSuspended());

    QVERIFY(guider.resume());
    QVERIFY(!guider.isProcessingSuspended());

    guider.abort();
}

// Bug 526397: when a scheduler job repeats and its sequence ended with flats, guiding is still
// suspended, so SchedulerProcess::startGuiding() calls connectGuider() and then guide() over D-Bus.
// The restarted session reported GUIDE_GUIDING but kept skipping every frame (no corrections and no
// guide stats) because the suspension was never cleared.
void TestInternalGuider::testRestartWhileSuspended()
{
    Ekos::InternalGuider guider;
    guider.setGuideView(m_View);
    QSignalSpy statusSpy(&guider, &Ekos::GuideInterface::newStatus);

    QVERIFY(guider.guide());
    QVERIFY(guider.suspend());
    QVERIFY(guider.isProcessingSuspended());

    // What Guide::connectGuider() followed by Guide::guide() does to the internal guider.
    QVERIFY(guider.Connect());
    statusSpy.clear();
    QVERIFY(guider.guide());

    QVERIFY(!statusSpy.isEmpty());
    QCOMPARE(statusSpy.last().at(0).value<Ekos::GuideState>(), Ekos::GUIDE_GUIDING);
    QVERIFY(!guider.isProcessingSuspended());

    guider.abort();
}

// SEP MultiStar local star search: the Guide view draws the guide star boxes blue while it is used.
void TestInternalGuider::testLocalSearchColour()
{
    QVERIFY(!m_View->localSearchUsed());
    QCOMPARE(m_View->trackingBoxColor(), QColor(Qt::green));

    m_View->setLocalSearchUsed(true);
    QVERIFY(m_View->localSearchUsed());
    QCOMPARE(m_View->trackingBoxColor(), QColor(0, 160, 255));

    // Switching the local search off (Subframe unchecked) turns the boxes green right away.
    Ekos::InternalGuider guider;
    guider.setGuideView(m_View);
    guider.setMultiStarLocalSearch(false);
    QVERIFY(!m_View->localSearchUsed());
    QCOMPARE(m_View->trackingBoxColor(), QColor(Qt::green));
}

namespace
{
constexpr int FrameWidth = 1280;
constexpr int FrameHeight = 960;

// A 16-bit guide frame with 40 well separated Gaussian stars on a noisy sky, all shifted by dx,dy
// (the same kind of frame as in testguidestars).
QSharedPointer<FITSData> makeGuideFrame(double dx, double dy, quint32 noiseSeed)
{
    QRandomGenerator starRng(99);
    QList<QVector3D> stars;  // x, y, peak
    while (stars.size() < 40)
    {
        const QVector3D star(90 + starRng.bounded(FrameWidth - 180), 90 + starRng.bounded(FrameHeight - 180),
                             2000 + starRng.bounded(28000));
        bool tooClose = false;
        for (const auto &other : stars)
            tooClose = tooClose || std::hypot(other.x() - star.x(), other.y() - star.y()) < 30;
        if (!tooClose)
            stars.append(star);
    }

    QRandomGenerator rng(noiseSeed);
    QVector<double> image(FrameWidth * FrameHeight);
    for (auto &pixel : image)
    {
        const double u1 = std::max(rng.generateDouble(), 1e-12);
        pixel = 1000 + 15 * std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * M_PI * rng.generateDouble());
    }
    for (const auto &star : stars)
    {
        const double cx = star.x() + dx, cy = star.y() + dy, sigma = 1.6;
        for (int y = int(cy) - 10; y <= int(cy) + 10; ++y)
            for (int x = int(cx) - 10; x <= int(cx) + 10; ++x)
            {
                const double r2 = (x - cx) * (x - cx) + (y - cy) * (y - cy);
                image[y * FrameWidth + x] += star.z() * std::exp(-r2 / (2 * sigma * sigma));
            }
    }
    QVector<quint16> pixels(image.size());
    for (int i = 0; i < image.size(); ++i)
        pixels[i] = static_cast<quint16>(qBound(0.0, image[i], 65535.0));

    fitsfile *fptr = nullptr;
    void *fitsBuffer = nullptr;
    size_t fitsSize = 0;
    int status = 0;
    long naxes[2] = { FrameWidth, FrameHeight };
    fits_create_memfile(&fptr, &fitsBuffer, &fitsSize, 2880, realloc, &status);
    fits_create_img(fptr, USHORT_IMG, 2, naxes, &status);
    fits_write_img(fptr, TUSHORT, 1, pixels.size(), pixels.data(), &status);
    fits_flush_file(fptr, &status);
    fits_close_file(fptr, &status);
    if (status != 0)
    {
        free(fitsBuffer);
        return {};
    }
    QByteArray buffer(static_cast<const char *>(fitsBuffer), static_cast<int>(fitsSize));
    free(fitsBuffer);

    QSharedPointer<FITSData> data(new FITSData(FITS_GUIDE));
    data->setExtension("fits");
    if (!data->loadFromBuffer(buffer))
        return {};
    return data;
}
}  // namespace

// Blue guide star boxes only on frames whose star search was purely the local search. The first guiding
// frame also searches the full frame (to set the lock position) before the local search, so it stays green.
void TestInternalGuider::testFirstGuidingFrameColour()
{
#ifndef HAVE_STELLARSOLVER
    QSKIP("Needs StellarSolver for star detection.");
#endif
    Options::setGuideAlgorithm(SEP_MULTISTAR);
    Options::setGuideOptionsProfile(0);
    Options::setMinDetectionsSEPMultistar(5);
    Options::setMaxMultistarReferenceStars(10);
    Options::setGuideMaxHFR(4.5);
    Options::setAlwaysInventGuideStar(true);
    Options::setRAGuidePulseAlgorithm(0);
    Options::setSaveGuideImages(false);

    auto frame0 = makeGuideFrame(0, 0, 1);
    auto frame1 = makeGuideFrame(0.8, -0.6, 2);
    auto frame2 = makeGuideFrame(0.3, 0.4, 3);
    QVERIFY(frame0 && frame1 && frame2);

    Ekos::InternalGuider guider;
    guider.setGuideView(m_View);
    guider.setFrameParams(0, 0, FrameWidth, FrameHeight, 1, 1);
    guider.setStarDetectionAlgorithm(SEP_MULTISTAR);
    guider.setMultiStarLocalSearch(true);
    guider.setImageData(frame0);
    QVERIFY(guider.selectAutoStar());
    QVERIFY(guider.guide());

    // First guiding frame: full-frame search, then the local search on the same image.
    guider.setImageData(frame1);
    QVERIFY(guider.guide());
    QVERIFY(!m_View->localSearchUsed());
    QCOMPARE(m_View->trackingBoxColor(), QColor(Qt::green));

    // Next frame: only the local search.
    guider.setImageData(frame2);
    QVERIFY(guider.guide());
    QVERIFY(m_View->localSearchUsed());
    QCOMPARE(m_View->trackingBoxColor(), QColor(0, 160, 255));

    guider.abort();
    m_View->setLocalSearchUsed(false);
}

QTEST_MAIN(TestInternalGuider)

#include "testinternalguider.moc"
