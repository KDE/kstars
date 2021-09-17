/*
    SPDX-FileCopyrightText: 2010 Henry de Valence <hdevalence@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "equirectangularprojector.h"

#include "ksutils.h"
#include "kstarsdata.h"
#include "skycomponents/skylabeler.h"

EquirectangularProjector::EquirectangularProjector(const ViewParams &p) : Projector(p)
{
    updateClipPoly();
}

Projector::Projection EquirectangularProjector::type() const
{
    return Equirectangular;
}

double EquirectangularProjector::radius() const
{
    return 1.0;
}

Eigen::Vector2f EquirectangularProjector::toScreenVec(const SkyPoint *o, bool oRefract, bool *onVisibleHemisphere) const
{
    double Y, dX;
    Eigen::Vector2f p;

    oRefract &= m_vp.useRefraction;
    if (m_vp.useAltAz)
    {
        double Y0;
        Y = SkyPoint::refract(o->alt(), oRefract).radians(); //account for atmospheric refraction
        Y0 = SkyPoint::refract(m_vp.focus->alt(), oRefract).radians();
        dX = m_vp.focus->az().reduce().radians() - o->az().reduce().radians();

        p[1] = 0.5 * m_vp.height - m_vp.zoomFactor * (Y - Y0);
    }
    else
    {
        dX   = o->ra().reduce().radians() - m_vp.focus->ra().reduce().radians();
        Y    = o->dec().radians();
        p[1] = 0.5 * m_vp.height - m_vp.zoomFactor * (Y - m_vp.focus->dec().radians());
    }

    dX = KSUtils::reduceAngle(dX, -dms::PI, dms::PI);

    p[0] = 0.5 * m_vp.width - m_vp.zoomFactor * dX;

    if (onVisibleHemisphere)
        *onVisibleHemisphere = (p[0] > 0 && p[0] < m_vp.width);

    return p;
}

SkyPoint EquirectangularProjector::fromScreen(const QPointF &p, dms *LST, const dms *lat, bool onlyAltAz) const
{
    SkyPoint result;

    //Convert pixel position to x and y offsets in radians
    double dx = (0.5 * m_vp.width - p.x()) / m_vp.zoomFactor;
    double dy = (0.5 * m_vp.height - p.y()) / m_vp.zoomFactor;

    if (m_vp.useAltAz)
    {
        dms az, alt;
        dx = -1.0 * dx; //Azimuth goes in opposite direction compared to RA
        az.setRadians(dx + m_vp.focus->az().radians());
        alt.setRadians(dy + SkyPoint::refract(m_vp.focus->alt(), m_vp.useRefraction).radians());
        result.setAz(az.reduce());
        if (m_vp.useRefraction)
            alt = SkyPoint::unrefract(alt);
        result.setAlt(alt);
        if (!onlyAltAz)
            result.HorizontalToEquatorial(LST, lat);
        return result;
    }
    else
    {
        dms ra, dec;
        ra.setRadians(dx + m_vp.focus->ra().radians());
        dec.setRadians(dy + m_vp.focus->dec().radians());
        result.set(ra.reduce(), dec);
        result.EquatorialToHorizontal(LST, lat);
        return result;
    }
}

bool EquirectangularProjector::unusablePoint(const QPointF &p) const
{
    double dx = (0.5 * m_vp.width - p.x()) / m_vp.zoomFactor;
    double dy = (0.5 * m_vp.height - p.y()) / m_vp.zoomFactor;
    return (dx * dx > M_PI * M_PI / 4.0) || (dy * dy > M_PI * M_PI / 4.0);
}

QVector<Eigen::Vector2f> EquirectangularProjector::groundPoly(SkyPoint *labelpoint, bool *drawLabel) const
{
    float x0 = m_vp.width / 2.;
    float y0 = m_vp.width / 2.;
    if (m_vp.useAltAz)
    {
        float dX = m_vp.zoomFactor * M_PI;
        float dY = m_vp.zoomFactor * M_PI;
        SkyPoint belowFocus;
        belowFocus.setAz(m_vp.focus->az().Degrees());
        belowFocus.setAlt(0.0);

        Eigen::Vector2f obf = toScreenVec(&belowFocus, false);

        //If the horizon is off the bottom edge of the screen,
        //we can return immediately
        if (obf.y() > m_vp.height)
        {
            if (drawLabel)
                *drawLabel = false;
            return QVector<Eigen::Vector2f>();
        }

        //We can also return if the horizon is off the top edge,
        //as long as the ground poly is not being drawn
        if (obf.y() < 0. && m_vp.fillGround == false)
        {
            if (drawLabel)
                *drawLabel = false;
            return QVector<Eigen::Vector2f>();
        }

        QVector<Eigen::Vector2f> ground;
        //Construct the ground polygon, which is a simple rectangle in this case
        ground << Eigen::Vector2f(x0 - dX, obf.y()) << Eigen::Vector2f(x0 + dX, obf.y()) << Eigen::Vector2f(x0 + dX, y0 + dY)
               << Eigen::Vector2f(x0 - dX, y0 + dY);

        if (labelpoint)
        {
            QPointF pLabel(x0 - dX - 50., obf.y());
            KStarsData *data = KStarsData::Instance();
            *labelpoint      = fromScreen(pLabel, data->lst(), data->geo()->lat());
        }
        if (drawLabel)
            *drawLabel = true;

        return ground;
    }
    else
    {
        float dX = m_vp.zoomFactor * M_PI / 2;
        float dY = m_vp.zoomFactor * M_PI / 2;
        QVector<Eigen::Vector2f> ground;

        static const QString horizonLabel = i18n("Horizon");
        float marginLeft, marginRight, marginTop, marginBot;
        SkyLabeler::Instance()->getMargins(horizonLabel, &marginLeft, &marginRight, &marginTop, &marginBot);
        double daz = 90.;
        double faz = m_vp.focus->az().Degrees();
        double az1 = faz - daz;
        double az2 = faz + daz;

        bool allGround = true;
        bool allSky    = true;

        double inc = 1.0;
        //Add points along horizon
        for (double az = az1; az <= az2 + inc; az += inc)
        {
            SkyPoint p   = pointAt(az);
            bool visible = false;
            Eigen::Vector2f o   = toScreenVec(&p, false, &visible);
            if (visible)
            {
                ground.append(o);
                //Set the label point if this point is onscreen
                if (labelpoint && o.x() < marginRight && o.y() > marginTop && o.y() < marginBot)
                    *labelpoint = p;

                if (o.y() > 0.)
                    allGround = false;
                if (o.y() < m_vp.height)
                    allSky = false;
            }
        }

        if (allSky)
        {
            if (drawLabel)
                *drawLabel = false;
            return QVector<Eigen::Vector2f>();
        }

        if (allGround)
        {
            ground.clear();
            ground.append(Eigen::Vector2f(x0 - dX, y0 - dY));
            ground.append(Eigen::Vector2f(x0 + dX, y0 - dY));
            ground.append(Eigen::Vector2f(x0 + dX, y0 + dY));
            ground.append(Eigen::Vector2f(x0 - dX, y0 + dY));
            if (drawLabel)
                *drawLabel = false;
            return ground;
        }

        if (labelpoint)
        {
            QPointF pLabel(x0 - dX - 50., ground.last().y());
            KStarsData *data = KStarsData::Instance();
            *labelpoint      = fromScreen(pLabel, data->lst(), data->geo()->lat());
        }
        if (drawLabel)
            *drawLabel = true;

        //Now add points along the ground
        ground.append(Eigen::Vector2f(x0 + dX, ground.last().y()));
        ground.append(Eigen::Vector2f(x0 + dX, y0 + dY));
        ground.append(Eigen::Vector2f(x0 - dX, y0 + dY));
        ground.append(Eigen::Vector2f(x0 - dX, ground.first().y()));
        return ground;
    }
}

void EquirectangularProjector::updateClipPoly()
{
    m_clipPolygon.clear();

    m_clipPolygon << QPointF(0, 0) << QPointF(m_vp.width, 0) << QPointF(m_vp.width, m_vp.height)
                  << QPointF(0, m_vp.height);
}
