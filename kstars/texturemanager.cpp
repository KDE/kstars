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

#include <kstandarddirs.h>

#include "skymap.h"
#include "kstars.h"

TextureManager* TextureManager::m_p;

const Texture* TextureManager::getTexture(const QString& name)
{
    if(!m_p) {
        m_p = new TextureManager(KStars::Instance());
    }
    
    Texture *tex = m_p->m_textures.value(name,0);
    if( !tex ) {
        qDebug() << "Trying to load texture" << name;
        qDebug() << "Current textures loaded:";
        qDebug() << m_p->m_textures.keys();
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

void TextureManager::genTextures()
{
    //If there's no instance, there are no textures to bind!
    if(!m_p) return;
    
    for( QHash<QString, Texture*>::const_iterator it = m_p->m_textures.begin();
         it != m_p->m_textures.end();
         ++it )
    {
        qDebug() << (*it)->image().isNull();
        qDebug() << (*it)->image().size();
        qDebug() << (*it)->isReady();
        qDebug() << (*it)->m_tid;
        if( !(*it)->isReady() )
            (*it)->genTexture();
        qDebug() << (*it)->isReady();
        qDebug() << (*it)->m_tid;
    }
}

TextureManager::TextureManager(QObject* parent): QObject(parent)
{
    
}


