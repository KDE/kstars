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
USING_PART_OF_NAMESPACE_EIGEN

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

Vector2f SkyGLPainter::m_vertex[(int)SkyObject::TYPE_UNKNOWN][6*BUFSIZE];
Vector2f SkyGLPainter::m_texcoord[(int)SkyObject::TYPE_UNKNOWN][6*BUFSIZE];
Vector3f SkyGLPainter::m_color[(int)SkyObject::TYPE_UNKNOWN][6*BUFSIZE];
int      SkyGLPainter::m_idx[SkyObject::TYPE_UNKNOWN];

SkyGLPainter::SkyGLPainter(SkyMap* sm)
    : SkyPainter(sm)
{
    for(int i = 0; i < SkyObject::TYPE_UNKNOWN; ++i) {
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
}

void SkyGLPainter::drawBuffer(int type)
{
    //printf("Drawing buffer for type %d, has %d objects\n", type, m_idx[type]);
    if( m_idx[type] == 0 ) return;

    glEnable(GL_TEXTURE_2D);
    Texture *tex = TextureManager::getTexture("star");
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

void SkyGLPainter::addItem(SkyPoint* p, int type, float width, char sp)
{
    bool visible = false;
    Vector2f vec = m_sm->toScreenVec(p,true,&visible);
    if(!visible) return;
    
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

    Vector3f c(0.,1.,1.);
    switch(sp) {
        case 'o': case 'O': c = Vector3f( 0.,  0.,  1.); break;
        case 'b': case 'B': c = Vector3f( 0., 0.8,  1.); break;
        case 'a': case 'A': c = Vector3f( 0.,  1.,  1.); break;
        case 'f': case 'F': c = Vector3f(0.8,  1., 0.4); break;
        case 'g': case 'G': c = Vector3f( 1.,  1.,  0.); break;
        case 'k': case 'K': c = Vector3f( 1., 0.4,  0.); break;
        case 'm': case 'M': c = Vector3f( 1.,  0.,  0.); break;
    }

    for(int j = 0; j < 6; ++j) {
        m_color[type][i+j] = c;
    }
    
    ++m_idx[type];
}

bool SkyGLPainter::drawPlanet(KSPlanetBase* planet)
{
    float fakeStarSize = ( 10.0 + log10( Options::zoomFactor() ) - log10( MINZOOM ) ) * ( 10 - planet->mag() ) / 10;
    addItem(planet, planet->type(), qMin(fakeStarSize,(float)20.));
    return true;
}

bool SkyGLPainter::drawDeepSkyObject(DeepSkyObject* obj, bool drawImage)
{
    addItem(obj, obj->type(), starWidth(obj->mag()));
    return true;
}

bool SkyGLPainter::drawPointSource(SkyPoint* loc, float mag, char sp)
{
    addItem(loc, SkyObject::STAR, starWidth(mag), sp);
    return true;
}

void SkyGLPainter::drawSkyPolygon(LineList* list)
{
    glDisable(GL_TEXTURE_2D);
    SkyList *points = list->points();
    glBegin(GL_POLYGON);
    for(int i = 0; i < points->size(); ++i) {
        glVertex2fv(m_sm->toScreenVec(points->at(i)).data());
    }
    glEnd();
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
    for(int i = 0; i < SkyObject::TYPE_UNKNOWN; ++i) {
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
    QColor c = brush.color();
    glColor4f( c.redF(), c.greenF(), c.blueF(), c.alphaF() );
}

void SkyGLPainter::setPen(const QPen& pen)
{
    QColor c = pen.color();
    glColor4f( c.redF(), c.greenF(), c.blueF(), c.alphaF() );
    glLineWidth(pen.widthF());
    if( pen.style() != Qt::SolidLine ) {
        glEnable(GL_LINE_STIPPLE);
    } else {
        glDisable(GL_LINE_STIPPLE);
    }

}

