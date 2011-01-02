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

#ifndef KSTEXTUREMANAGER_H
#define KSTEXTUREMANAGER_H

#include <QObject>
#include <QHash>

#include "texture.h"

#include <config-kstars.h>

class QGLContext;

/** @brief a singleton class to manage texture loading/retrieval */
class TextureManager : public QObject
{
    Q_OBJECT
public:
    /** Gets and/or loads a texture
        @param name the name of the texture
        @return a pointer to the texture
        */
    static const Texture* getTexture(const QString& name);
    /** If there exist textures that have a QImage loaded
        but which have not yet been set up for use with GL,
        this function will set them up. */
    static void genTextures();

    /**
     *@return the QGLContext that is used for the textures
     */
    static inline QGLContext* getContext() { return (m_p ? m_p->m_context : 0); }

    /**
     *@short Create the instance of TextureManager
     */
    static TextureManager *Create();

protected:
    TextureManager(QObject* parent = 0);
    static TextureManager* m_p;
    QHash<QString,Texture*> m_textures;
    static QGLContext *m_context; // GL Context to bind textures to.
};

#endif // KSTEXTUREMANAGER_H
