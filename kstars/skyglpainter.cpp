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

#include <cstddef>
#include <Eigen/Core>
#include <Eigen/Geometry>
USING_PART_OF_NAMESPACE_EIGEN
using Eigen::Rotation2Df;

#include <GL/gl.h>
#include <QGLWidget>

#include "skymap.h"
#include "kstarsdata.h"
#include "Options.h"

#include "texturemanager.h"

#include "skycomponents/linelist.h"
#include "skycomponents/skiplist.h"
#include "skycomponents/linelistlabel.h"
#include "skycomponents/skymapcomposite.h"
#include "skycomponents/flagcomponent.h"
#include "skycomponents/satellitescomponent.h"

#include "skyobjects/deepskyobject.h"
#include "skyobjects/kscomet.h"
#include "skyobjects/ksasteroid.h"
#include "skyobjects/trailobject.h"
#include "skyobjects/satellite.h"

Vector2f SkyGLPainter::m_vertex[NUMTYPES][6*BUFSIZE];
Vector2f SkyGLPainter::m_texcoord[NUMTYPES][6*BUFSIZE];
Vector3f SkyGLPainter::m_color[NUMTYPES][6*BUFSIZE];
int      SkyGLPainter::m_idx[NUMTYPES];
bool     SkyGLPainter::m_init = false;


                                 
SkyGLPainter::SkyGLPainter( QGLWidget *widget ) :
                                     SkyPainter()
{
    m_widget = widget;
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
        //Generate textures that were loaded before the SkyMap was
        m_init = true;
    }
}

