/*
    (C) 2010 Henry de Valence <hdevalence@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*/

#ifndef SKYQPAINTER_H
#define SKYQPAINTER_H

#include "skypainter.h"
// REMOVE THIS TESTING ONLY
#include "dms.h"

class Projector;
class QWidget;
class QSize;
class QMessageBox;
/** @short The QPainter-based painting backend.
    This class implements the SkyPainter interface using a QPainter.
    For documentation, @see SkyPainter. */
class SkyQPainter : public SkyPainter, public QPainter
{
public:

    /**
     * @short Creates a SkyQPainter with the given QPaintDevice and uses the dimensions of the paint device as canvas dimensions
     * @param pd the painting device. Cannot be 0
     * @param canvasSize the size of the canvas
     */
    SkyQPainter( QPaintDevice *pd, const QSize &canvasSize );

    /**
     * @short Creates a SkyQPainter with the given QPaintDevice and given canvas size
     * @param pd the painting device. Cannot be 0
     */
    SkyQPainter( QPaintDevice *pd );

    /**
     * @short Creates a SkyQPainter given a QWidget and an optional QPaintDevice.
     * @param widget the QWidget that provides the canvas size, and also the paint device unless @p pd is specified
     * @param pd the painting device. If 0, then @p widget will be used.
     */
    explicit SkyQPainter( QWidget *widget, QPaintDevice *pd = 0 );

    virtual ~SkyQPainter();
    virtual void setPen(const QPen& pen);
    virtual void setBrush(const QBrush& brush);

    /**
     * @param vectorStars Draw stars as vector graphics whenever possible.
     * @note Drawing stars as vectors is slower, but is better when saving .svg files. Set to true only when you are drawing on a canvas where speed doesn't matter. Definitely not when drawing on the SkyMap.
     */
    inline void setVectorStars( bool vectorStars ) { m_vectorStars = vectorStars; }
    inline bool getVectorStars() const { return m_vectorStars; }

    virtual void begin();
    virtual void end();
    
    /** Recalculates the star pixmaps. */
    static void initStarImages();
    
    // Sky drawing functions
    virtual void drawSkyBackground();
    virtual void drawSkyLine(SkyPoint* a, SkyPoint* b);
    virtual void drawSkyPolyline(LineList* list, SkipList *skipList = 0,
                                 LineListLabel *label = 0);
    virtual void drawSkyPolygon(LineList* list);
    virtual bool drawPointSource(SkyPoint *loc, float mag, char sp = 'A');
    virtual bool drawDeepSkyObject(DeepSkyObject *obj, bool drawImage = false);
    virtual bool drawPlanet(KSPlanetBase *planet);
    virtual void drawObservingList(const QList<SkyObject*>& obs);
    virtual void drawFlags();
    virtual void drawHorizon( bool filled, SkyPoint *labelPoint = 0, bool *drawLabel = 0);
    virtual void drawSatellite( Satellite* sat );
    virtual void drawDeepSkySymbol(const QPointF& pos, int type, float size, float e,
                                         float positionAngle);
    virtual bool drawSupernova(Supernova* sup);
    ///This function exists so that we can draw other objects (e.g., planets) as point sources.
    virtual void drawPointSource(const QPointF& pos, float size, char sp = 'A');
    virtual bool drawConstellationArtImage(ConstellationsArt *obj);

    static void setCHelper(dms cra, dms cdec, dms cpa, dms c_w, dms c_h) { ra = cra; dec=cdec; pa=cpa; cw=c_w; ch=c_h; }

private:
    virtual bool drawDeepSkyImage (const QPointF& pos, DeepSkyObject* obj,
                                         float positionAngle);
    QPaintDevice *m_pd;
    const Projector* m_proj;
    bool m_vectorStars;
    QSize m_size;
    static int starColorMode;

    static dms ra, dec, pa, cw, ch ;
};

#endif
