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
        const double alt = p->alt().Degrees();
        if (qIsNaN(az.Degrees()) || qIsNaN(alt)) continue;
        if (!firstOne && inBetween(desiredAzimuth, lastAz, az))
        {
            *constraintExists = true;
            // If the input angle is in the interval between the last two points,
            // interpolate the altitude constraint, and use that value.
            // If there are other line segments which also contain the point,
            // we use the max constraint. Convert to GreatCircle?
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
                const double deltaToLast = fabs(lastAz.deltaAngle(desiredAzimuth).Degrees());
                const double weight = deltaToLast / totalDelta;
                const double newConstraint = (1.0 - weight) * lastAlt + weight * alt;
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
}

bool ArtificialHorizonComponent::load()
{
    horizon.load(KStarsData::Instance()->userdb()->GetAllHorizons());

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
    return Options::showGround();
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
                                       LineList *region)
{
    const bool ceiling = horizonList()->at(entity)->ceiling();
    const ArtificialHorizonEntity *thisOne = horizonList()->at(entity);
    double alt1b = 0, alt2b = 0;
    bool exists = false;
    if (!ceiling)
    {
        // For standard horizon lines, the polygon is drawn down to the next lower-altitude
        // enabled line, or to the horizon if a lower line doesn't exist.
        const ArtificialHorizonEntity *constraint = getConstraintBelow(az1, alt1, thisOne);
        if (constraint != nullptr)
        {
            double alt = constraint->altitudeConstraint(az1, &exists);
            if (exists)
                alt1b = alt;
        }
        constraint = getConstraintBelow(az2, alt2, thisOne);
        if (constraint != nullptr)
        {
            double alt = constraint->altitudeConstraint(az2, &exists);
            if (exists)
                alt2b = alt;
        }
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
            double alt = constraint->altitudeConstraint(az1, &exists);
            if (exists) alt1b = alt;
        }
        constraint = getConstraintAbove(az2, alt2, thisOne);
        if (constraint != nullptr)
        {
            if (!constraint->ceiling()) return false;
            double alt = constraint->altitudeConstraint(az2, &exists);
            if (exists) alt2b = alt;
        }
    }

    std::shared_ptr<SkyPoint> sp(new SkyPoint());
    sp->setAz(az1);
    sp->setAlt(alt1b);
    if (!testing)
        sp->HorizontalToEquatorial(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
    region->append(sp);

    appendGreatCirclePoints(az1, alt1b,  az1, alt1, region, testing);
    appendGreatCirclePoints(az1, alt1,   az2, alt2, region, testing);
    appendGreatCirclePoints(az2, alt2,   az2, alt2b, region, testing);
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
        // Should not happen.
        fprintf(stderr, "Bad input to artificialhorizoncomponent.cpp::DrawSampledPolygons\n");
        return;
    }
    double lastAz = az1;
    double lastAlt = alt1;
    const double azRange = az2 - az1, altRange = alt2 - alt1;
    if (azRange == 0) return;
    for (double az = az1 + sampling; az < az2; az += sampling)
    {
        double alt = alt1 + altRange * (az - az1) / azRange;

        LineList region;
        if (computePolygon(entity, lastAz, lastAlt, az, alt, &region))
        {
            if (painter != nullptr)
                painter->drawSkyPolygon(&region, false);
            if (regions != nullptr)
                regions->append(region);
        }
        lastAz = az;
        lastAlt = alt;
    }
    LineList region;
    if (computePolygon(entity, lastAz, lastAlt, az2, alt2, &region))
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
        constexpr double sampling = 0.1;  // Draw a polygon for every degree in Azimuth
        if (wrapAround)
        {
            // We've detected that the line segment crosses 0 degrees.
            // Draw one polygon on one side of 0 degrees, and another on the other side.
            // Compute the altitude at wrap-around.
            const double fraction = fabs(dms(360.0).deltaAngle(dms(maxAz)).Degrees() /
                                         p1.az().deltaAngle(p2.az()).Degrees());
            const double midAlt = minAzAlt + fraction * (maxAzAlt - minAzAlt);
            // Draw polygons form maxAz upto 0 degrees, then again from 0 to minAz.
            drawSampledPolygons(entity, maxAz, maxAzAlt, 360.0, midAlt, sampling, painter, regions);
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

void ArtificialHorizonComponent::draw(SkyPainter *skyp)
{
    if (!selected())
        return;

    if (livePreview.get())
    {
        if ((livePreview->points() != nullptr) && (livePreview->points()->size() > 0))
        {
            // Draws a series of line segments, overlayed by the vertices.
            // One vertex (the current selection) is emphasized.
            skyp->setPen(QPen(Qt::white, 2));
            skyp->drawSkyPolyline(livePreview.get());
            skyp->setBrush(QBrush(Qt::yellow));
            drawSelectedPoint(livePreview.get(), selectedPreviewPoint, skyp);
            skyp->setBrush(QBrush(Qt::red));
            drawHorizonPoints(livePreview.get(), skyp);
        }
    }

    preDraw(skyp);

    QList<LineList> regions;
    horizon.drawPolygons(skyp, &regions);
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
}

void ArtificialHorizonComponent::removeRegion(const QString &regionName, bool lineOnly)
{
    ArtificialHorizonEntity *regionHorizon = horizon.findRegion(regionName);
    if (regionHorizon != nullptr && regionHorizon->list())
        removeLine(regionHorizon->list());
    horizon.removeRegion(regionName, lineOnly);
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

double ArtificialHorizon::altitudeConstraint(double azimuthDegrees) const
{
    const ArtificialHorizonEntity *horizonBelow = getConstraintBelow(azimuthDegrees, 90.0, nullptr);
    if (horizonBelow == nullptr)
        return UNDEFINED_ALTITUDE;
    bool ignore = false;
    return horizonBelow->altitudeConstraint(azimuthDegrees, &ignore);
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

// An altitude is blocked (not visible) if either:
// - there are constraints above and the closest above constraint is not a ceiling, or
// - there are constraints below and the closest below constraint is a ceiling.
bool ArtificialHorizon::isVisible(double azimuthDegrees, double altitudeDegrees) const
{
    const ArtificialHorizonEntity *above = getConstraintAbove(azimuthDegrees, altitudeDegrees);
    if (above != nullptr && !above->ceiling()) return false;
    const ArtificialHorizonEntity *below = getConstraintBelow(azimuthDegrees, altitudeDegrees);
    if (below != nullptr && below->ceiling()) return false;
    return true;
}
