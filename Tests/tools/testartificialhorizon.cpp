/*
    SPDX-FileCopyrightText: 2021 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/*
 * This file contains unit tests for the ArtificialHorizon class.
 */

#include <QObject>
#include <QtTest>
#include <memory>

#include "artificialhorizoncomponent.h"
#include "linelist.h"
#include "Options.h"

class TestArtificialHorizon : public QObject
{
        Q_OBJECT

    public:
        /** @short Constructor */
        TestArtificialHorizon();

        /** @short Destructor */
        ~TestArtificialHorizon() override = default;

    private slots:
        void artificialHorizonTest();

    private:
};

// This include must go after the class declaration.
#include "testartificialhorizon.moc"

TestArtificialHorizon::TestArtificialHorizon() : QObject()
{
}

namespace
{

// Pass in two lists with azimuth and altitude values to set up an artificial horizon LineList.
std::shared_ptr<LineList> setupHorizonEntities(const QList<double> &az, const QList<double> alt)
{
    std::shared_ptr<LineList> list(new LineList());
    std::shared_ptr<SkyPoint> p;

    for (int i = 0; i < az.size(); ++i)
    {
        p.reset(new SkyPoint());
        p->setAz(dms(az[i]));
        p->setAlt(dms(alt[i]));
        list->append(p);
    }
    return list;
}

// Checks both that horizon.isVisible(az, alt) == visibility, as well as making sure that the
// given az,alt is contained in one of the polygons (if visibility is false) or not contained
// if visibility is true. Those polygons should be those generated to render the red
// artificial-horizon area on the SkyMap.
bool checkHorizon(const ArtificialHorizon &horizon, double az, double alt,
                  bool visibility, QList<LineList> &polygons)
{
    bool azAltInPolygon = false;
    for (int i = 0; i < polygons.size(); ++i)
    {
        const SkyList &points = *(polygons[i].points());
        if (points.size() < 3)
            return false;

        QPolygonF polygon;
        for (int j = 0; j < points.size(); ++j)
            polygon << QPointF(points[j]->az().Degrees(), points[j]->alt().Degrees());
        polygon << QPointF(points[0]->az().Degrees(), points[0]->alt().Degrees());

        if (polygon.containsPoint(QPointF(az, alt), Qt::OddEvenFill))
        {
            azAltInPolygon = true;
            break;
        }
    }
    return (horizon.isVisible(az, alt) == visibility) && (azAltInPolygon != visibility);
}

}  // namespace