void SkyGLPainter::drawBuffer(int type)
{
    //printf("Drawing buffer for type %d, has %d objects\n", type, m_idx[type]);
    if( m_idx[type] == 0 ) return;

    glEnable(GL_TEXTURE_2D);
    switch( type ) {
        case 3: case 13:          TextureManager::bindTexture("open-cluster",     m_widget); break;
        case 4:                   TextureManager::bindTexture("globular-cluster", m_widget); break;
        case 6:                   TextureManager::bindTexture("planetary-nebula", m_widget); break;
        case 5: case 7: case 15:  TextureManager::bindTexture("gaseous-nebula",   m_widget); break;
        case 8: case 16:          TextureManager::bindTexture("galaxy",           m_widget); break;
        case 14:                  TextureManager::bindTexture("galaxy-cluster",   m_widget); break;
        case 0: case 1: default:  TextureManager::bindTexture("star",             m_widget); break;
    }

    glBlendFunc(GL_ONE, GL_ONE);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
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
    Vector2f vec = m_proj->toScreenVec(p,true,&visible);
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

void SkyGLPainter::drawTexturedRectangle( const QImage& img,
                                          const Vector2f& pos, const float angle,
                                          const float sizeX,   const float sizeY )
{
    // Set up texture
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glEnable(GL_TEXTURE_2D);
    TextureManager::bindFromImage( img, m_widget );

    // Render rectangle
    glPushMatrix();
    glTranslatef( pos.x(), pos.y(), 0 );
    glRotatef( angle, 0, 0, 1 );
    glScalef( sizeX, sizeY, 1 );

    glBegin(GL_QUADS);
        // Note! Y coordinate of texture is mirrored w.r.t. to
        // vertex coordinates to account for difference between
        // OpenGL and QPainter coordinate system.
        // Otherwise image would appear mirrored
        glTexCoord2f( 0,    1   );
        glVertex2f(  -0.5, -0.5 );

        glTexCoord2f( 1,    1   );
        glVertex2f(   0.5, -0.5 );

        glTexCoord2f( 1,    0   );
        glVertex2f(   0.5,  0.5 );

        glTexCoord2f( 0,    0   );
        glVertex2f(  -0.5,  0.5 );
    glEnd();

    glPopMatrix();
}

bool SkyGLPainter::drawPlanet(KSPlanetBase* planet)
{
    //If it's surely not visible, just stop now
    if( !m_proj->checkVisibility(planet) )
        return false;

    float zoom = Options::zoomFactor();
    float fakeStarSize = ( 10.0 + log10( zoom ) - log10( MINZOOM ) ) * ( 10 - planet->mag() ) / 10;
    fakeStarSize = qMin(fakeStarSize, 20.f);

    float size = planet->angSize() * dms::PI * zoom/10800.0;
    if( size < fakeStarSize || planet->image().isNull() )
    {
        // Draw them as bright stars of appropriate color instead of images
        char spType;
        //FIXME: do these need i18n?
        if( planet->name() == i18n( "Sun" ) ) {
            spType = 'G';
        } else if( planet->name() == i18n("Mars") ) {
            spType = 'K';
        } else if( planet->name() == i18n("Jupiter") || planet->name() == i18n("Mercury") || planet->name() == i18n("Saturn") ) {
            spType = 'F';
        } else {
            spType = 'B';
        }
        return addItem(planet, planet->type(), qMin(fakeStarSize,(float)20.),spType);
    } else {
        // Draw them as textures
        bool visible = false;
        Vector2f pos = m_proj->toScreenVec(planet,true,&visible);
        if( !visible )
            return false;

        //Because Saturn has rings, we inflate its image size by a factor 2.5
        if( planet->name() == "Saturn" )
            size *= 2.5;

        drawTexturedRectangle( planet->image(),
                               pos, m_proj->findPA(planet, pos.x(), pos.y()),
                               size, size );
        return true;
    }
}

bool SkyGLPainter::drawDeepSkyObject(DeepSkyObject* obj, bool drawImage)
{
    //If it's surely not visible, just stop now
    if( !m_proj->checkVisibility(obj) )
        return false;
    int type = obj->type();
    //addItem(obj, type, obj->a() * dms::PI * Options::zoomFactor() / 10800.0);
    
    //If it's a star, add it like a star
    if( type < 2 )
        return addItem(obj, type, starWidth(obj->mag()));

    bool visible = false;
    Vector2f vec = m_proj->toScreenVec(obj,true,&visible);
    if(!visible)
        return false;

    float width = obj->a() * dms::PI * Options::zoomFactor() / 10800.0;
    float pa = m_proj->findPA(obj, vec[0], vec[1]) * (M_PI/180.0);
    Rotation2Df r(pa);
    float w = width/2.;
    float h = w * obj->e();

    //Init texture if it doesn't exist and we would be drawing it anyways
    if( drawImage  &&  !obj->image().isNull() ) {
        drawTexturedRectangle( obj->image(), vec, pa, w, h);
    } else {
        //If the buffer is full, flush it
        if(m_idx[type] == BUFSIZE)
            drawBuffer(type);

        const int i = 6*m_idx[type];
        m_vertex[type][i + 0] = vec + r* Vector2f(-w,-h);
        m_vertex[type][i + 1] = vec + r* Vector2f( w,-h);
        m_vertex[type][i + 2] = vec + r* Vector2f(-w, h);
        m_vertex[type][i + 3] = vec + r* Vector2f(-w, h);
        m_vertex[type][i + 4] = vec + r* Vector2f( w,-h);
        m_vertex[type][i + 5] = vec + r* Vector2f( w, h);
        Vector3f c( m_pen[0], m_pen[1], m_pen[2] );

        for(int j = 0; j < 6; ++j)
            m_color[type][i+j] = c;
        
        ++m_idx[type];
    }
    return true;
}

bool SkyGLPainter::drawPointSource(SkyPoint* loc, float mag, char sp)
{
    //If it's surely not visible, just stop now
    if( !m_proj->checkVisibility(loc) ) return false;
    return addItem(loc, SkyObject::STAR, starWidth(mag), sp);
}

void SkyGLPainter::drawSkyPolygon(LineList* list)
{
    SkyList *points = list->points();
    bool isVisible, isVisibleLast;
    SkyPoint* pLast = points->last();
    Vector2f  oLast = m_proj->toScreenVec( pLast, true, &isVisibleLast );
    // & with the result of checkVisibility to clip away things below horizon
    isVisibleLast &= m_proj->checkVisibility(pLast);

    //Guess that we will require around the same number of items as in points.
    QVector<Vector2f> polygon;
    polygon.reserve(points->size());
    for ( int i = 0; i < points->size(); ++i ) {
        SkyPoint* pThis = points->at( i );
        Vector2f oThis = m_proj->toScreenVec( pThis, true, &isVisible );
        // & with the result of checkVisibility to clip away things below horizon
        isVisible &= m_proj->checkVisibility(pThis);

        if ( isVisible && isVisibleLast ) {
            polygon << oThis;
        } else if ( isVisibleLast ) {
            Vector2f oMid = m_proj->clipLineVec( pLast, pThis );
            polygon << oMid;
        } else if ( isVisible ) {
            Vector2f oMid = m_proj->clipLineVec( pThis, pLast );
            polygon << oMid;
            polygon << oThis;
        }

        pLast = pThis;
        oLast = oThis;
        isVisibleLast = isVisible;
    }

    #define MAKE_KSTARS_SLOW TRUE
    if ( polygon.size() ) {
        #ifdef MAKE_KSTARS_SLOW
        drawPolygon(polygon, false);
        #else
        //Assume convexity
        drawPolygon(polygon, true);
        #endif
    }
}

void SkyGLPainter::drawPolygon(const QVector<Vector2f>& polygon, bool convex, bool flush_buffers)
{
    //Flush all buffers
    if( flush_buffers ) {
        for(int i = 0; i < NUMTYPES; ++i) {
            drawBuffer(i);
        }
    }
    glDisable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if( !convex ) {
        //Set up the stencil buffer and disable the color buffer
        glClear(GL_STENCIL_BUFFER_BIT);
        glColorMask(0,0,0,0);
        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_ALWAYS, 0, 0);
        glStencilOp(GL_INVERT, GL_INVERT, GL_INVERT);
        //Now draw a triangle fan. Because of the GL_INVERT,
        //this will fill the stencil buffer with odd-even fill.
        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(2,GL_FLOAT,0, polygon.data() );
        glDrawArrays(GL_TRIANGLE_FAN, 0, polygon.size());
        glDisableClientState(GL_VERTEX_ARRAY);

        //Now draw the stencil:
        glStencilFunc(GL_NOTEQUAL, 0, 0xffffffff);
        glColorMask(1,1,1,1);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        glBegin(GL_QUADS);
        glVertex2f(0,0);
        glVertex2f(0,m_widget->height());
        glVertex2f(m_widget->width(),m_widget->height());
        glVertex2f(m_widget->width(),0);
        glEnd();
        glDisable(GL_STENCIL_TEST);
    } else {
        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(2,GL_FLOAT,0, polygon.data() );
        glDrawArrays(GL_POLYGON, 0, polygon.size());
        glDisableClientState(GL_VERTEX_ARRAY);
    }
}

