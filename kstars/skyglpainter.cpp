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

#include "skycomponents/linelist.h"
#include "skycomponents/skiplist.h"
#include "skycomponents/linelistlabel.h"

#include "skyobjects/deepskyobject.h"
#include "skyobjects/kscomet.h"
#include "skyobjects/ksasteroid.h"
#include "skyobjects/trailobject.h"

SkyGLPainter::SkyGLPainter(SkyMap* sm)
    : SkyPainter(sm)
{

}

void SkyGLPainter::drawAsPoint(SkyPoint* p)
{
    Vector3f vec = m_sm->toScreenVec(p);
    //FIXME: not so inefficient...
    glBegin(GL_POINTS);
    glColor3f(1.,1.,1.);
    glVertex3fv(vec.data());
    glEnd();
}

bool SkyGLPainter::drawPlanet(KSPlanetBase* planet)
{
    drawAsPoint(planet);
    return true;
}

bool SkyGLPainter::drawDeepSkyObject(DeepSkyObject* obj, bool drawImage)
{
    drawAsPoint(obj);
    return true;
}

bool SkyGLPainter::drawPointSource(SkyPoint* loc, float mag, char sp)
{
    drawAsPoint(loc);
    return true;
}

void SkyGLPainter::drawSkyPolygon(LineList* list)
{
    SkyList *points = list->points();
    glBegin(GL_POLYGON);
    for(int i = 0; i < points->size(); ++i) {
        glVertex3fv(m_sm->toScreenVec(points->at(i)).data());
    }
    glEnd();
}

void SkyGLPainter::drawSkyPolyline(LineList* list, SkipList* skipList, LineListLabel* label)
{
    SkyList *points = list->points();
    bool isVisible = false;
    bool isVisibleLast = false;
    glBegin(GL_LINE_STRIP);
    for(int i = 0; i < points->size(); ++i) {
        SkyPoint* p = points->at(i);
        Vector3f vec = m_sm->toScreenVec(p,true,&isVisible);
        bool doSkip = (skipList ? skipList->skip(i) : false);
        if( !doSkip ) {
            glVertex3fv(vec.data());
            //FIXME: check whether this actually works when the labels are fixed.
            if ( isVisible && isVisibleLast && label ) {
                label->updateLabelCandidates(vec[0], vec[1], list, i);
            }
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
    QColor bg = KStarsData::Instance()->colorScheme()->colorNamed( "SkyColor" );
    glClearColor( bg.redF(), bg.greenF(), bg.blueF(), bg.alphaF() );
    glClear(GL_COLOR_BUFFER_BIT);
}

void SkyGLPainter::end()
{

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
    glEnable(GL_POINT_SMOOTH);
    glPointSize(3.);
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
}

