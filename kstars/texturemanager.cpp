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

#include "texturemanager.h"
#include "skymap.h"
#include "kstars.h"

#include <kstandarddirs.h>

#ifdef HAVE_OPENGL
#include <QGLWidget>
QGLContext* TextureManager::m_context = 0;
#endif

TextureManager* TextureManager::m_p;

const Texture* TextureManager::getTexture(const QString& name)
{

    Create();

    Texture *tex = m_p->m_textures.value(name,0);
    if( !tex ) {
        QString filename = KStandardDirs::locate("appdata",QString("textures/%1.png").arg(name));
        tex = new Texture(m_p);
        if( !filename.isNull() ) {
            QImage img(filename);
            tex->setImage(img);
        } else {
            qWarning() << "Could not find texture" << name;
        }
        m_p->m_textures.insert(name,tex);
    }
    return tex;
}

#ifdef HAVE_OPENGL
void TextureManager::genTextures()
{
    //If there's no instance, there are no textures to bind!
    if(!m_p) return;
    
    for( QHash<QString, Texture*>::const_iterator it = m_p->m_textures.constBegin();
         it != m_p->m_textures.constEnd();
         ++it )
    {
        if( !(*it)->isReady() )
            (*it)->genTexture();
    }
}
#endif

TextureManager *TextureManager::Create() {
    if( !m_p )
        m_p = new TextureManager( KStars::Instance() );
#ifdef HAVE_OPENGL
    if( !m_context )
        m_context = new QGLContext( QGLFormat(QGL::SampleBuffers) );
#endif
    return m_p;
}

Texture* TextureManager::createTexture( QImage image )
{
    int longest, tex_size = 2;
    Texture *texture = new Texture( m_p );

    // Resize image if necessary and create texture
    if ( image.width() != image.height() || ( image.width() & ( image.width() - 1 ) ) ) {
        // Compute texture size
        if ( image.width() > image.height() )
            longest = image.width();
        else
            longest = image.height();

        while ( tex_size < longest ) {
            tex_size *= 2;
        }

        texture->setImage( image.scaled( tex_size, tex_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation ) );
    } else
        texture->setImage( image );

    return texture;
}

TextureManager::TextureManager(QObject* parent): QObject(parent)
{

}
