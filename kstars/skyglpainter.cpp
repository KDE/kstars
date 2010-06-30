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

SkyGLPainter::SkyGLPainter(SkyMap* sm)
    : SkyPainter(sm)
{

}

void SkyGLPainter::drawAsPoint(SkyPoint* p)
{
    Vector3f vec = m_sm->toScreenVec(p);
    //FIXME: not so inefficient...
    glBegin(GL_POINTS);
    glVertex3fv(vec.data());
    glEnd();
}

bool SkyGLPainter::drawPlanet(KSPlanetBase* planet)
{
    return false;
}

bool SkyGLPainter::drawDeepSkyObject(DeepSkyObject* obj, bool drawImage)
{
    return false;
}

bool SkyGLPainter::drawPointSource(SkyPoint* loc, float mag, char sp)
{
    return false;
}

void SkyGLPainter::drawSkyPolygon(LineList* list)
{

}

void SkyGLPainter::drawSkyPolyline(LineList* list, SkipList* skipList, LineListLabel* label)
{

}

void SkyGLPainter::drawSkyLine(SkyPoint* a, SkyPoint* b)
{

}

void SkyGLPainter::drawSkyBackground()
{

}

void SkyGLPainter::end()
{

}

void SkyGLPainter::begin()
{
    //Set various parameters
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    //Load ortho projection
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0,m_sm->width(), m_sm->height(),0, -1,1);

    //reset modelview matrix
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    //fill with color
    glClearColor(1.,0.4,0.7,1);
    glClear(GL_COLOR_BUFFER_BIT);

    glColor3f(1.,0.3,0.6);
    glBegin(GL_TRIANGLES);
        glVertex3f( 100.,200., 0);
        glVertex3f( 221.0, 21.,0);
        glVertex3f( m_sm->width()/2., 200., 0);
    glEnd();
}

void SkyGLPainter::setBrush(const QBrush& brush)
{

}

void SkyGLPainter::setPen(const QPen& pen)
{

}

