/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "artificialhorizoncomponent.h"

#include "greatcircle.h"
#include "kstarsdata.h"
#include "linelist.h"
#include "Options.h"
#include "skymap.h"
#include "skymapcomposite.h"
#include "skypainter.h"
#include "projections/projector.h"

#define UNDEFINED_ALTITUDE -90

ArtificialHorizonEntity::~ArtificialHorizonEntity()
{
    clearList();
}

QString ArtificialHorizonEntity::region() const
{
    return m_Region;
}

void ArtificialHorizonEntity::setRegion(const QString &Region)
{
    m_Region = Region;
}

bool ArtificialHorizonEntity::enabled() const
{
    return m_Enabled;
}

void ArtificialHorizonEntity::setEnabled(bool Enabled)
{
    m_Enabled = Enabled;
}

bool ArtificialHorizonEntity::ceiling() const
{
    return m_Ceiling;
}

void ArtificialHorizonEntity::setCeiling(bool value)
{
    m_Ceiling = value;
}

void ArtificialHorizonEntity::setList(const std::shared_ptr<LineList> &list)
{
    m_List = list;
}

std::shared_ptr<LineList> ArtificialHorizonEntity::list() const
{
    return m_List;
}

void ArtificialHorizonEntity::clearList()
{
    m_List.reset();
}

namespace
{

// Returns true if angle is "in between" range1 and range2, two other angles,
// where in-between means "the short way".
bool inBetween(const dms &angle, const dms &range1, const dms &range2)
{
    const double rangeDelta = fabs(range1.deltaAngle(range2).Degrees());
    const double delta1 = fabs(range1.deltaAngle(angle).Degrees());
    const double delta2 = fabs(range2.deltaAngle(angle).Degrees());
    // The angle is between range1 and range2 if its two distances to each are both
    // less than the range distance.
    return delta1 <= rangeDelta && delta2 <= rangeDelta;
}
}  // namespace

double ArtificialHorizonEntity::altitudeConstraint(double azimuthDegrees, bool *constraintExists) const
{
    *constraintExists = false;
    if (m_List == nullptr)
        return UNDEFINED_ALTITUDE;

    SkyList *points = m_List->points();
    if (points == nullptr)
        return UNDEFINED_ALTITUDE;

    double constraint = !m_Ceiling ? UNDEFINED_ALTITUDE : 90.0;
    dms desiredAzimuth(azimuthDegrees);
    dms lastAz;
    double lastAlt = 0;
    bool firstOne = true;
    for (auto &p : *points)
    {
        const dms az = p->az();

        // There can be issues with GreatCircle below if altitudes are at the zenith.
        const double alt = std::min(89.999, p->alt().Degrees());

        if (qIsNaN(az.Degrees()) || qIsNaN(alt)) continue;
        if (!firstOne && inBetween(desiredAzimuth, lastAz, az))
        {
            *constraintExists = true;
            // If the input angle is in the interval between the last two points,
            // interpolate the altitude constraint, and use that value.
            // If there are other line segments which also contain the point,
            // we use the max constraint.
            const double totalDelta = fabs(lastAz.deltaAngle(az).Degrees());
            if (totalDelta <= 0)
            {
                if (!m_Ceiling)
                    constraint = std::max(constraint, alt);
                else
                    constraint = std::min(constraint, alt);
            }
            else
            {
                GreatCircle gc(lastAz.Degrees(), lastAlt, az.Degrees(), alt);
                const double newConstraint = gc.altAtAz(azimuthDegrees);
                if (!m_Ceiling)
                    constraint = std::max(constraint, newConstraint);
                else
                    constraint = std::min(constraint, newConstraint);
            }
        }
        firstOne = false;
        lastAz = az;
        lastAlt = alt;
    }
    return constraint;
}

ArtificialHorizonComponent::ArtificialHorizonComponent(SkyComposite *parent)
    : NoPrecessIndex(parent, i18n("Artificial Horizon"))
{
    load();
}

