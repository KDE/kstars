/*
    SPDX-FileCopyrightText: 2010 Henry de Valence <hdevalence@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#ifdef KSTARS_LITE
#include "skymaplite.h"
#else
#include "skymap.h"
#endif
#include "skyobjects/skypoint.h"

#if __GNUC__ > 5
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-attributes"
#endif
#if __GNUC__ > 6
#pragma GCC diagnostic ignored "-Wint-in-bool-context"
#endif
#include <Eigen/Core>
#if __GNUC__ > 5
#pragma GCC diagnostic pop
#endif

#include <QPointF>

#include <cstddef>
#include <cmath>

class KStarsData;

/** This is just a container that holds information needed to do projections. */
class ViewParams
{
    public:
        float width, height;
        float zoomFactor;
        bool useRefraction;
        bool useAltAz;
        bool fillGround; ///<If the ground is filled, then points below horizon are invisible
        SkyPoint *focus;
        ViewParams() : width(0), height(0), zoomFactor(0),
            useRefraction(false), useAltAz(false), fillGround(false),
            focus(nullptr) {}
};

/**
 * @class Projector
 *
 * The Projector class is the primary class that serves as an interface to handle projections.
 */
class Projector
{
        Q_GADGET
    public:
        /**
         * Constructor.
         *
         * @param p the ViewParams for this projection
         */
        explicit Projector(const ViewParams &p);

        virtual ~Projector() = default;

        /** Update cached values for projector */
        void setViewParams(const ViewParams &p);
        ViewParams viewParams() const
        {
            return m_vp;
        }

        enum Projection
        {
            Lambert,
            AzimuthalEquidistant,
            Orthographic,
            Equirectangular,
            Stereographic,
            Gnomonic,
            UnknownProjection
        };
        Q_ENUM(Projection)

        /** Return the type of this projection */
        Q_INVOKABLE virtual Projection type() const = 0;

        /** Return the FOV of this projection */
        double fov() const;

        /**
         * Check if the current point on screen is a valid point on the sky. This is needed
         * to avoid a crash of the program if the user clicks on a point outside the sky (the
         * corners of the sky map at the lowest zoom level are the invalid points).
         * @param p the screen pixel position
         */
        virtual bool unusablePoint(const QPointF &p) const;

        /**
         * Given the coordinates of the SkyPoint argument, determine the
         * pixel coordinates in the SkyMap.
         *
         * Since most of the projections used by KStars are very similar,
         * if this function were to be reimplemented in each projection subclass
         * we would end up changing maybe 5 or 6 lines out of 150.
         * Instead, we have a default implementation that uses the projectionK
         * and projectionL functions to take care of the differences between
         * e.g. Orthographic and Stereographic. There is also the cosMaxFieldAngle
         * function, which is used for testing whether a point is on the visible
         * part of the projection, and the radius function which gives the radius of
         * the projection in screen coordinates.
         *
         * While this seems ugly, it is less ugly than duplicating 150 loc to change 5.
         *
         * @return Eigen::Vector2f containing screen pixel x, y coordinates of SkyPoint.
         * @param o pointer to the SkyPoint for which to calculate x, y coordinates.
         * @param oRefract true = use Options::useRefraction() value.
         *   false = do not use refraction.  This argument is only needed
         *   for the Horizon, which should never be refracted.
         * @param onVisibleHemisphere pointer to a bool to indicate whether the point is
         *   on the visible part of the Celestial Sphere.
         */
        virtual Eigen::Vector2f toScreenVec(const SkyPoint *o, bool oRefract = true,
                                            bool *onVisibleHemisphere = nullptr) const;

        /**
         * This is exactly the same as toScreenVec but it returns a QPointF.
         * It just calls toScreenVec and converts the result.
         * @see toScreenVec()
         */
        QPointF toScreen(const SkyPoint *o, bool oRefract = true, bool *onVisibleHemisphere = nullptr) const;

        /**
         * @short Determine RA, Dec coordinates of the pixel at (dx, dy), which are the
         * screen pixel coordinate offsets from the center of the Sky pixmap.
         * @param p the screen pixel position to convert
         * @param LST pointer to the local sidereal time, as a dms object.
         * @param lat pointer to the current geographic laitude, as a dms object
         * @param onlyAltAz the returned SkyPoint's RA & DEC are not computed, only Alt/Az.
         */
        virtual SkyPoint fromScreen(const QPointF &p, dms *LST, const dms *lat, bool onlyAltAz = false) const;

        /**
         * ASSUMES *p1 did not clip but *p2 did.  Returns the QPointF on the line
         * between *p1 and *p2 that just clips.
         */
        QPointF clipLine(SkyPoint *p1, SkyPoint *p2) const;

        /**
         * ASSUMES *p1 did not clip but *p2 did.  Returns the Eigen::Vector2f on the line
         * between *p1 and *p2 that just clips.
         */
        Eigen::Vector2f clipLineVec(SkyPoint *p1, SkyPoint *p2) const;

        /** Check whether the projected point is on-screen */
        bool onScreen(const QPointF &p) const;
        bool onScreen(const Eigen::Vector2f &p) const;

