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

#include "skyglpainter.h"

#include <Eigen/Core>
#include <Eigen/Geometry>
USING_PART_OF_NAMESPACE_EIGEN
using Eigen::Rotation2Df;

#include <GL/gl.h>

#include "skymap.h"
#include "kstarsdata.h"
#include "Options.h"

#include "texturemanager.h"
#include "texture.h"

#include "skycomponents/linelist.h"
#include "skycomponents/skiplist.h"
#include "skycomponents/linelistlabel.h"

#include "skyobjects/deepskyobject.h"
#include "skyobjects/kscomet.h"
#include "skyobjects/ksasteroid.h"
#include "skyobjects/trailobject.h"

Vector2f SkyGLPainter::m_vertex[NUMTYPES][6*BUFSIZE];
Vector2f SkyGLPainter::m_texcoord[NUMTYPES][6*BUFSIZE];
Vector3f SkyGLPainter::m_color[NUMTYPES][6*BUFSIZE];
int      SkyGLPainter::m_idx[NUMTYPES];
bool     SkyGLPainter::m_init = false;

SkyGLPainter::SkyGLPainter(SkyMap* sm)
    : SkyPainter(sm)
{
    if( !m_init ) {
        printf("Initializing texcoord arrays...\n");
        for(int i = 0; i < NUMTYPES; ++i) {
            m_idx[i] = 0;
            for(int j = 0; j < BUFSIZE; ++j) {
                m_texcoord[i][6*j +0] = Vector2f(0,0);
                m_texcoord[i][6*j +1] = Vector2f(1,0);
                m_texcoord[i][6*j +2] = Vector2f(0,1);
                m_texcoord[i][6*j +3] = Vector2f(0,1);
                m_texcoord[i][6*j +4] = Vector2f(1,0);
                m_texcoord[i][6*j +5] = Vector2f(1,1);
            }
        }
        m_init = true;
    }
}