ArtificialHorizonComponent::~ArtificialHorizonComponent()
{
}

ArtificialHorizon::~ArtificialHorizon()
{
    qDeleteAll(m_HorizonList);
    m_HorizonList.clear();
}

void ArtificialHorizon::load(const QList<ArtificialHorizonEntity *> &list)
{
    m_HorizonList = list;
    resetPrecomputeConstraints();
    checkForCeilings();
}

bool ArtificialHorizonComponent::load()
{
    QList<ArtificialHorizonEntity *> list;
    KStarsData::Instance()->userdb()->GetAllHorizons(list);
    horizon.load(list);

    foreach (ArtificialHorizonEntity *horizon, *horizon.horizonList())
        appendLine(horizon->list());

    return true;
}

void ArtificialHorizonComponent::save()
{
    KStarsData::Instance()->userdb()->DeleteAllHorizons();

    foreach (ArtificialHorizonEntity *horizon, *horizon.horizonList())
        KStarsData::Instance()->userdb()->AddHorizon(horizon);
}

bool ArtificialHorizonComponent::selected()
{
    return true;
}

void ArtificialHorizonComponent::preDraw(SkyPainter *skyp)
{
    QColor color(KStarsData::Instance()->colorScheme()->colorNamed("ArtificialHorizonColor"));
    color.setAlpha(40);
    skyp->setBrush(QBrush(color));
    skyp->setPen(Qt::NoPen);
}