        /**
         * @short Determine if the skypoint p is likely to be visible in the display window.
         *
         * checkVisibility() is an optimization function.  It determines whether an object
         * appears within the bounds of the skymap window, and therefore should be drawn.
         * The idea is to save time by skipping objects which are off-screen, so it is
         * absolutely essential that checkVisibility() is significantly faster than
         * the computations required to draw the object to the screen.
         *
         * If the ground is to be filled, the function first checks whether the point is
         * below the horizon, because they will be covered by the ground anyways.
         * Importantly, it does not call the expensive EquatorialToHorizontal function.
         * This means that the horizontal coordinates MUST BE CORRECT! The vast majority
         * of points are already synchronized, so recomputing the horizontal coordinates is
         * a waste.
         *
         * The function then checks the difference between the Declination/Altitude
         * coordinate of the Focus position, and that of the point p.  If the absolute
         * value of this difference is larger than fov, then the function returns false.
         * For most configurations of the sky map window, this simple check is enough to
         * exclude a large number of objects.
         *
         * Next, it determines if one of the poles of the current Coordinate System
         * (Equatorial or Horizontal) is currently inside the sky map window.  This is
         * stored in the member variable 'bool SkyMap::isPoleVisible, and is set by the
         * function SkyMap::setMapGeometry(), which is called by SkyMap::paintEvent().
         * If a Pole is visible, then it will return true immediately.  The idea is that
         * when a pole is on-screen it is computationally expensive to determine whether
         * a particular position is on-screen or not: for many valid Dec/Alt values, *all*
         * values of RA/Az will indeed be onscreen, but for other valid Dec/Alt values,
         * only *most* RA/Az values are onscreen.  It is cheaper to simply accept all
         * "horizontal" RA/Az values, since we have already determined that they are
         * on-screen in the "vertical" Dec/Alt coordinate.
         *
         * Finally, if no Pole is onscreen, it checks the difference between the Focus
         * position's RA/Az coordinate and that of the point p.  If the absolute value of
         * this difference is larger than XMax, the function returns false.  Otherwise,
         * it returns true.
         *
         * @param p pointer to the skypoint to be checked.
         * @return true if the point p was found to be inside the Sky map window.
         * @see SkyMap::setMapGeometry()
         * @see SkyMap::fov()
         * @note If you are creating skypoints using equatorial coordinates, then
         * YOU MUST CALL EQUATORIALTOHORIZONTAL BEFORE THIS FUNCTION!
         */
        bool checkVisibility(const SkyPoint *p) const;

        /**
         * Determine the on-screen position angle of a SkyPont with recept with NCP.
         * This is the object's sky position angle (w.r.t. North).
         * of "North" at the position of the object (w.r.t. the screen Y-axis).
         * The latter is determined by constructing a test point with the same RA but
         * a slightly increased Dec as the object, and calculating the angle w.r.t. the
         * Y-axis of the line connecting the object to its test point.
         */
        double findNorthPA(const SkyPoint *o, float x, float y) const;

        /**
         * Determine the on-screen position angle of a SkyObject.  This is the sum
         * of the object's sky position angle (w.r.t. North), and the position angle
         * of "North" at the position of the object (w.r.t. the screen Y-axis).
         * The latter is determined by constructing a test point with the same RA but
         * a slightly increased Dec as the object, and calculating the angle w.r.t. the
         * Y-axis of the line connecting the object to its test point.
         */
        double findPA(const SkyObject *o, float x, float y) const;

        /**
         * Get the ground polygon
         * @param labelpoint This point will be set to something suitable for attaching a label
         * @param drawLabel this tells whether to draw a label.
         * @return the ground polygon
         */
        virtual QVector<Eigen::Vector2f> groundPoly(SkyPoint *labelpoint = nullptr, bool *drawLabel = nullptr) const;

        /**
         * @brief updateClipPoly calculate the clipping polygen given the current FOV.
         */
        virtual void updateClipPoly();

        /**
         * @return the clipping polygen covering the visible sky area. Anything outside this polygon is
         * clipped by QPainter.
         */
        virtual QPolygonF clipPoly() const;

    protected:
        /**
         * Get the radius of this projection's sky circle.
         * @return the radius in radians
         */
        virtual double radius() const
        {
            return 2 * M_PI;
        }

        /**
         * This function handles some of the projection-specific code.
         * @see toScreen()
         */
        virtual double projectionK(double x) const
        {
            return x;
        }

        /**
         * This function handles some of the projection-specific code.
         * @see toScreen()
         */
        virtual double projectionL(double x) const
        {
            return x;
        }

        /**
         * This function returns the cosine of the maximum field angle, i.e., the maximum angular
         * distance from the focus for which a point should be projected. Default is 0, i.e.,
         * 90 degrees.
         */
        virtual double cosMaxFieldAngle() const
        {
            return 0;
        }

        /**
         * Helper function for drawing ground.
         * @return the point with Alt = 0, az = @p az
         */
        static SkyPoint pointAt(double az);

        KStarsData *m_data { nullptr };
        ViewParams m_vp;
        double m_sinY0 { 0 };
        double m_cosY0 { 0 };
        double m_fov { 0 };
        QPolygonF m_clipPolygon;

    private:
        //Used by CheckVisibility
        double m_xrange { 0 };
        bool m_isPoleVisible { false };
};
