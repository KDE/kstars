/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "../testhelpers.h"

#include "kstarsdata.h"
#include "planetviewer.h"
#include "pvplotwidget.h"

#include <kplotobject.h>
#include <kplotpoint.h>

#include <QTest>

/**
 * @brief Tests mouse handling of the Solar System Viewer (Tools > Solar System).
 *
 * Regression test for bug 528951: double-clicking the plot crashed KStars because the
 * double-click handler looked up nine planet plot objects, while the viewer only creates
 * eight (Pluto was removed).
 */
class TestPlanetViewer : public QObject
{
        Q_OBJECT

    private slots:
        void initTestCase();
        void cleanupTestCase();

        void testPlanetObjectOutOfRange();
        void testDoubleClickBeforePlotObjects();
        void testDoubleClickEmptySpace_data();
        void testDoubleClickEmptySpace();
        void testDoubleClickPlanet_data();
        void testDoubleClickPlanet();

    private:
        static PVPlotWidget *showViewer(PlanetViewer &viewer);
        static QPoint widgetPosition(PVPlotWidget *map, const QPointF &dataPoint);
};

void TestPlanetViewer::initTestCase()
{
    // Installs the data files (VSOP87 tables, orbit files) the planets need.
    KTEST_BEGIN();

    // KSPlanetBase::findPosition() needs KStarsData::Instance()->skyComposite().
    KStarsData *data = KStarsData::Create();
    QVERIFY(data != nullptr);
    QVERIFY(data->initialize());
}

void TestPlanetViewer::cleanupTestCase()
{
    KTEST_END();
}

PVPlotWidget *TestPlanetViewer::showViewer(PlanetViewer &viewer)
{
    viewer.show();
    if (!QTest::qWaitForWindowExposed(&viewer))
        return nullptr;

    // The plot objects are created by a zero-delay timer after construction.
    auto plotObjectsReady = [&viewer]()
    {
        return viewer.planetCount() > 0 && viewer.planetObject(viewer.planetCount() - 1) != nullptr;
    };
    if (!QTest::qWaitFor(plotObjectsReady))
        return nullptr;

    return viewer.findChild<PVPlotWidget *>(QStringLiteral("map"));
}

QPoint TestPlanetViewer::widgetPosition(PVPlotWidget *map, const QPointF &dataPoint)
{
    // mapToWidget() works in plot-area coordinates, which start after the padding.
    const QPointF p = map->mapToWidget(dataPoint) + QPointF(map->leftPadding(), map->topPadding());
    return p.toPoint();
}

void TestPlanetViewer::testPlanetObjectOutOfRange()
{
    PlanetViewer viewer;
    QVERIFY(showViewer(viewer) != nullptr);

    QCOMPARE(viewer.planetCount(), 8);
    for (int i = 0; i < viewer.planetCount(); ++i)
        QVERIFY(viewer.planetObject(i) != nullptr);

    // The ninth slot (Pluto) has no plot object.
    QCOMPARE(viewer.planetObject(viewer.planetCount()), static_cast<KPlotObject *>(nullptr));
}

void TestPlanetViewer::testDoubleClickBeforePlotObjects()
{
    PlanetViewer viewer;
    PVPlotWidget *map = viewer.findChild<PVPlotWidget *>(QStringLiteral("map"));
    QVERIFY(map != nullptr);

    // No event loop has run yet, so the plot objects do not exist.
    QCOMPARE(viewer.planetObject(0), static_cast<KPlotObject *>(nullptr));

    QTest::mouseDClick(map, Qt::LeftButton, Qt::NoModifier, map->rect().center());
    QVERIFY(viewer.centerPlanet().isEmpty());
}

void TestPlanetViewer::testDoubleClickEmptySpace_data()
{
    QTest::addColumn<Qt::MouseButton>("button");

    // Bug 528951 was reported with a right-button double-click.
    QTest::newRow("right button") << Qt::RightButton;
    QTest::newRow("left button") << Qt::LeftButton;
    QTest::newRow("middle button") << Qt::MiddleButton;
}

void TestPlanetViewer::testDoubleClickEmptySpace()
{
    QFETCH(Qt::MouseButton, button);

    PlanetViewer viewer;
    PVPlotWidget *map = showViewer(viewer);
    QVERIFY(map != nullptr);

    // 40 AU in x and y is beyond Neptune's orbit (about 30 AU) but inside the default view.
    QTest::mouseDClick(map, button, Qt::NoModifier, widgetPosition(map, QPointF(40.0, 40.0)));

    QVERIFY2(viewer.centerPlanet().isEmpty(), qPrintable(viewer.centerPlanet()));
}

void TestPlanetViewer::testDoubleClickPlanet_data()
{
    QTest::addColumn<int>("index");

    // The inner planets lie within a few pixels of each other in the default view, so use
    // the outer planets, which are always far apart.
    QTest::newRow("Jupiter") << 4;
    QTest::newRow("Saturn") << 5;
    QTest::newRow("Uranus") << 6;
    QTest::newRow("Neptune") << 7;
}

void TestPlanetViewer::testDoubleClickPlanet()
{
    QFETCH(int, index);

    PlanetViewer viewer;
    PVPlotWidget *map = showViewer(viewer);
    QVERIFY(map != nullptr);

    KPlotObject *object = viewer.planetObject(index);
    QVERIFY(object != nullptr);
    QVERIFY(!object->points().isEmpty());
    const KPlotPoint *point = object->points().at(0);

    QTest::mouseDClick(map, Qt::LeftButton, Qt::NoModifier,
                       widgetPosition(map, QPointF(point->x(), point->y())));

    QCOMPARE(viewer.centerPlanet(), viewer.planetName(index));
}

QTEST_MAIN(TestPlanetViewer)

#include "testplanetviewer.moc"