namespace
{

// Returns an equivalent degrees in the range 0 <= 0 < 360
double normalizeDegrees(double degrees)
{
    while (degrees < 0)
        degrees += 360;
    while (degrees >= 360.0)
        degrees -= 360.0;
    return degrees;
}

// Draws a "round polygon", sampling a circle every 45 degrees, with the given radius,
// centered on the SkyPoint.
void drawHorizonPoint(const SkyPoint &pt, double radius, SkyPainter *painter)

{
    LineList region;
    double az = pt.az().Degrees(), alt = pt.alt().Degrees();

    for (double angle = 0; angle < 360; angle += 45)
    {
        double radians = angle * 2 * M_PI / 360.0;
        double az1 = az + radius * cos(radians);
        double alt1 = alt + radius * sin(radians);
        std::shared_ptr<SkyPoint> sp(new SkyPoint());
        sp->setAz(az1);
        sp->setAlt(alt1);
        sp->HorizontalToEquatorial(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
        region.append(sp);
    }
    // Repeat the first point.
    double az1 = az + radius * cos(0);
    double alt1 = alt + radius * sin(0);
    std::shared_ptr<SkyPoint> sp(new SkyPoint());
    sp->setAz(az1);
    sp->setAlt(alt1);
    sp->HorizontalToEquatorial(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
    region.append(sp);

    painter->drawSkyPolygon(&region, false);
}

// Draws a series of points whose coordinates are given by the LineList.
void drawHorizonPoints(LineList *lineList, SkyPainter *painter)
{
    const SkyList &points = *(lineList->points());
    for (int i = 0; i < points.size(); ++i)
    {
        const SkyPoint &pt = *points[i];
        if (qIsNaN(pt.az().Degrees()) || qIsNaN(pt.alt().Degrees()))
            continue;
        drawHorizonPoint(pt, .5, painter);
    }
}

// Draws a points that is larger than the one drawn by drawHorizonPoint().
// The point's coordinates are the ith (index) point in the LineList.
void drawSelectedPoint(LineList *lineList, int index, SkyPainter *painter)
{
    if (index >= 0 && index < lineList->points()->size())
    {
        const SkyList &points = *(lineList->points());
        const SkyPoint &pt = *points[index];
        if (qIsNaN(pt.az().Degrees()) || qIsNaN(pt.alt().Degrees()))
            return;
        drawHorizonPoint(pt, 1.0, painter);
    }
}

// This creates a set of connected line segments from az1,alt1 to az2,alt2, sampling
// points on the great circle between az1,alt1 and az2,alt2 every 2 degrees or so.
// The errors would be obvious for longer lines if we just drew a standard line.
// If testing is true, HorizontalToEquatorial is not called.
void appendGreatCirclePoints(double az1, double alt1, double az2, double alt2, LineList *region, bool testing)
{
    constexpr double sampling = 2.0;  // degrees
    const double maxAngleDiff = std::max(fabs(az1 - az2), fabs(alt1 - alt2));
    const int numSamples = maxAngleDiff / sampling;

    // Hy 9/25/22: These 4 lines cause rendering issues in equatorial mode (horizon mode is ok).
    // Not sure why--though I suspect the initial conditions computed in drawPolygons().
    // Without them there are some jagged lines near the horizon, but much better than with them.
    // std::shared_ptr<SkyPoint> sp0(new SkyPoint());
    // sp0->setAz(az1);
    // sp0->setAlt(alt1);
    // region->append(sp0);

    if (numSamples > 1)
    {
        GreatCircle gc(az1, alt1, az2, alt2);
        for (int i = 1; i < numSamples; ++i)
        {
            const double fraction = i / static_cast<double>(numSamples);
            double az, alt;
            gc.waypoint(fraction, &az, &alt);
            std::shared_ptr<SkyPoint> sp(new SkyPoint());
            sp->setAz(az);
            sp->setAlt(alt);
            if (!testing)
                sp->HorizontalToEquatorial(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
            region->append(sp);
        }
    }
    std::shared_ptr<SkyPoint> sp(new SkyPoint());
    sp->setAz(az2);
    sp->setAlt(alt2);
    // Is HorizontalToEquatorial necessary in any case?
    if (!testing)
        sp->HorizontalToEquatorial(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
    region->append(sp);
}

}  // namespace

// Draws a polygon, where one of the sides is az1,alt1 --> az2,alt2 (except that's implemented as series
// of connected line segments along a great circle).
// It figures out the opposite side depending on the type of the constraint for this entity
// (horizon line or ceiling) and the other contraints that are enabled.
bool ArtificialHorizon::computePolygon(int entity, double az1, double alt1, double az2, double alt2,
                                       double sampling, LineList *region)
{
    // There can be issues with GreatCircle below if both altitudes are at the zenith.
    if (alt1 >= 90 && alt2 >= 90)
    {
        alt1 = 89.99;
        alt2 = 89.99;
    }
    const bool ceiling = horizonList()->at(entity)->ceiling();
    const ArtificialHorizonEntity *thisOne = horizonList()->at(entity);
    double alt1b = 0, alt2b = 0;
    bool exists = false;
    LineList left, top, right, bottom;

    double lastAz = az1;
    double lastAlt = alt1;

    if (az1 >= az2)
        return false;

    if (az1 + sampling > az2)
        sampling = (az2 - az1) - 1e-6;

    GreatCircle gc(az1, alt1, az2, alt2);
    double numSamples = (az2 - az1) / sampling;
    for (int i = 0; i < numSamples; ++i)
    {
        double fraction = i / numSamples;
        if (fraction + (1.0 / numSamples) > (1 + .0001))
            fraction = 1.0;

        double az, alt;
        gc.waypoint(fraction, &az, &alt);
        double alt1b = 0, alt2b = 0;

        if (!ceiling)
        {
            // For standard horizon lines, the polygon is drawn down to the next lower-altitude
            // enabled line, or to the horizon if a lower line doesn't exist.
            const ArtificialHorizonEntity *constraint = getConstraintBelow(lastAz, lastAlt, thisOne);
            if (constraint != nullptr)
            {
                double altTemp = constraint->altitudeConstraint(lastAz, &exists);
                if (exists)
                    alt1b = altTemp;
            }
            constraint = getConstraintBelow(az, alt, thisOne);
            if (constraint != nullptr)
            {
                double altTemp = constraint->altitudeConstraint(az, &exists);
                if (exists)
                    alt2b = altTemp;
            }
            appendGreatCirclePoints(lastAz, lastAlt,   az, alt, &top, testing);
            appendGreatCirclePoints(lastAz, alt1b,   az, alt2b, &bottom, testing);
        }
        else
        {
            // For ceiling lines, the polygon is drawn up to the next higher-altitude enabled line
            // but only if that line is another cieling, otherwise it not drawn at all (because that
            // horizon line will do the drawing).
            const ArtificialHorizonEntity *constraint = getConstraintAbove(lastAz, lastAlt, thisOne);
            alt1b = 90;
            alt2b = 90;
            if (constraint != nullptr)
            {
                if (constraint->ceiling()) return false;
                double altTemp = constraint->altitudeConstraint(lastAz, &exists);
                if (exists) alt1b = altTemp;
            }
            constraint = getConstraintAbove(az, alt, thisOne);
            if (constraint != nullptr)
            {
                if (constraint->ceiling()) return false;
                double altTemp = constraint->altitudeConstraint(az, &exists);
                if (exists) alt2b = altTemp;
            }
            appendGreatCirclePoints(lastAz, lastAlt,   az, alt, &top, testing);
            // Note that "bottom" for a ceiling is above.
            appendGreatCirclePoints(lastAz, alt1b,   az, alt2b, &bottom, testing);
        }
        lastAz = az;
        lastAlt = alt;
    }

    if (!ceiling)
    {
        // For standard horizon lines, the polygon is drawn down to the next lower-altitude
        // enabled line, or to the horizon if a lower line doesn't exist.
        const ArtificialHorizonEntity *constraint = getConstraintBelow(az1, alt1, thisOne);
        if (constraint != nullptr)
        {
            double altTemp = constraint->altitudeConstraint(az1, &exists);
            if (exists)
                alt1b = altTemp;
        }
        appendGreatCirclePoints(az1, alt1b,   az1, alt1, &left, testing);

        const ArtificialHorizonEntity *constraint2 = getConstraintBelow(az2, alt2, thisOne);
        if (constraint2 != nullptr)
        {
            double altTemp = constraint2->altitudeConstraint(az2, &exists);
            if (exists)
                alt2b = altTemp;
        }
        appendGreatCirclePoints(az2, alt2,   az2, alt2b, &right, testing);
    }
    else
    {
        // For ceiling lines, the polygon is drawn up to the next higher-altitude enabled line
        // but only if that line is another cieling, otherwise it not drawn at all (because that
        // horizon line will do the drawing).
        const ArtificialHorizonEntity *constraint = getConstraintAbove(az1, alt1, thisOne);
        alt1b = 90;
        alt2b = 90;
        if (constraint != nullptr)
        {
            if (!constraint->ceiling()) return false;
            double altTemp = constraint->altitudeConstraint(az1, &exists);
            if (exists) alt1b = altTemp;
        }
        appendGreatCirclePoints(az1, alt1b,   az1, alt1, &left, testing);

        const ArtificialHorizonEntity *constraint2 = getConstraintAbove(az2, alt2, thisOne);
        if (constraint2 != nullptr)
        {
            if (!constraint2->ceiling()) return false;
            double altTemp = constraint2->altitudeConstraint(az2, &exists);
            if (exists) alt2b = altTemp;
        }
        appendGreatCirclePoints(az2, alt2,   az2, alt2b, &right, testing);
    }

    // Now we have all the sides: left, top, right, bottom, but the order of bottom is reversed.
    // Make a polygon with all the points.
    for (const auto &p : * (left.points()))
        region->append(p);
    for (const auto &p : * (top.points()))
        region->append(p);
    for (const auto &p : * (right.points()))
        region->append(p);
    for (int i = bottom.points()->size() - 1; i >= 0; i--)
        region->append(bottom.points()->at(i));

    return true;
}

// Draws a series of polygons of width in azimuth of "sampling degrees".
// Drawing a single polygon would have "great-circle issues". This looks a lot better.
// Assumes az1 and az2 in range 0-360 and az1 < az2.
// regions is only not nullptr during testing. In this wasy we can test
// whether the appropriate regions are drawn.
void ArtificialHorizon::drawSampledPolygons(int entity, double az1, double alt1, double az2, double alt2,
        double sampling, SkyPainter *painter, QList<LineList> *regions)
{
    if (az1 > az2)
    {
        // Should not generally happen. Possibility e.g. az 0 -> 0.01 in a wrap around.
        // OK to ignore.
        return;
    }

    LineList region;
    if (computePolygon(entity, az1, alt1, az2, alt2, sampling, &region))
    {
        if (painter != nullptr)
            painter->drawSkyPolygon(&region, false);
        if (regions != nullptr)
            regions->append(region);
    }
}

// This draws a series of polygons that fill the area that the horizon entity with index "entity"
// is responsible for.  If that is a horizon line, it draws it down to the horizon, or to the next
// lower line. It draws the polygons one pair of points at a time, and deals with complications
// of when the azimuth angle wraps around 360 degrees.
void ArtificialHorizon::drawPolygons(int entity, SkyPainter *painter, QList<LineList> *regions)
{
    const ArtificialHorizonEntity &ah = *(horizonList()->at(entity));
    const SkyList &points = *(ah.list()->points());

    // The skylist shouldn't contain NaN values, but, it has in the past,
    // and, to be cautious, this checks for them and removes points with NaNs.
    int start = 0;
    for (; start < points.size(); ++start)
    {
        const SkyPoint &p = *points[start];
        if (!qIsNaN(p.az().Degrees()) && !qIsNaN(p.alt().Degrees()))
            break;
    }

    for (int i = start + 1; i < points.size(); ++i)
    {
        const SkyPoint &p2 = *points[i];
        if (qIsNaN(p2.az().Degrees()) || qIsNaN(p2.alt().Degrees()))
            continue;
        const SkyPoint &p1 = *points[start];
        start = i;

        const double az1 = normalizeDegrees(p1.az().Degrees());
        const double az2 = normalizeDegrees(p2.az().Degrees());

        double minAz, maxAz, minAzAlt, maxAzAlt;
        if (az1 < az2)
        {
            minAz = az1;
            minAzAlt = p1.alt().Degrees();
            maxAz = az2;
            maxAzAlt = p2.alt().Degrees();
        }
        else
        {
            minAz = az2;
            minAzAlt = p2.alt().Degrees();
            maxAz = az1;
            maxAzAlt = p1.alt().Degrees();
        }
        const bool wrapAround = !inBetween(dms((minAz + maxAz) / 2.0), dms(minAz), dms(maxAz));
        constexpr double sampling = 1.0;  // Draw a polygon for every degree in Azimuth
        if (wrapAround)
        {
            // We've detected that the line segment crosses 0 degrees.
            // Draw one polygon on one side of 0 degrees, and another on the other side.
            // Compute the altitude at wrap-around.
            GreatCircle gc(maxAz, maxAzAlt, minAz, minAzAlt);
            const double midAlt = gc.altAtAz(0);
            // Draw polygons form maxAz upto 0 degrees, then again from 0 to minAz.
            drawSampledPolygons(entity, maxAz, maxAzAlt, 360, midAlt, sampling, painter, regions);
            drawSampledPolygons(entity, 0, midAlt, minAz, minAzAlt, sampling, painter, regions);
        }
        else
        {
            // Draw the polygons without wraparound
            drawSampledPolygons(entity, minAz, minAzAlt, maxAz, maxAzAlt, sampling, painter, regions);
        }
    }
}

void ArtificialHorizon::drawPolygons(SkyPainter *painter, QList<LineList> *regions)
{
    for (int i = 0; i < horizonList()->size(); i++)
    {
        if (enabled(i))
            drawPolygons(i, painter, regions);
    }
}

// This samples the lines in the input list by 0.1 degrees.
// In this way they will be drawn to approximate a great-circle curve.
void sampleLineList(std::shared_ptr<LineList> *list, std::shared_ptr<LineList> *tempPoints)
{
    constexpr double sampling = 0.1; // degrees
    const auto points = list->get()->points();
    const int size = points->size();
    (*tempPoints)->points()->clear();
    for (int upto = 0; upto < size - 1; ++upto)
    {
        const auto p1 = points->at(upto);
        const auto p2 = points->at(upto + 1);
        GreatCircle gc(p1->az().Degrees(), std::min(89.999, p1->alt().Degrees()), p2->az().Degrees(), std::min(89.999,
                       p2->alt().Degrees()));
        const double maxDelta = std::max(fabs(p2->az().Degrees() - p1->az().Degrees()),
                                         fabs(p2->alt().Degrees() - p1->alt().Degrees()));
        if (maxDelta == 0) continue;
        int numP = maxDelta / sampling;
        if (numP == 0) numP = 2;
        for (int i = 0; i < numP; ++i)
        {
            double newAz = 0, newAlt = 0;
            gc.waypoint(i * 1.0 / numP, &newAz, &newAlt);
            SkyPoint *newPt = new SkyPoint;
            newPt->setAz(newAz);
            newPt->setAlt(newAlt);
            newPt->HorizontalToEquatorial(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
            (*tempPoints)->append(std::shared_ptr<SkyPoint>(newPt));
        }
        // Do the last point.
        if (upto == (size - 2))
        {
            SkyPoint *newPt = new SkyPoint;
            newPt->setAz(p2->az().Degrees());
            newPt->setAlt(p2->alt().Degrees());
            newPt->HorizontalToEquatorial(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
            (*tempPoints)->append(std::shared_ptr<SkyPoint>(newPt));
        }
    }
}

void ArtificialHorizonComponent::draw(SkyPainter *skyp)
{
    if (!selected())
        return;

    bool showPolygons = Options::showGround();
    if (livePreview.get())
    {
        if ((livePreview->points() != nullptr) && (livePreview->points()->size() > 0))
        {
            // Draws a series of line segments, overlayed by the vertices.
            // One vertex (the current selection) is emphasized.
            skyp->setPen(QPen(Qt::white, 2));

            // Sample the points so that the line renders as an approximation to a great-circle curve.
            auto tempLineList = std::shared_ptr<LineList>(new LineList);
            *tempLineList = *livePreview;
            sampleLineList(&livePreview, &tempLineList);
            skyp->drawSkyPolyline(tempLineList.get());
            skyp->setBrush(QBrush(Qt::yellow));
            drawSelectedPoint(livePreview.get(), selectedPreviewPoint, skyp);
            skyp->setBrush(QBrush(Qt::red));
            drawHorizonPoints(livePreview.get(), skyp);
            showPolygons = true;
        }
    }

    if (showPolygons)
    {
        preDraw(skyp);
        QList<LineList> regions;
        horizon.drawPolygons(skyp, &regions);
    }
}

bool ArtificialHorizon::enabled(int i) const
{
    return m_HorizonList.at(i)->enabled();
}

ArtificialHorizonEntity *ArtificialHorizon::findRegion(const QString &regionName)
{
    ArtificialHorizonEntity *regionHorizon = nullptr;

    foreach (ArtificialHorizonEntity *horizon, m_HorizonList)
    {
        if (horizon->region() == regionName)
        {
            regionHorizon = horizon;
            break;
        }
    }

    return regionHorizon;
}

void ArtificialHorizon::removeRegion(const QString &regionName, bool lineOnly)
{
    ArtificialHorizonEntity *regionHorizon = findRegion(regionName);

    if (regionHorizon == nullptr)
        return;

    if (lineOnly)
        regionHorizon->clearList();
    else
    {
        m_HorizonList.removeOne(regionHorizon);
        delete (regionHorizon);
    }
    resetPrecomputeConstraints();
    checkForCeilings();
}

void ArtificialHorizonComponent::removeRegion(const QString &regionName, bool lineOnly)
{
    ArtificialHorizonEntity *regionHorizon = horizon.findRegion(regionName);
    if (regionHorizon != nullptr && regionHorizon->list())
        removeLine(regionHorizon->list());
    horizon.removeRegion(regionName, lineOnly);
}

void ArtificialHorizon::checkForCeilings()
{
    noCeilingConstraints = true;
    for (const auto &r : m_HorizonList)
    {
        if (r->ceiling() && r->enabled())
        {
            noCeilingConstraints = false;
            break;
        }
    }
}

void ArtificialHorizon::addRegion(const QString &regionName, bool enabled, const std::shared_ptr<LineList> &list,
                                  bool ceiling)
{
    ArtificialHorizonEntity *horizon = new ArtificialHorizonEntity;

    horizon->setRegion(regionName);
    horizon->setEnabled(enabled);
    horizon->setCeiling(ceiling);
    horizon->setList(list);

    m_HorizonList.append(horizon);
    resetPrecomputeConstraints();
    checkForCeilings();
}

void ArtificialHorizonComponent::addRegion(const QString &regionName, bool enabled, const std::shared_ptr<LineList> &list,
        bool ceiling)
{
    horizon.addRegion(regionName, enabled, list, ceiling);
    appendLine(list);
}

bool ArtificialHorizon::altitudeConstraintsExist() const
{
    foreach (ArtificialHorizonEntity *horizon, m_HorizonList)
    {
        if (horizon->enabled())
            return true;
    }
    return false;
}

const ArtificialHorizonEntity *ArtificialHorizon::getConstraintAbove(double azimuthDegrees, double altitudeDegrees,
        const ArtificialHorizonEntity *ignore) const
{
    double closestAbove = 1e6;
    const ArtificialHorizonEntity *entity = nullptr;

    foreach (ArtificialHorizonEntity *horizon, m_HorizonList)
    {
        if (!horizon->enabled()) continue;
        if (horizon == ignore) continue;
        bool constraintExists = false;
        double constraint = horizon->altitudeConstraint(azimuthDegrees, &constraintExists);
        // This horizon doesn't constrain this azimuth.
        if (!constraintExists) continue;

        double altitudeDiff = constraint - altitudeDegrees;
        if (altitudeDiff > 0 && constraint < closestAbove)
        {
            closestAbove = constraint;
            entity = horizon;
        }
    }
    return entity;
}

// Estimate the horizon contraint to .1 degrees.
// This significantly speeds up computation.
constexpr int PRECOMPUTED_RESOLUTION = 10;

double ArtificialHorizon::altitudeConstraint(double azimuthDegrees) const
{
    if (precomputedConstraints.size() != 360 * PRECOMPUTED_RESOLUTION)
        precomputeConstraints();
    return precomputedConstraint(azimuthDegrees);
}

double ArtificialHorizon::altitudeConstraintInternal(double azimuthDegrees) const
{
    const ArtificialHorizonEntity *horizonBelow = getConstraintBelow(azimuthDegrees, 90.0, nullptr);
    if (horizonBelow == nullptr)
        return UNDEFINED_ALTITUDE;
    bool ignore = false;
    return horizonBelow->altitudeConstraint(azimuthDegrees, &ignore);
}

// Quantize the constraints to within .1 degrees (so there are 360*10=3600
// precomputed values).
void ArtificialHorizon::precomputeConstraints() const
{
    precomputedConstraints.clear();
    precomputedConstraints.fill(0, 360 * PRECOMPUTED_RESOLUTION);
    for (int i = 0; i < 360 * PRECOMPUTED_RESOLUTION; ++i)
    {
        const double az = i / static_cast<double>(PRECOMPUTED_RESOLUTION);
        precomputedConstraints[i] = altitudeConstraintInternal(az);
    }
}

void ArtificialHorizon::resetPrecomputeConstraints() const
{
    precomputedConstraints.clear();
}

double ArtificialHorizon::precomputedConstraint(double azimuth) const
{
    constexpr int maxval = 360 * PRECOMPUTED_RESOLUTION;
    int index = azimuth * PRECOMPUTED_RESOLUTION + 0.5;
    if (index == maxval)
        index = 0;
    if (index < 0 || index >= precomputedConstraints.size())
        return UNDEFINED_ALTITUDE;
    return precomputedConstraints[index];
}

const ArtificialHorizonEntity *ArtificialHorizon::getConstraintBelow(double azimuthDegrees, double altitudeDegrees,
        const ArtificialHorizonEntity *ignore) const
{
    double closestBelow = -1e6;
    const ArtificialHorizonEntity *entity = nullptr;

    foreach (ArtificialHorizonEntity *horizon, m_HorizonList)
    {
        if (!horizon->enabled()) continue;
        if (horizon == ignore) continue;
        bool constraintExists = false;
        double constraint = horizon->altitudeConstraint(azimuthDegrees, &constraintExists);
        // This horizon doesn't constrain this azimuth.
        if (!constraintExists) continue;

        double altitudeDiff = constraint - altitudeDegrees;
        if (altitudeDiff < 0 && constraint > closestBelow)
        {
            closestBelow = constraint;
            entity = horizon;
        }
    }
    return entity;
}

bool ArtificialHorizon::isAltitudeOK(double azimuthDegrees, double altitudeDegrees, QString *reason,
                                     double *margin) const
{
    if (noCeilingConstraints)
    {
        const double constraint = altitudeConstraint(azimuthDegrees);
        const double diff = altitudeDegrees - constraint;
        if (margin)
            *margin = fabs(diff);
        if (diff >= 0)
            return true;
        if (reason != nullptr)
            *reason = QString("altitude %1 < horizon %2").arg(altitudeDegrees, 0, 'f', 1).arg(constraint, 0, 'f', 1);
        return false;
    }
    return isVisible(azimuthDegrees, altitudeDegrees, reason, margin);
}

// An altitude is blocked (not visible) if either:
// - there are constraints above and the closest above constraint is not a ceiling, or
// - there are constraints below and the closest below constraint is a ceiling.
bool ArtificialHorizon::isVisible(double azimuthDegrees, double altitudeDegrees, QString *reason, double *margin) const
{
    if (margin)
        *margin = 90;
    const ArtificialHorizonEntity *above = getConstraintAbove(azimuthDegrees, altitudeDegrees);
    bool ignoreMe;
    if (above != nullptr && !above->ceiling())
    {
        if (reason != nullptr || margin != nullptr)
        {
            double constraint = above->altitudeConstraint(azimuthDegrees, &ignoreMe);
            if (reason)
                *reason = QString("altitude %1 < horizon %2").arg(altitudeDegrees, 0, 'f', 1).arg(constraint, 0, 'f', 1);
            if (margin)
                *margin = fabs(altitudeDegrees - constraint);
        }
        return false;
    }
    const ArtificialHorizonEntity *below = getConstraintBelow(azimuthDegrees, altitudeDegrees);
    if (below != nullptr && below->ceiling())
    {
        if (reason != nullptr || margin != nullptr)
        {
            double constraint = below->altitudeConstraint(azimuthDegrees, &ignoreMe);
            if (reason)
                *reason = QString("altitude %1 > ceiling %2").arg(altitudeDegrees, 0, 'f', 1).arg(constraint, 0, 'f', 1);
            if (margin)
                *margin = fabs(altitudeDegrees - constraint);
        }
        return false;
    }
    if (margin)
    {
        // we're ok on one or both margins, but how close?
        if (below && !below->ceiling())
            *margin = fabs(altitudeDegrees - below->altitudeConstraint(azimuthDegrees, &ignoreMe));
        if (above && above->ceiling())
            *margin = std::min(*margin, fabs(altitudeDegrees - above->altitudeConstraint(azimuthDegrees, &ignoreMe)));
    }
    return true;
}
