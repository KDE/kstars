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

#include "texture.h"
#include "skymap.h"

Texture::Texture(QObject* parent): QObject(parent)
{
    m_ready = false;
    m_tid = 0;
}

bool Texture::bind() const
{

    #ifdef USEGL
    if( m_ready ) {
        glBindTexture(GL_TEXTURE_2D, m_tid);
        return true;
    } else
    #endif
        return false;
}

const QImage& Texture::image() const
{
    return m_image;
}

bool Texture::isReady() const
{
    return m_ready;
}

void Texture::setImage(const QImage& img)
{
    m_image = img;
    m_ready = true;
    genTexture();
}

void Texture::genTexture()
{
    //FIXME do proper mipmapping
    #ifdef USEGL
    if( SkyMap::Instance() && !m_image.isNull() ) {
        m_tid = SkyMap::Instance()->bindTexture(m_image, GL_TEXTURE_2D, GL_RGBA, QGLContext::DefaultBindOption);
        m_ready = (m_tid != 0);
    } else
        m_ready = false;
    #endif
}



