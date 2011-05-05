/*
    Copyright (C) 2010 Henry de Valence <hdevalence@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY
    or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*/

#include "texturemanager.h"
#include "skymap.h"
#include "kstars.h"

#include <kstandarddirs.h>

#ifdef HAVE_OPENGL
# include <QGLWidget>
#endif

// We returning reference to image. We refer to this image when search
// for image fails
const static QImage emptyImage;

TextureManager* TextureManager::m_p;


TextureManager *TextureManager::Create() {
    if( !m_p )
        m_p = new TextureManager();
    return m_p;
}

const QImage& TextureManager::getImage(const QString& name)
{
    Create();
    CacheIter it = findTexture( name );
    if( it != m_p->m_textures.end() ) {
        return *it;
    } else {
        return emptyImage;
    }
}

// FIXME: should be cache images which are not found?
TextureManager::CacheIter TextureManager::findTexture(const QString& name)
{
    Create();
    // Lookup in cache first
    CacheIter it = m_p->m_textures.find( name );
    if( it != m_p->m_textures.end() ) {
        return it;
    } else {
        // Try to load from file
        QString filename = KStandardDirs::locate("appdata",QString("textures/%1.png").arg(name));
        if( !filename.isNull() ) {
            return m_p->m_textures.insert( name, QImage(filename) );
        } else {
            return m_p->m_textures.end();
        }
    }
}

#ifdef HAVE_OPENGL
static void bindImage(const QImage& img, QGLWidget* cxt)
{
    GLuint tid = cxt->bindTexture(img, GL_TEXTURE_2D, GL_RGBA, QGLContext::DefaultBindOption);
    glBindTexture(GL_TEXTURE_2D, tid);
}

// FIXME: should we check that image have appropriate size as bindFromImage do?
void TextureManager::bindTexture(const QString& name, QGLWidget* cxt)
{
    Create();
    Q_ASSERT( "Must be called only with valid GL context" && cxt );

    CacheIter it = findTexture( name );
    if( it != m_p->m_textures.end() )
        bindImage( *it, cxt );
}

void TextureManager::bindFromImage(const QImage& image, QGLWidget* cxt)
{
    Create();
    Q_ASSERT( "Must be called only with valid GL context" && cxt );

    if ( image.width() != image.height() || ( image.width() & ( image.width() - 1 ) ) ) {
        // Compute texture size
        int longest  = qMax( image.width(), image.height() );
        int tex_size = 2;
        while ( tex_size < longest ) {
            tex_size *= 2;
        }
        // FIXME: Check if Qt does this for us already. [Note that it does scale to the nearest power of two]
        bindImage( 
            image.scaled( tex_size, tex_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation ),
            cxt );
    } else {
        bindImage( image, cxt );
    }
}
#endif

TextureManager::TextureManager(QObject* parent): QObject(parent)
{}