void SkyGLPainter::drawBuffer(int type)
{
    //printf("Drawing buffer for type %d, has %d objects\n", type, m_idx[type]);
    if( m_idx[type] == 0 ) return;

    glEnable(GL_TEXTURE_2D);
    Texture *tex = 0;
    switch( type ) {
        case 3: case 13:          tex = TextureManager::getTexture("open-cluster"); break;
        case 4: case 7:           tex = TextureManager::getTexture("globular-cluster"); break;
        case 5: case 6: case 15:  tex = TextureManager::getTexture("gaseous-nebula"); break;
        case 8: case 16:          tex = TextureManager::getTexture("galaxy"); break;
        case 14:                  tex = TextureManager::getTexture("galaxy-cluster"); break;
        case 0: case 1: default:  tex = TextureManager::getTexture("star"); break;
    }
    tex->bind();
    
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    
    glVertexPointer  (2,GL_FLOAT,0, &m_vertex[type]);
    glTexCoordPointer(2,GL_FLOAT,0, &m_texcoord[type]);
    glColorPointer   (3,GL_FLOAT,0, &m_color[type]);
    
    glDrawArrays(GL_TRIANGLES, 0, 6*m_idx[type]);
    m_idx[type] = 0;
    
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

bool SkyGLPainter::addItem(SkyPoint* p, int type, float width, char sp)
{
    //don't draw things that are too small
    if( width < 1.5 ) return false;
    bool visible = false;
    Vector2f vec = m_sm->toScreenVec(p,true,&visible);
    if(!visible) return false;
    
    //If the buffer is full, flush it
    if(m_idx[type] == BUFSIZE) {
        drawBuffer(type);
    }

    int i = 6*m_idx[type];
    float w = width/2.;
    
    m_vertex[type][i + 0] = vec + Vector2f(-w,-w);
    m_vertex[type][i + 1] = vec + Vector2f( w,-w);
    m_vertex[type][i + 2] = vec + Vector2f(-w, w);
    m_vertex[type][i + 3] = vec + Vector2f(-w, w);
    m_vertex[type][i + 4] = vec + Vector2f( w,-w);
    m_vertex[type][i + 5] = vec + Vector2f( w, w);

    Vector3f c(1.,1.,1.);
    switch(sp) {
        case 'o': case 'O': c = Vector3f( 153./255., 153./255., 255./255.); break;
        case 'b': case 'B': c = Vector3f( 151./255., 233./255., 255./255.); break;
        case 'a': case 'A': c = Vector3f( 153./255., 255./255., 255./255.); break;
        case 'f': case 'F': c = Vector3f( 219./255., 255./255., 135./255.); break;
        case 'g': case 'G': c = Vector3f( 255./255., 255./255., 153./255.); break;
        case 'k': case 'K': c = Vector3f( 255./255., 193./255., 153./255.); break;
        case 'm': case 'M': c = Vector3f( 255./255., 153./255., 153./255.); break;
        case 'x':           c = Vector3f( m_pen[0], m_pen[1], m_pen[2]   ); break;
    } 

    for(int j = 0; j < 6; ++j) {
        m_color[type][i+j] = c;
    }
    
    ++m_idx[type];
    return true;
}

bool SkyGLPainter::drawPlanet(KSPlanetBase* planet)
{
    float fakeStarSize = ( 10.0 + log10( Options::zoomFactor() ) - log10( MINZOOM ) ) * ( 10 - planet->mag() ) / 10;
    return addItem(planet, planet->type(), qMin(fakeStarSize,(float)20.));
}

bool SkyGLPainter::drawDeepSkyObject(DeepSkyObject* obj, bool drawImage)
{
    int type = obj->type();
    //addItem(obj, type, obj->a() * dms::PI * Options::zoomFactor() / 10800.0);
    
    //If it's a star, add it like a star
    if( type < 2 ) {
        return addItem(obj, type, starWidth(obj->mag()));
    } else {
        bool visible = false;
        Vector2f vec = m_sm->toScreenVec(obj,true,&visible);
        if(!visible) return false;

        //If the buffer is full, flush it
        if(m_idx[type] == BUFSIZE) {
            drawBuffer(type);
        }

        const int i = 6*m_idx[type];
        float width = obj->a() * dms::PI * Options::zoomFactor() / 10800.0;
        float pa = m_sm->findPA(obj, vec[0], vec[1]) * (M_PI/180.0);
        Rotation2Df r(pa);
        float w = width/2.;
        float h = w * obj->e();

        m_vertex[type][i + 0] = vec + r* Vector2f(-w,-h);
        m_vertex[type][i + 1] = vec + r* Vector2f( w,-h);
        m_vertex[type][i + 2] = vec + r* Vector2f(-w, h);
        m_vertex[type][i + 3] = vec + r* Vector2f(-w, h);
        m_vertex[type][i + 4] = vec + r* Vector2f( w,-h);
        m_vertex[type][i + 5] = vec + r* Vector2f( w, h);

        Vector3f c( m_pen[0], m_pen[1], m_pen[2]   );

        for(int j = 0; j < 6; ++j) {
            m_color[type][i+j] = c;
        }

        ++m_idx[type];
        return true;
    }
}

bool SkyGLPainter::drawPointSource(SkyPoint* loc, float mag, char sp)
{
    return addItem(loc, SkyObject::STAR, starWidth(mag), sp);
}

void SkyGLPainter::drawSkyPolygon(LineList* list)
{
    SkyList *points = list->points();
    bool isVisible, isVisibleLast;
    SkyPoint* pLast = points->last();
    Vector2f  oLast = m_sm->toScreenVec( pLast, true, &isVisibleLast );

    QVector<Vector2f> polygon;
    for ( int i = 0; i < points->size(); ++i ) {
        SkyPoint* pThis = points->at( i );
        Vector2f oThis = m_sm->toScreenVec( pThis, true, &isVisible );

        if ( isVisible && isVisibleLast ) {
            polygon << oThis;
        } else if ( isVisibleLast ) {
            Vector2f oMid = m_sm->clipLineVec( pLast, pThis );
            polygon << oMid;
        } else if ( isVisible ) {
            Vector2f oMid = m_sm->clipLineVec( pThis, pLast );
            polygon << oMid;
            polygon << oThis;
        }

        pLast = pThis;
        oLast = oThis;
        isVisibleLast = isVisible;
    }

    if ( polygon.size() ) {
        glDisable(GL_TEXTURE_2D);
        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(2,GL_FLOAT,0, polygon.data() );
        glDrawArrays(GL_POLYGON, 0, polygon.size() );
        glDisableClientState(GL_VERTEX_ARRAY);
    }
}

void SkyGLPainter::drawSkyPolyline(LineList* list, SkipList* skipList, LineListLabel* label)
{
    glDisable(GL_TEXTURE_2D);
    SkyList *points = list->points();
    bool isVisible = false;
    bool isVisibleLast = false;
    glBegin(GL_LINE_STRIP);
    for(int i = 0; i < points->size(); ++i) {
        SkyPoint* p = points->at(i);
        Vector2f vec = m_sm->toScreenVec(p,true,&isVisible);
        bool doSkip = (skipList ? skipList->skip(i) : false);
        if( doSkip ) {
            glEnd();
            glBegin(GL_LINE_STRIP);
        }
        glVertex2fv(vec.data());
        //FIXME: check whether this actually works when the labels are fixed.
        if ( isVisible && isVisibleLast && label ) {
            label->updateLabelCandidates(vec[0], vec[1], list, i);
        }
        isVisibleLast = isVisible;
    }
    glEnd();
}

void SkyGLPainter::drawSkyLine(SkyPoint* a, SkyPoint* b)
{

}

void SkyGLPainter::drawSkyBackground()
{
    glDisable(GL_TEXTURE_2D);
    QColor bg = KStarsData::Instance()->colorScheme()->colorNamed( "SkyColor" );
    glClearColor( bg.redF(), bg.greenF(), bg.blueF(), bg.alphaF() );
    glClear(GL_COLOR_BUFFER_BIT);
}

void SkyGLPainter::end()
{
    for(int i = 0; i < NUMTYPES; ++i) {
        drawBuffer(i);
    }
    /*
    glEnable(GL_TEXTURE_2D);
    Texture *tex = TextureManager::getTexture("star");
    tex->bind();
    glBegin(GL_QUADS);
    glTexCoord2f(0,0);
    glVertex2f(100,100);
    glTexCoord2f(1,0);
    glVertex2f(200,100);
    glTexCoord2f(1,1);
    glVertex2f(200,200);
    glTexCoord2f(0,1);
    glVertex2f(100,200);
    glEnd();
    glDisable(GL_TEXTURE_2D); */
}

void SkyGLPainter::begin()
{
    //Load ortho projection
    glViewport(0,0,m_sm->width(),m_sm->height());
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0,m_sm->width(), m_sm->height(),0, -1,1);

    //reset modelview matrix
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    //Set various parameters
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glPointSize(1.);
    glEnable(GL_POINT_SMOOTH);
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_POLYGON_SMOOTH);
    glLineStipple(1,0xCCCC);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    
}

void SkyGLPainter::setBrush(const QBrush& brush)
{
    /*
    QColor c = brush.color();
    m_pen = Vector4f( c.redF(), c.greenF(), c.blueF(), c.alphaF() );
    glColor4fv( m_pen.data() );
    */
}

void SkyGLPainter::setPen(const QPen& pen)
{
    QColor c = pen.color();
    m_pen = Vector4f( c.redF(), c.greenF(), c.blueF(), c.alphaF() );
    glColor4fv( m_pen.data() );
    glLineWidth(pen.widthF());
    if( pen.style() != Qt::SolidLine ) {
        glEnable(GL_LINE_STIPPLE);
    } else {
        glDisable(GL_LINE_STIPPLE);
    }

}

