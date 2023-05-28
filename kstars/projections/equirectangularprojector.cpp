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
    double x, y;

    oRefract &= m_vp.useRefraction;
    if (m_vp.useAltAz)
    {
        double Y0;
        Y = SkyPoint::refract(o->alt(), oRefract).radians(); //account for atmospheric refraction
        Y0 = SkyPoint::refract(m_vp.focus->alt(), oRefract).radians();
        dX = m_vp.focus->az().reduce().radians() - o->az().reduce().radians();

        y = (Y - Y0);
    }
    else
    {
        dX   = o->ra().reduce().radians() - m_vp.focus->ra().reduce().radians();
        Y    = o->dec().radians();
        y = (Y - m_vp.focus->dec().radians());
    }

    dX = KSUtils::reduceAngle(dX, -dms::PI, dms::PI);

    x = dX;

    p = rst(x, y);

    if (onVisibleHemisphere)
        *onVisibleHemisphere = (p[0] > 0 && p[0] < m_vp.width);

    return p;
}

SkyPoint EquirectangularProjector::fromScreen(const QPointF &p, dms *LST, const dms *lat, bool onlyAltAz) const
{
    SkyPoint result;

    //Convert pixel position to x and y offsets in radians
    auto p_ = derst(p.x(), p.y());
    double dx = p_[0];
    double dy = p_[1];

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
    auto p_ = derst(p.x(), p.y());
    double dx = p_[0];
    double dy = p_[1];
    return (dx * dx > M_PI * M_PI / 4.0) || (dy * dy > M_PI * M_PI / 4.0);
}

QVector<Eigen::Vector2f> EquirectangularProjector::groundPoly(SkyPoint *labelpoint, bool *drawLabel) const
{
    float x0 = m_vp.width / 2.;
    if (m_vp.useAltAz)
    {
        float dX = M_PI;

        // N.B. alt ranges from -π/2 to π/2, but the focus can be at
        // either extreme, so the Y-range of the map is actually -π to
        // π -- asimha
        float dY = M_PI;

        SkyPoint belowFocus;
        belowFocus.setAz(m_vp.focus->az().Degrees());
        belowFocus.setAlt(0.0);

        // Compute the ends of the horizon line
        Eigen::Vector2f obf = toScreenVec(&belowFocus, false);
        auto obf_derst = derst(obf.x(), obf.y());
        auto corner1 = rst(obf_derst[0] - dX,
                           obf_derst[1]);
        auto corner2 = rst(obf_derst[0] + dX,
                           obf_derst[1]);

        auto corner3 = rst(obf_derst[0] + dX,
                           -dY);
        auto corner4 = rst(obf_derst[0] - dX,
                           -dY);

        QVector<Eigen::Vector2f> ground;
        //Construct the ground polygon, which is a simple rectangle in this case
        ground << corner1
               << corner2;
        if (m_vp.fillGround) {
               ground << corner3
                      << corner4;
        }

        if (labelpoint)
        {
            auto pLabel_ = corner2 - 50. * (corner1 - corner2).normalized();
            QPointF pLabel(pLabel_[0], pLabel_[1]);
            KStarsData *data = KStarsData::Instance();
            *labelpoint      = fromScreen(pLabel, data->lst(), data->geo()->lat());
        }
        if (drawLabel)
            *drawLabel = true;

        return ground;
    }
    else
    {
        float dX = m_vp.zoomFactor * M_PI; // RA ranges from 0 to 2π, so half-length is π
        float dY = m_vp.zoomFactor * M_PI;
        QVector<Eigen::Vector2f> ground;

        static const QString horizonLabel = i18n("Horizon");
        float marginLeft, marginRight, marginTop, marginBot;
        SkyLabeler::Instance()->getMargins(horizonLabel, &marginLeft, &marginRight, &marginTop, &marginBot);

        double daz = 180.;
        double faz = m_vp.focus->az().Degrees();
        double az1 = faz - daz;
        double az2 = faz + daz;

        bool inverted = ((m_vp.rotationAngle + 90.0_deg).reduce().Degrees() > 180.);
        bool allGround = true;
        bool allSky    = true;

        double inc = 1.0;
        //Add points along horizon
        std::vector<Eigen::Vector2f> groundPoints;
        for (double az = az1; az <= az2 + inc; az += inc)
        {
            SkyPoint p   = pointAt(az);
            bool visible = false;
            Eigen::Vector2f o   = toScreenVec(&p, false, &visible);
            if (visible)
            {
                groundPoints.push_back(o);
                //Set the label point if this point is onscreen
                if (labelpoint && o.x() < marginRight && o.y() > marginTop && o.y() < marginBot)
                    *labelpoint = p;

                if (o.y() > 0.)
                    allGround = false;
                if (o.y() < m_vp.height)
                    allSky = false;
            }
        }

        if (inverted)
            std::swap(allGround, allSky);

        if (allSky)
        {
            if (drawLabel)
                *drawLabel = false;
            return QVector<Eigen::Vector2f>();
        }

        const Eigen::Vector2f slope {m_vp.rotationAngle.cos(), m_vp.rotationAngle.sin()};
        std::sort(groundPoints.begin(), groundPoints.end(), [&](const Eigen::Vector2f & a,
                  const Eigen::Vector2f & b)
        {
            return a.dot(slope) < b.dot(slope);
        });

        for (auto point : groundPoints)
        {
            ground.append(point);
        }

        // if (allGround)
        // {
        //     ground.clear();
        //     ground.append(Eigen::Vector2f(x0 - dX, y0 - dY));
        //     ground.append(Eigen::Vector2f(x0 + dX, y0 - dY));
        //     ground.append(Eigen::Vector2f(x0 + dX, y0 + dY));
        //     ground.append(Eigen::Vector2f(x0 - dX, y0 + dY));
        //     if (drawLabel)
        //         *drawLabel = false;
        //     return ground;
        // }

        if (labelpoint)
        {
            QPointF pLabel(x0 - dX - 50., ground.last().y());
            KStarsData *data = KStarsData::Instance();
            *labelpoint      = fromScreen(pLabel, data->lst(), data->geo()->lat());
        }
        if (drawLabel)
            *drawLabel = true;

        const auto lat = KStarsData::Instance()->geo()->lat();
        const Eigen::Vector2f perpendicular {-m_vp.rotationAngle.sin(), m_vp.rotationAngle.cos()};
        const double sgn = (lat->Degrees() > 0 ? 1. : -1.);
        if (m_vp.fillGround)
        {
            ground.append(groundPoints.back() + perpendicular * sgn * dY);
            ground.append(groundPoints.front() + perpendicular * sgn * dY);
        }
        return ground;
    }
}

void EquirectangularProjector::updateClipPoly()
{
    m_clipPolygon.clear();

    m_clipPolygon << QPointF(0, 0) << QPointF(m_vp.width, 0) << QPointF(m_vp.width, m_vp.height)
                  << QPointF(0, m_vp.height);
}
