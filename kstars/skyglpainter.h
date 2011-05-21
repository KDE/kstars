/*
    Copyright (C) 2010 Henry de Valence <hdevalence@gmail.com>

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

#ifndef SKYGLPAINTER_H
#define SKYGLPAINTER_H

#include <Eigen/Core>
USING_PART_OF_NAMESPACE_EIGEN

#include "skypainter.h"
#include "skyobjects/skyobject.h"
#include "projections/projector.h"

class QGLWidget;

class SkyGLPainter : public SkyPainter
{
public:
    SkyGLPainter( QGLWidget *widget );
    virtual bool drawPlanet(KSPlanetBase* planet);
    virtual bool drawDeepSkyObject(DeepSkyObject* obj, bool drawImage = false);
    virtual bool drawPointSource(SkyPoint* loc, float mag, char sp = 'A');
    virtual void drawSkyPolygon(LineList* list);
    virtual void drawSkyPolyline(LineList* list, SkipList* skipList = 0, LineListLabel* label = 0);
    virtual void drawSkyLine(SkyPoint* a, SkyPoint* b);
    virtual void drawSkyBackground();
    virtual void drawObservingList(const QList<SkyObject*>& obs);
    virtual void drawFlags();
    virtual void end();
    virtual void begin();
    virtual void setBrush(const QBrush& brush);
    virtual void setPen(const QPen& pen);
    virtual void drawHorizon( bool filled, SkyPoint *labelPoint = 0, bool *drawLabel = 0);
    virtual void drawSatellite( Satellite* sat );
    void drawText( int x, int y, const QString text, QFont font, QColor color );
private:
    bool addItem(SkyPoint* p, int type, float width, char sp = 'a');
    void drawBuffer(int type);
    void drawPolygon(const QVector< Vector2f >& poly, bool convex = true, bool flush_buffers = true);

    /** Render textured rectangle on screeen. Parameters are texture
     *  to be used, position, orientation and size of rectangle*/
    void drawTexturedRectangle( const QImage& img,
                                const Vector2f& pos, const float angle,
                                const float sizeX, const float sizeY );

    const Projector *m_proj;
    
    Vector4f m_pen;
    static const int BUFSIZE = 512;
    ///FIXME: what kind of TYPE_UNKNOWN objects are there?
    static const int NUMTYPES = (int)SkyObject::TYPE_UNKNOWN + 1;
    static Vector2f m_vertex[NUMTYPES][6*BUFSIZE];
    static Vector2f m_texcoord[NUMTYPES][6*BUFSIZE];
    static Vector3f m_color[NUMTYPES][6*BUFSIZE];
    static int m_idx[NUMTYPES];
    static bool m_init; ///< keep track of whether we have filled the texcoord array
    QGLWidget* m_widget; // Pointer to (GL) widget on which we are painting
};

#endif // SKYGLPAINTER_H
