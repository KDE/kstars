/*  Artificial Horizon
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "artificialhorizoncomponent.h"

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

void ArtificialHorizonEntity::setList(const std::shared_ptr<LineList> &list)
{
    m_List = list;
}

std::shared_ptr<LineList> ArtificialHorizonEntity::list()
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

double ArtificialHorizonEntity::altitudeConstraint(double azimuthDegrees)
{
    if (m_List == nullptr)
        return UNDEFINED_ALTITUDE;

    SkyList *points = m_List->points();
    if (points == nullptr)
        return UNDEFINED_ALTITUDE;

    double constraint = UNDEFINED_ALTITUDE;
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
            // If the input angle is in the interval between the last two points,
            // interpolate the altitude constraint, and use that value.
            // If there are other line segments which also contain the point,
            // we use the max constraint.
            // Might convert to use a great circle, though, it may be overkill.
            // See: https://en.wikipedia.org/wiki/Great-circle_navigation
            const double totalDelta = fabs(lastAz.deltaAngle(az).Degrees());
            if (totalDelta <= 0)
                return alt;
            const double deltaToLast = fabs(lastAz.deltaAngle(desiredAzimuth).Degrees());
            const double weight = deltaToLast / totalDelta;
            constraint = std::max(constraint, (1.0 - weight) * lastAlt + weight * alt);
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
    qDeleteAll(m_HorizonList);
    m_HorizonList.clear();
}

bool ArtificialHorizonComponent::load()
{
    m_HorizonList = KStarsData::Instance()->userdb()->GetAllHorizons();

    foreach (ArtificialHorizonEntity *horizon, m_HorizonList)
        appendLine(horizon->list());

    return true;
}

void ArtificialHorizonComponent::save()
{
    KStarsData::Instance()->userdb()->DeleteAllHorizons();

    foreach (ArtificialHorizonEntity *horizon, m_HorizonList)
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

// Draws a single polygon whose azimuth,altitude points are (az1,0) (az1,alt1) (az2,alt2) (az2,0).
// That is, the area between the line az1,alt1 --> az2,alt2 and the horizon directly below it.
void drawHorizonPolygon(double az1, double alt1, double az2, double alt2, SkyPainter *painter)
{
    LineList region;

    std::shared_ptr<SkyPoint> sp(new SkyPoint());
    sp->setAz(az1);
    sp->setAlt(0);
    sp->HorizontalToEquatorial(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
    region.append(sp);

    std::shared_ptr<SkyPoint> sp2(new SkyPoint());
    sp2->setAz(az1);
    sp2->setAlt(alt1);
    sp2->HorizontalToEquatorial(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
    region.append(sp2);

    std::shared_ptr<SkyPoint> sp3(new SkyPoint());
    sp3->setAz(az2);
    sp3->setAlt(alt2);
    sp3->HorizontalToEquatorial(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
    region.append(sp3);

    std::shared_ptr<SkyPoint> sp4(new SkyPoint());
    sp4->setAz(az2);
    sp4->setAlt(0);
    sp4->HorizontalToEquatorial(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
    region.append(sp4);

    painter->drawSkyPolygon(&region, false);
}

// Returns an equivalent degrees in the range 0 <= 0 < 360
double normalizeDegrees(double degrees)
{
    while (degrees < 0)
        degrees += 360;
    while (degrees >= 360.0)
        degrees -= 360.0;
    return degrees;
}

// Draws a series of polygons of width in azimuth of "sampling degrees".
// Drawing a single polygon would have "great-circle issues". This looks a lot better.
// Assumes az1 and az2 in range 0-360 and az1 < az2.
void DrawSampledPolygons(double az1, double alt1, double az2, double alt2, double sampling, SkyPainter *painter)
{
    if (az1 > az2)
    {
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
        drawHorizonPolygon(lastAz, lastAlt, az, alt, painter);
        lastAz = az;
        lastAlt = alt;
    }
    drawHorizonPolygon(lastAz, lastAlt, az2, alt2, painter);
}

// This draws a series of polygons that fill the area beween the line list
// and the horizon directly below it.
// It draws the polygons one pair of points at a time, and deals with complications
// of when the angle wraps around.
void drawHorizonPolygons(LineList *lineList, SkyPainter *painter)
{
    const SkyList &points = *(lineList->points());
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
            // Compute the altitude at wrap-around.
            const double fraction = fabs(dms(360.0).deltaAngle(dms(maxAz)).Degrees() /
                                         p1.az().deltaAngle(p2.az()).Degrees());
            const double midAlt = minAzAlt + fraction * (maxAzAlt - minAzAlt);
            // Draw polygons form maxAz upto 0 degrees, then again from 0 to minAz.
            DrawSampledPolygons(maxAz, maxAzAlt, 360.0, midAlt, sampling, painter);
            DrawSampledPolygons(0, midAlt, minAz, minAzAlt, sampling, painter);
        }
        else
        {
            // Draw the polygons without wraparound
            DrawSampledPolygons(minAz, minAzAlt, maxAz, maxAzAlt, sampling, painter);
        }
    }
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

}  // namespace

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
        return;
    }

    preDraw(skyp);

    DrawID drawID = skyMesh()->drawID();

    for (int i = 0; i < listList().count(); i++)
    {
        std::shared_ptr<LineList> lineList = listList().at(i);
        if (lineList->drawID == drawID || m_HorizonList.at(i)->enabled() == false)
            continue;
        lineList->drawID = drawID;
        drawHorizonPolygons(lineList.get(), skyp);
    }
}

void ArtificialHorizonComponent::removeRegion(const QString &regionName, bool lineOnly)
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

    if (regionHorizon == nullptr)
        return;

    if (regionHorizon->list())
        removeLine(regionHorizon->list());

    if (lineOnly)
        regionHorizon->clearList();
    else
    {
        m_HorizonList.removeOne(regionHorizon);
        delete (regionHorizon);
    }
}

void ArtificialHorizonComponent::addRegion(const QString &regionName, bool enabled, const std::shared_ptr<LineList> &list)
{
    ArtificialHorizonEntity *horizon = new ArtificialHorizonEntity;

    horizon->setRegion(regionName);
    horizon->setEnabled(enabled);
    horizon->setList(list);

    m_HorizonList.append(horizon);

    appendLine(list);
}

double ArtificialHorizonComponent::altitudeConstraint(double azimuthDegrees) const
{
    double maxConstraint = UNDEFINED_ALTITUDE;
    foreach (ArtificialHorizonEntity *horizon, m_HorizonList)
    {
        if (horizon->enabled())
            maxConstraint = std::max(maxConstraint, horizon->altitudeConstraint(azimuthDegrees));
    }
    return maxConstraint;
}

bool ArtificialHorizonComponent::altitudeConstraintsExist() const
{
    foreach (ArtificialHorizonEntity *horizon, m_HorizonList)
    {
        if (horizon->enabled())
            return true;
    }
    return false;
}