void SkyGLPainter::drawHorizon(bool filled, SkyPoint* labelPoint, bool* drawLabel)
{
    QVector<Vector2f> ground = m_proj->groundPoly( labelPoint, drawLabel );

    if( ground.size() ) {
        if( filled ) {
            glDisableClientState( GL_COLOR_ARRAY );
            drawPolygon( ground, false, false );
        } else {
            glDisable( GL_TEXTURE_2D );
            glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
            glEnableClientState( GL_VERTEX_ARRAY );
            glVertexPointer( 2, GL_FLOAT, 0, ground.data() );
            glDrawArrays( GL_LINE_LOOP, 0, ground.size() );
            glDisableClientState( GL_VERTEX_ARRAY );
        }
    }
}

//This implementation is *correct* but slow.
void SkyGLPainter::drawSkyPolyline(LineList* list, SkipList* skipList, LineListLabel* label)
{
    glDisable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_LINE_STRIP);
    SkyList *points = list->points();
    bool isVisible, isVisibleLast;
    Vector2f oLast = m_proj->toScreenVec(points->first(), true, &isVisibleLast);
    // & with the result of checkVisibility to clip away things below horizon
    isVisibleLast &= m_proj->checkVisibility(points->first());
    if( isVisibleLast ) { glVertex2fv(oLast.data()); }

    for(int i = 1; i < points->size(); ++i) {
        Vector2f oThis = m_proj->toScreenVec( points->at(i), true, &isVisible);
        // & with the result of checkVisibility to clip away things below horizon
        isVisible &= m_proj->checkVisibility(points->at(i));

        bool doSkip = (skipList ? skipList->skip(i) : false);
        //This tells us whether we need to end the current line or whether we
        //are to keep drawing into it. If we skip, then we are going to have to end.
        bool shouldEnd = doSkip;

        if( !doSkip ) {
            if( isVisible && isVisibleLast ) {
                glVertex2fv(oThis.data());
                if( label ) {
                    label->updateLabelCandidates(oThis.x(), oThis.y(), list, i);
                }
            } else if( isVisibleLast ) {
                Vector2f oMid = m_proj->clipLineVec( points->at(i-1), points->at(i) );
                glVertex2fv(oMid.data());
                //If the last point was visible but this one isn't we are at
                //the end of a strip, so we need to end
                shouldEnd = true;
            } else if( isVisible ) {
                Vector2f oMid = m_proj->clipLineVec( points->at(i), points->at(i-1) );
                glVertex2fv(oMid.data());
                glVertex2fv(oThis.data());
            }
        }

        if(shouldEnd) {
            glEnd();
            glBegin(GL_LINE_STRIP);
            if( isVisible ) {
                glVertex2fv(oThis.data());
            }
        }

        isVisibleLast = isVisible;
    }
    glEnd();
}