void TestArtificialHorizon::artificialHorizonTest()
{
    ArtificialHorizon horizon;

    // This removes a call to KStars::Instance() that's not needed in testing.
    horizon.setTesting();

    // Setup a simple 3-point horizon.
    QList<double> az1  = {10.0, 20.0, 30.0};
    QList<double> alt1 = {20.0, 40.0, 26.0};
    auto list1 = setupHorizonEntities(az1, alt1);
    horizon.addRegion("R1", true, list1, false);

    QVERIFY(horizon.altitudeConstraintsExist());
    QList<LineList> polygons;
    horizon.drawPolygons(nullptr, &polygons);

    // Check points above and below the first horizon line.
    QVERIFY(checkHorizon(horizon, 20, 41, true, polygons));
    QVERIFY(checkHorizon(horizon, 20, 39, false, polygons));
    QVERIFY(checkHorizon(horizon, 20.0, 51, true, polygons));
    QVERIFY(checkHorizon(horizon, 25.0, 34, true, polygons));
    QVERIFY(checkHorizon(horizon, 25.0, 32, false, polygons));
    QVERIFY(checkHorizon(horizon, 15.0, 31, true, polygons));
    QVERIFY(checkHorizon(horizon, 15.0, 29, false, polygons));
    QVERIFY(checkHorizon(horizon, 35.0, -90 + 1, true, polygons));
    QVERIFY(checkHorizon(horizon, 5.0, -90 + 1, true, polygons));

    // Try adding and subtracting 360-degrees from the azimuth for
    // the same tests. Shouldn't matter.
    // Graphics test doesn't make sense for >360-degrees so just test isVisible().
    QVERIFY(horizon.isVisible(360.0 + 20.0, 41));
    QVERIFY(!horizon.isVisible(360.0 + 20.0, 39));
    QVERIFY(horizon.isVisible(-360.0 + 20.0, 41));
    QVERIFY(!horizon.isVisible(-360.0 + 20.0, 39));
    QVERIFY(horizon.isVisible(360.0 + 25.0, 34));
    QVERIFY(!horizon.isVisible(360.0 + 25.0, 32));
    QVERIFY(horizon.isVisible(-360.0 + 25.0, 34));
    QVERIFY(!horizon.isVisible(-360.0 + 25.0, 32));
    QVERIFY(horizon.isVisible(360.0 + 15.0, 31));
    QVERIFY(!horizon.isVisible(360.0 + 15.0, 29));
    QVERIFY(horizon.isVisible(-360.0 + 15.0, 31));
    QVERIFY(!horizon.isVisible(-360.0 + 15.0, 29));
    QVERIFY(horizon.isVisible(360.0 + 35.0, -90 + 1));
    QVERIFY(horizon.isVisible(360.0 + 5.0, -90 + 1));
    QVERIFY(horizon.isVisible(-360.0 + 35.0, -90 + 1));
    QVERIFY(horizon.isVisible(-360.0 + 5.0, -90 + 1));

    // Disable that horizon. All tests above shoud be visible.
    horizon.findRegion("R1")->setEnabled(false);
    polygons.clear();
    horizon.drawPolygons(nullptr, &polygons);

    QVERIFY(!horizon.altitudeConstraintsExist());
    QVERIFY(checkHorizon(horizon, 20, 41, true, polygons));
    QVERIFY(checkHorizon(horizon, 20, 39, true, polygons));
    QVERIFY(checkHorizon(horizon, 25.0, 34, true, polygons));
    QVERIFY(checkHorizon(horizon, 25.0, 32, true, polygons));
    QVERIFY(checkHorizon(horizon, 15.0, 31, true, polygons));
    QVERIFY(checkHorizon(horizon, 15.0, 29, true, polygons));
    QVERIFY(checkHorizon(horizon, 35.0, -90 + 1, true, polygons));
    QVERIFY(checkHorizon(horizon, 5.0, -90 + 1, true, polygons));

    // Re-enable it
    horizon.findRegion("R1")->setEnabled(true);
    QVERIFY(horizon.altitudeConstraintsExist());

    // Now add a ceiling above the first horizon line.
    // Above the ceiling should not be visible
    QList<double> az2  = {14.0, 24.0};
    QList<double> alt2 = {50.0, 50.0};
    auto list2 = setupHorizonEntities(az2, alt2);
    horizon.addRegion("R2", true, list2, true);
    polygons.clear();
    horizon.drawPolygons(nullptr, &polygons);

    QVERIFY(checkHorizon(horizon, 18, 51, false, polygons));
    QVERIFY(checkHorizon(horizon, 18, 60, false, polygons));
    QVERIFY(checkHorizon(horizon, 22, 51, false, polygons));
    QVERIFY(checkHorizon(horizon, 22, 60, false, polygons));
    QVERIFY(checkHorizon(horizon, 18, 49, true, polygons));
    // but it doesn't affect things to its side
    QVERIFY(checkHorizon(horizon, 13, 51, true, polygons));
    QVERIFY(checkHorizon(horizon, 13, 49, true, polygons));
    QVERIFY(checkHorizon(horizon, 25, 51, true, polygons));
    QVERIFY(checkHorizon(horizon, 25, 49, true, polygons));

    // Disable the ceiling. Those points become visible again.
    horizon.findRegion("R2")->setEnabled(false);
    polygons.clear();
    horizon.drawPolygons(nullptr, &polygons);

    QVERIFY(checkHorizon(horizon, 18, 51, true, polygons));
    QVERIFY(checkHorizon(horizon, 18, 60, true, polygons));
    QVERIFY(checkHorizon(horizon, 22, 51, true, polygons));
    QVERIFY(checkHorizon(horizon, 22, 60, true, polygons));

    // Re-enable the ceiling.
    horizon.findRegion("R2")->setEnabled(true);
    polygons.clear();
    horizon.drawPolygons(nullptr, &polygons);
    QVERIFY(checkHorizon(horizon, 18, 51, false, polygons));
    QVERIFY(checkHorizon(horizon, 18, 60, false, polygons));
    QVERIFY(checkHorizon(horizon, 22, 51, false, polygons));
    QVERIFY(checkHorizon(horizon, 22, 60, false, polygons));

    // Add another horizon line above the ceiling again makes that visible.
    QList<double> az3  = {10.0, 20.0};
    QList<double> alt3 = {50.5, 50.2};
    auto list3 = setupHorizonEntities(az3, alt3);
    horizon.addRegion("R3", true, list3, false);
    polygons.clear();
    horizon.drawPolygons(nullptr, &polygons);

    QVERIFY(checkHorizon(horizon, 18, 51, true, polygons));
    QVERIFY(checkHorizon(horizon, 18, 60, true, polygons));
    // but it shouldn't affect other az values
    QVERIFY(checkHorizon(horizon, 22, 51, false, polygons));
    QVERIFY(checkHorizon(horizon, 22, 60, false, polygons));

}

QTEST_GUILESS_MAIN(TestArtificialHorizon)
