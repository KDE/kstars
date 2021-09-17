/*  Artificial Horizon UI test
    SPDX-FileCopyrightText: 2021 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_artificial_horizon.h"

#if defined(HAVE_INDI)

#include <QStandardItemModel>

#include "artificialhorizoncomponent.h"
#include "kstars_ui_tests.h"
#include "horizonmanager.h"
#include "linelist.h"
#include "skycomponents/skymapcomposite.h"
#include "skymap.h"
#include "test_ekos.h"

TestArtificialHorizon::TestArtificialHorizon(QObject *parent) : QObject(parent)
{
}

void TestArtificialHorizon::initTestCase()
{
    // HACK: Reset clock to initial conditions
    KHACK_RESET_EKOS_TIME();
}

void TestArtificialHorizon::cleanupTestCase()
{
}

void TestArtificialHorizon::init()
{

}

void TestArtificialHorizon::cleanup()
{

}

void TestArtificialHorizon::testArtificialHorizon_data()
{

}

namespace
{

void skyClick(SkyMap *sky, int x, int y)
{
    QTest::mouseClick(sky->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(x, y), 100);
}

// Returns the QModelIndex for the nth point in the mth region
QModelIndex pointIndex(QStandardItemModel *model, int region, int point)
{
    QModelIndex badIndex;
    if (model == nullptr)
        return badIndex;
    auto reg = model->item(region, 0);
    if (reg == nullptr)
        return badIndex;
    auto child = reg->child(point, 1);
    if (child == nullptr)
        return badIndex;
    return child->index();
}

// Returns the QModelIndex for the nth region .
QModelIndex regionIndex(QStandardItemModel *model, int region)
{
    QModelIndex badIndex;
    if (model == nullptr)
        return badIndex;
    QModelIndex idx = model->index(region, 0);
    return idx;
}

// Simulates a left mouse clock on the given view.
// The click is centered vertically, and offset by leftOffset from the left size of the view.
bool clickView(QAbstractItemView *view, const QModelIndex &idx, int leftOffset)
{
    if (!idx.isValid())
        return false;
    QPoint itemPtCenter = view->visualRect(idx).center();
    if (itemPtCenter.isNull())
        return false;
    QPoint itemPtLeft = itemPtCenter;
    itemPtLeft.setX(view->visualRect(idx).left() + leftOffset);
    QTest::mouseClick(view->viewport(), Qt::LeftButton, 0, itemPtLeft);
    return true;
}

// Simulates a click on the enable checkbox for the region view.
bool clickEnableRegion(QAbstractItemView *view, int region)
{
    QStandardItemModel *model = static_cast<QStandardItemModel*>(view->model());
    const QModelIndex index = regionIndex(model, region);
    if (!index.isValid()) return false;
    // The checkbox is on the far left, so left offset is 5.
    return clickView(view, index, 5);
}

// Simulates a click on the nth region, in the region view.
bool clickSelectRegion(QAbstractItemView *view, int region)
{
    QStandardItemModel *model = static_cast<QStandardItemModel*>(view->model());
    const QModelIndex index = regionIndex(model, region);
    if (!index.isValid()) return false;
    // Clicks near the middle of the box (left offset of 50).
    return clickView(view, index, 50);
}

// Simulates a click on the nth point, in the point list view.
bool clickSelectPoint(QAbstractItemView *view, int region, int point)
{
    QStandardItemModel *model = static_cast<QStandardItemModel*>(view->model());
    const QModelIndex index = pointIndex(model, region, point);
    // Clicks near the middle of he box (left offset of 50).
    return clickView(view, index, 50);
}

#if 0
// Debugging printout. Prints the list of az/alt points in a region.
bool printAzAlt(QStandardItemModel *model, int region)
{
    if (model->rowCount() <= region)
        return false;

    const auto reg = model->item(region);
    const int numPoints = reg->rowCount();
    for (int i = 0; i < numPoints; ++i)
    {
        QStandardItem *azItem  = reg->child(i, 1);
        QStandardItem *altItem = reg->child(i, 2);

        const dms az  = dms::fromString(azItem->data(Qt::DisplayRole).toString(), true);
        const dms alt = dms::fromString(altItem->data(Qt::DisplayRole).toString(), true);

        fprintf(stderr, "az %f alt %f\n", az.Degrees(), alt.Degrees());
    }
    return true;
}
#endif

}  // namespace

// Returns the nth region.
QStandardItem *TestArtificialHorizon::getRegion(int region)
{
    return m_Model->item(region);
}


// Creates a list of SkyPoints corresponding to the points in the nth region.
QList<SkyPoint> TestArtificialHorizon::getRegionPoints(int region)
{
    const auto reg = getRegion(region);
    const int numPoints = reg->rowCount();
    QList<SkyPoint> pts;
    for (int i = 0; i < numPoints; ++i)
    {
        QStandardItem *azItem  = reg->child(i, 1);
        QStandardItem *altItem = reg->child(i, 2);
        const dms az  = dms::fromString(azItem->data(Qt::DisplayRole).toString(), true);
        const dms alt = dms::fromString(altItem->data(Qt::DisplayRole).toString(), true);
        SkyPoint p;
        p.setAz(az);
        p.setAlt(alt);
        pts.append(p);
    }
    return pts;
}

// Returns true if the SkyList contains the same az/alt points as the region.
bool TestArtificialHorizon::compareLivePreview(int region, SkyList *previewPoints)
{
    if (m_Model->rowCount() <= region)
        return false;
    QList<SkyPoint> regionPoints = getRegionPoints(region);
    if (previewPoints->size() != regionPoints.size())
        return false;
    for (int i = 0; i < regionPoints.size(); ++i)
    {
        if (previewPoints->at(i)->az().Degrees() != regionPoints[i].az().Degrees() ||
                previewPoints->at(i)->alt().Degrees() != regionPoints[i].alt().Degrees())
            return false;
    }
    return true;
}

// Checks for a testing bug where all az/alt points were repeated.
bool TestArtificialHorizon::checkForRepeatedAzAlt(int region)
{
    if (m_Model->rowCount() <= region)
        return false;
    const auto reg = getRegion(region);
    const int numPoints = reg->rowCount();
    double azKeep = 0, altKeep = 0;
    for (int i = 0; i < numPoints; ++i)
    {
        QStandardItem *azItem  = reg->child(i, 1);
        QStandardItem *altItem = reg->child(i, 2);

        const dms az  = dms::fromString(azItem->data(Qt::DisplayRole).toString(), true);
        const dms alt = dms::fromString(altItem->data(Qt::DisplayRole).toString(), true);

        if (i == 0)
        {
            azKeep = az.Degrees();
            altKeep = alt.Degrees();
        }
        else
        {
            if (az.Degrees() == azKeep || altKeep == alt.Degrees())
            {
                fprintf(stderr, "Repeated point in Region %d pt %d: %f %f\n", region, i, az.Degrees(), alt.Degrees());
                return false;
            }
        }
    }
    return true;
}

void TestArtificialHorizon::testArtificialHorizon()
{
    // Open the Artificial Horizon menu and instantiate the interface.
    KStars::Instance()->slotHorizonManager();
    SkyMap *skymap = KStars::Instance()->map();

    ArtificialHorizonComponent *horizonComponent = KStarsData::Instance()->skyComposite()->artificialHorizon();

    // Region buttons
    KTRY_AH_GADGET(QPushButton, addRegionB);
    KTRY_AH_GADGET(QPushButton, removeRegionB);
    KTRY_AH_GADGET(QPushButton, toggleCeilingB);
    KTRY_AH_GADGET(QPushButton, saveB);
    // Points buttons
    KTRY_AH_GADGET(QPushButton, addPointB);
    KTRY_AH_GADGET(QPushButton, removePointB);
    KTRY_AH_GADGET(QPushButton, clearPointsB);
    KTRY_AH_GADGET(QPushButton, selectPointsB);
    // Views
    KTRY_AH_GADGET(QTableView, pointsList);
    KTRY_AH_GADGET(QListView, regionsList);

    // This is the underlying data structure for the entire UI.
    m_Model = static_cast<QStandardItemModel*>(regionsList->model());

    // There shouldn't be any regions at the start.
    QVERIFY(regionsList->model()->rowCount() == 0);

    // There should be a live preview.
    QVERIFY(!horizonComponent->livePreview.get());

    // Add a region.
    KTRY_AH_CLICK(addRegionB);

    // There should now be  one region, it has no points, is checked, and named "Region 1".
    QVERIFY(regionsList->currentIndex().row() == 0);
    QVERIFY(getRegion(0)->rowCount() == 0);
    QVERIFY(getRegion(0)->checkState() == Qt::Checked);
    QVERIFY(m_Model->index(0, 0).data( Qt::DisplayRole ).toString() == QString("Region 1"));

    // Check we can toggle on and off "enable" with a mouse click.
    QVERIFY(clickEnableRegion(regionsList, 0));
    QVERIFY(getRegion(0)->checkState() == Qt::Unchecked);
    QVERIFY(clickEnableRegion(regionsList, 0));
    QVERIFY(getRegion(0)->checkState() == Qt::Checked);

    // Mouse-click entry of points shouldn't be enabled yet.
    QVERIFY(!selectPointsB->isChecked());

    // Enable mouse-click entry of points
    KTRY_AH_CLICK(selectPointsB);
    QVERIFY(selectPointsB->isChecked());

    // Add 5 points to the region by clicking on the skymap.
    skyClick(skymap, 200, 200);
    skyClick(skymap, 250, 250);
    skyClick(skymap, 300, 300);
    skyClick(skymap, 350, 350);
    skyClick(skymap, 400, 400);

    // Make sure there are 5 points now for region 0.
    QVERIFY(5 == getRegion(0)->rowCount());
    QVERIFY(checkForRepeatedAzAlt(0));

    // Turn this region into a ceiling, check it was noted, and turn that off.
    QVERIFY(!getRegion(0)->data(Qt::UserRole).toBool());
    KTRY_AH_CLICK(toggleCeilingB);
    QVERIFY(getRegion(0)->data(Qt::UserRole).toBool());
    KTRY_AH_CLICK(toggleCeilingB);

    // Add a 2nd region. This also turns of mouse-entry of points.
    KTRY_AH_CLICK(addRegionB);
    QVERIFY(!selectPointsB->isChecked());

    // The new region shouldn't have any points, but the first should still have 5.
    QVERIFY(5 == getRegion(0)->rowCount());
    QVERIFY(0 == getRegion(1)->rowCount());

    // Add 2 points to the 2nd region.
    KTRY_AH_CLICK(selectPointsB);
    skyClick(skymap, 400, 400);
    skyClick(skymap, 450, 450);
    QVERIFY(5 == getRegion(0)->rowCount());
    QVERIFY(2 == getRegion(1)->rowCount());
    QVERIFY(checkForRepeatedAzAlt(0));
    QVERIFY(checkForRepeatedAzAlt(1));

    // Make sure the live preview reflects the points in the 2nd region.
    QVERIFY(horizonComponent->livePreview.get());
    QVERIFY(compareLivePreview(1, horizonComponent->livePreview->points()));

    // The 2nd region should still be the active one.
    QVERIFY(1 == regionsList->currentIndex().row());

    // Select the first region, and make sure it becomes active.
    QVERIFY(clickSelectRegion(regionsList, 0));
    QVERIFY(0 == regionsList->currentIndex().row());

    // Keep these points for later comparison.
    QList<SkyPoint> pointsA = getRegionPoints(0);

    // Click on the skymap to add a new point to the first region.
    skyClick(skymap, 450, 450);
    // The 1st region should now have one more point.
    QVERIFY(6 == getRegion(0)->rowCount());
    QVERIFY(2 == getRegion(1)->rowCount());

    // The point should have been appended to the end of the 1st region.
    QList<SkyPoint> pointsB = getRegionPoints(0);
    QVERIFY(pointsA.size() + 1 == pointsB.size());
    for (int i = 0; i < pointsA.size(); i++)
        QVERIFY(pointsA[i] == pointsB[i]);

    QVERIFY(checkForRepeatedAzAlt(0));
    QVERIFY(checkForRepeatedAzAlt(1));

    // Make sure the live preview now reflects the points in the 1st region.
    QVERIFY(horizonComponent->livePreview.get());
    QVERIFY(compareLivePreview(0, horizonComponent->livePreview->points()));

    // Select the 3rd point in the 1st region
    QVERIFY(clickSelectPoint(pointsList, 0, 2));
    QVERIFY(2 == pointsList->currentIndex().row());

    // Copy the points for later comparison.
    pointsA = getRegionPoints(0);

    // Insert a point after the (just selected) 3rd point by clicking on the SkyMap.
    skyClick(skymap, 375, 330);
    QVERIFY(7 == getRegion(0)->rowCount());
    QVERIFY(2 == getRegion(1)->rowCount());

    // The new point should have been place in the middle.
    pointsB = getRegionPoints(0);
    QVERIFY(pointsA.size() + 1 == pointsB.size());
    for (int i = 0; i < 3; i++)
        QVERIFY(pointsA[i] == pointsB[i]);
    for (int i = 3; i < pointsA.size(); i++)
        QVERIFY(pointsA[i] == pointsB[i + 1]);

    QVERIFY(checkForRepeatedAzAlt(0));
    QVERIFY(checkForRepeatedAzAlt(1));

    // Select the 5th point in the 1st region and delete it.
    QVERIFY(clickSelectPoint(pointsList, 0, 4));
    QVERIFY(4 == pointsList->currentIndex().row());
    KTRY_AH_CLICK(removePointB);

    // There should now be one less point in the 1st region.
    QVERIFY(6 == getRegion(0)->rowCount());
    QVERIFY(2 == getRegion(1)->rowCount());

    QVERIFY(checkForRepeatedAzAlt(0));
    QVERIFY(checkForRepeatedAzAlt(1));

    // Clear all the points in the (currently selected) 1st region.
    KTRY_AH_CLICK(clearPointsB);
    QVERIFY(0 == getRegion(0)->rowCount());
    QVERIFY(2 == getRegion(1)->rowCount());

    // Remove the original 1st region. The 2nd region becomes the 1st one.
    pointsA = getRegionPoints(1);
    KTRY_AH_CLICK(removeRegionB);
    QVERIFY(2 == getRegion(0)->rowCount());
    pointsB = getRegionPoints(0);
    QVERIFY(pointsA == pointsB);

    QVERIFY(checkForRepeatedAzAlt(0));

    // Apply, then close the Artificial Horizon menu.

    // Equivalent to clicking the apply button.
    KStars::Instance()->m_HorizonManager->slotSaveChanges();
    // same as clicking X to close the window.
    KStars::Instance()->m_HorizonManager->close();

    // Should no longer have a live preview.
    QVERIFY(!horizonComponent->livePreview.get());

    // Re-open the menu.
    KStars::Instance()->slotHorizonManager();

    // The 2-point region should still be there.
    QVERIFY(regionsList->model()->rowCount() == 1);
    QVERIFY(2 == getRegion(0)->rowCount());

    // There should be a live preview again.
    QVERIFY(horizonComponent->livePreview.get());
    QVERIFY(compareLivePreview(0, horizonComponent->livePreview->points()));

    // This section tests to make sure that, when a region is enabled,
    // the horizon component's isVisible() method reflects the values
    // of the region. Will test using the 2 points of the saved region above.

    // Get the approximate azimuth and altitude at the midpoint of the 2-point region.
    pointsA = getRegionPoints(0);
    QVERIFY(2 == pointsA.size());
    const double az = (pointsA[0].az().Degrees() + pointsA[1].az().Degrees()) / 2.0;
    const double alt = (pointsA[0].alt().Degrees() + pointsA[1].alt().Degrees()) / 2.0;

    // Make sure the region is enabled.
    const auto state = getRegion(0)->checkState();
    if (state != Qt::Checked)
    {
        QVERIFY(clickEnableRegion(regionsList, 0));
        QVERIFY(getRegion(0)->checkState() == Qt::Checked);
    }

    // Same as clicking the apply button.
    KStars::Instance()->m_HorizonManager->slotSaveChanges();

    // Verify that isVisible() roughly reflects the region's limit.
    QVERIFY(horizonComponent->getHorizon().isVisible(az, alt + 5));
    QVERIFY(!horizonComponent->getHorizon().isVisible(az, alt - 5));

    // Turn the line into a ceiling line. The visibility should be reversed.
    KTRY_AH_CLICK(toggleCeilingB);
    KStars::Instance()->m_HorizonManager->slotSaveChanges();
    QVERIFY(!horizonComponent->getHorizon().isVisible(az, alt + 5));
    QVERIFY(horizonComponent->getHorizon().isVisible(az, alt - 5));
    // and turn ceiling off for that line, and the original visibility returns.
    KTRY_AH_CLICK(toggleCeilingB);
    KStars::Instance()->m_HorizonManager->slotSaveChanges();
    QVERIFY(horizonComponent->getHorizon().isVisible(az, alt + 5));
    QVERIFY(!horizonComponent->getHorizon().isVisible(az, alt - 5));

    // Now Disable the constraint.
    QVERIFY(clickEnableRegion(regionsList, 0));
    QVERIFY(getRegion(0)->checkState() == Qt::Unchecked);
    // Same as clicking the apply button.
    KStars::Instance()->m_HorizonManager->slotSaveChanges();
    // The constraint should be at -90 when not enabled.
    QVERIFY(horizonComponent->getHorizon().isVisible(az, -89));

    // Finally enable the region again, click apply, and exit the module.
    // The constraint should still be there, even with the menu closed.
    QVERIFY(clickEnableRegion(regionsList, 0));
    QVERIFY(getRegion(0)->checkState() == Qt::Checked);
    KStars::Instance()->m_HorizonManager->slotSaveChanges();
    KStars::Instance()->m_HorizonManager->close();
    QVERIFY(horizonComponent->getHorizon().isVisible(az, alt + 5));
    QVERIFY(!horizonComponent->getHorizon().isVisible(az, alt - 5));
}

QTEST_KSTARS_MAIN(TestArtificialHorizon)

#endif // HAVE_INDI