//FIXME: implement these two

void SkyGLPainter::drawObservingList(const QList< SkyObject* >& obs)
{

    // TODO: Generalize to drawTargetList or something like that. Make
    // texture changeable etc.
    // TODO: Draw labels when required

    QVector<Vector2f> buffer( 6*obs.size() );
    int i = 0;
    foreach( SkyObject *obj, obs ) {
        if( !m_proj->checkVisibility(obj) ) continue;
        bool visible;
        Vector2f vec = m_proj->toScreenVec(obj, true, &visible);
        if( !visible || !m_proj->onScreen(vec) ) continue;

        const float w = 16.;
        const float h = 16.;
        
        buffer[i + 0] = vec + Vector2f(-w,-h);
        buffer[i + 1] = vec + Vector2f( w,-h);
        buffer[i + 2] = vec + Vector2f(-w, h);
        buffer[i + 3] = vec + Vector2f(-w, h);
        buffer[i + 4] = vec + Vector2f( w,-h);
        buffer[i + 5] = vec + Vector2f( w, h);

        ++i;
    }

    TextureManager::bindTexture("obslistsymbol", m_widget);

    glBlendFunc(GL_ONE, GL_ONE);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glVertexPointer  (2,GL_FLOAT,0, buffer.data());
    glTexCoordPointer(2,GL_FLOAT,0, &m_texcoord[0]);

    glDrawArrays(GL_TRIANGLES, 0, 6*i);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

void SkyGLPainter::drawFlags()
{
    KStarsData *data = KStarsData::Instance();
    SkyPoint* point;
    QImage image;
    const QString label;
    bool visible = false;
    Vector2f vec;
    int i;

    for ( i=0; i<data->skyComposite()->flags()->size(); i++ ) {
        point = data->skyComposite()->flags()->pointList().at( i );
        image = data->skyComposite()->flags()->image( i );

        // Set Horizontal coordinates
        point->EquatorialToHorizontal( data->lst(), data->geo()->lat() );

        // Get flag position on screen
        vec = m_proj->toScreenVec( point, true, &visible );

        // Return if flag is not visible
        if( !visible || !m_proj->onScreen( vec ) )
            continue;

        const QImage& img = data->skyComposite()->flags()->imageName( i ) == "Default"
                          ? TextureManager::getImage("defaultflag")
                          : image;

        drawTexturedRectangle( img, vec, 0, img.width(), img.height() );
        drawText( vec.x(), vec.y(),
                  data->skyComposite()->flags()->label( i ),
                  QFont( "Courier New", 10, QFont::Bold ),
                  data->skyComposite()->flags()->labelColor( i ) );
    }
}

void SkyGLPainter::drawText( int x, int y, const QString text, QFont font, QColor color )
{
    // Return if text is empty
    if ( text.isEmpty() )
        return;

    int longest, tex_size = 2;
    
    // Get size of text
    QFontMetrics fm( font );
    const QRect bounding_rect = fm.boundingRect( text );

    // Compute texture size
    if ( bounding_rect.width() > bounding_rect.height() )
        longest = bounding_rect.width();
    else
        longest = bounding_rect.height();

    while ( tex_size < longest ) {
        tex_size *= 2;
    }

    // Create image of text
    QImage text_image( tex_size, tex_size, QImage::Format_ARGB32 );
    text_image.fill( Qt::transparent );
    QPainter p( &text_image );
    p.setFont( font );
    p.setPen( color );
    p.drawText( 0, tex_size/2, text );
    p.end();

    // Create texture
    float w = text_image.width();
    float h = text_image.height();
    float vx = x + 0.5*w + 10;
    float vy = y - 10;
    drawTexturedRectangle( text_image, Vector2f(vx,vy), 0, w, h );
}

void SkyGLPainter::drawSkyLine(SkyPoint* a, SkyPoint* b)
{
    bool aVisible, bVisible;
    Vector2f aScreen = m_proj->toScreenVec( a, true, &aVisible );
    Vector2f bScreen = m_proj->toScreenVec( b, true, &bVisible );

    glDisable( GL_TEXTURE_2D );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    glBegin( GL_LINE_STRIP );
    
    //THREE CASES:
    if( aVisible && bVisible ) {
        //Both a,b visible, so paint the line normally:
        glVertex2fv( aScreen.data() );
        glVertex2fv( bScreen.data() );
    } else if( aVisible ) {
        //a is visible but b isn't:
        glVertex2fv( aScreen.data() );
        glVertex2fv( m_proj->clipLineVec( a, b ).data() );
    } else if( bVisible ) {
        //b is visible but a isn't:
        glVertex2fv( bScreen.data() );
        glVertex2fv( m_proj->clipLineVec( b, a ).data() );
    } //FIXME: what if both are offscreen but the line isn't?

    glEnd();
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
}

void SkyGLPainter::begin()
{
    m_proj = m_sm->projector();
    
    //Load ortho projection
    glViewport(0,0,m_widget->width(),m_widget->height());
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0,m_widget->width(), m_widget->height(),0, -1,1);

    //reset modelview matrix
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    //Set various parameters
    glDisable(GL_LIGHTING);
    glDisable(GL_COLOR_MATERIAL);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glPointSize(1.);
    glEnable(GL_POINT_SMOOTH);
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_POLYGON_SMOOTH);
    glLineStipple(1,0xCCCC);
    glEnable(GL_BLEND);
    
    glClearStencil(0);
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

