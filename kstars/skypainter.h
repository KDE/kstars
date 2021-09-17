/*
    SkyPainter: class for painting onto the sky for KStars
    SPDX-FileCopyrightText: 2010 Henry de Valence <hdevalence@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skycomponents/typedef.h"

#include <QList>
#include <QPainter>

class ConstellationsArt;
class DeepSkyObject;
class KSComet;
class KSPlanetBase;
class KSEarthShadow;
class LineList;
class LineListLabel;
class Satellite;
class SkipHashList;
class SkyMap;
class SkyObject;
class SkyPoint;
class Supernova;
class CatalogObject;

/**
 * @short Draws things on the sky, without regard to backend.
 * This class serves as an interface to draw objects onto the sky without
 * worrying about whether we are using a QPainter or OpenGL.
 */
class SkyPainter
{
    public:
        SkyPainter();

        virtual ~SkyPainter() = default;

        /** @short Set the pen of the painter **/
        virtual void setPen(const QPen &pen) = 0;

        /** @short Set the brush of the painter **/
        virtual void setBrush(const QBrush &brush) = 0;

        //FIXME: find a better way to do this.
        void setSizeMagLimit(float sizeMagLim);

        /**
         * Begin painting.
         * @note this function <b>must</b> be called before painting anything.
         * @see end()
         */
        virtual void begin() = 0;

        /**
         * End and finalize painting.
         * @note this function <b>must</b> be called after painting anything.
         * @note it is not guaranteed that anything will actually be drawn until end() is called.
         * @see begin();
         */
        virtual void end() = 0;

        ////////////////////////////////////
        //                                //
        // SKY DRAWING FUNCTIONS:         //
        //                                //
        ////////////////////////////////////

        /** @short Draw the sky background */
        virtual void drawSkyBackground() = 0;

        /**
         * @short Draw a line between points in the sky.
         * @param a the first point
         * @param b the second point
         * @note this function will skip lines not on screen and clip lines
         * that are only partially visible.
         */
        virtual void drawSkyLine(SkyPoint *a, SkyPoint *b) = 0;

        /**
         * @short Draw a polyline in the sky.
         * @param list a list of points in the sky
         * @param skipList a SkipList object used to control skipping line segments
         * @param label a pointer to the label for this line
         * @note it's more efficient to use this than repeated calls to drawSkyLine(),
         * because it avoids an extra points->size() -2 projections.
         */
    virtual void drawSkyPolyline(LineList *list, SkipHashList *skipList = nullptr,
                                 LineListLabel *label = nullptr) = 0;

        /**
         * @short Draw a polygon in the sky.
         * @param list a list of points in the sky
         * @param forceClip If true (default), it enforces clipping of the polygon, otherwise, it draws the
         * complete polygen without running any boundary checks.
         * @see drawSkyPolyline()
         */
        virtual void drawSkyPolygon(LineList *list, bool forceClip = true) = 0;

        /**
         * @short Draw a comet in the sky.
         * @param com comet to draw
         * @return true if a comet was drawn
         */
        virtual bool drawComet(KSComet *com) = 0;

        /**
         * @short Draw a point source (e.g., a star).
         * @param loc the location of the source in the sky
         * @param mag the magnitude of the source
         * @param sp the spectral class of the source
         * @return true if a source was drawn
         */
    virtual bool drawPointSource(const SkyPoint *loc, float mag, char sp = 'A') = 0;

        /**
     * @short Draw a deep sky object (loaded from the new implementation)
     * @param obj the object to draw
     * @param drawImage if true, try to draw the image of the object
     * @return true if it was drawn
     */
    virtual bool drawCatalogObject(const CatalogObject &obj) = 0;

    /**
         * @short Draw a planet
         * @param planet the planet to draw
         * @return true if it was drawn
         */
        virtual bool drawPlanet(KSPlanetBase *planet) = 0;

        /**
         * @short Draw the earths shadow on the moon (red-ish)
         * @param shadow the shadow to draw
         * @return true if it was drawn
         */
        virtual bool drawEarthShadow(KSEarthShadow *shadow) = 0;

        /**
         * @short Draw the symbols for the observing list
         * @param obs the observing list
         */
        virtual void drawObservingList(const QList<SkyObject *> &obs) = 0;

        /** @short Draw flags */
        virtual void drawFlags() = 0;

        /** @short Draw a satellite */
        virtual bool drawSatellite(Satellite *sat) = 0;

        /** @short Draw a Supernova */
        virtual bool drawSupernova(Supernova *sup) = 0;

    virtual void drawHorizon(bool filled, SkyPoint *labelPoint = nullptr,
                             bool *drawLabel = nullptr) = 0;

        /** @short Get the width of a star of magnitude mag */
        float starWidth(float mag) const;

        /**
         * @short Draw a ConstellationsArt object
         * @param obj the object to draw
         * @return true if it was drawn
         */
        virtual bool drawConstellationArtImage(ConstellationsArt *obj) = 0;

        /**
         * @brief drawHips Draw HIPS all sky catalog
         * @return true if it was drawn
         */
        virtual bool drawHips() = 0;

        /**
         * @brief drawHips Draw the Terrain
         * @return true if it was drawn
         */
        virtual bool drawTerrain() = 0;

    protected:
    SkyMap *m_sm{ nullptr };

    private:
    float m_sizeMagLim{ 10.0f };
};
