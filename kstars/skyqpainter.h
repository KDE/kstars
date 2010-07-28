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

class Projector;

/** @short The QPainter-based painting backend.
    This class implements the SkyPainter interface using a QPainter.
    For documentation, @see SkyPainter. */
class SkyQPainter : public SkyPainter, public QPainter
{
public:
    /** Constructor.
        @param sm the SkyMap pointer
        @param pd the painting device. If 0, then @p sm will be used.
        */
    SkyQPainter(SkyMap *sm, QPaintDevice *pd = 0);
    virtual ~SkyQPainter();
    virtual void setPen(const QPen& pen);
    virtual void setBrush(const QBrush& brush);

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
private:
    ///This function exists so that we can draw other objects (e.g., planets) as point sources.
    virtual void drawPointSource(const QPointF& pos, float size, char sp = 'A');
    virtual void drawDeepSkySymbol(const QPointF& pos, DeepSkyObject* obj,
                                         float positionAngle);
    virtual bool drawDeepSkyImage (const QPointF& pos, DeepSkyObject* obj,
                                         float positionAngle);
    QPaintDevice *m_pd;
    const Projector* m_proj;
};

#endif