void SkyGLPainter::drawSatellite( Satellite* sat ) {
    KStarsData *data = KStarsData::Instance();
    bool visible = false;
    Vector2f pos, vertex;

    sat->HorizontalToEquatorial( data->lst(), data->geo()->lat() );

    pos = m_proj->toScreenVec( sat, true, &visible );

    if( !visible || !m_proj->onScreen( pos ) )
        return;

    if ( Options::drawSatellitesLikeStars() ) {
        drawPointSource(sat, 3.5, 'B');
    } else {
        if ( sat->isVisible() )
            setPen( data->colorScheme()->colorNamed( "VisibleSatColor" ) );
        else
            setPen( data->colorScheme()->colorNamed( "SatColor" ) );

        glDisable( GL_TEXTURE_2D );
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
        glBegin( GL_QUADS );

        vertex = pos + Vector2f( -1.0, -1.0 );
        glVertex2fv(vertex.data());
        vertex = pos + Vector2f(  1.0, -1.0 );
        glVertex2fv(vertex.data());
        vertex = pos + Vector2f(  1.0,  1.0 );
        glVertex2fv(vertex.data());
        vertex = pos + Vector2f( -1.0,  1.0 );
        glVertex2fv(vertex.data());

        glEnd();
    }

    if ( Options::showSatellitesLabels() )
        data->skyComposite()->satellites()->drawLabel( sat, QPointF( pos.x(), pos.y() ) );
}
