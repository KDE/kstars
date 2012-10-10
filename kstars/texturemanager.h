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

#ifndef TEXTUREMANAGER_H
#define TEXTUREMANAGER_H

#include <QObject>
#include <QHash>

#include <config-kstars.h>

class QGLWidget;
class QImage;

/** @brief a singleton class to manage texture loading/retrieval
 */
class TextureManager : public QObject
{
    Q_OBJECT
public:
    /** @short Create the instance of TextureManager */
    static TextureManager* Create();

    /** Return texture image. If image is not found in cache tries to
     *  load it from disk if that fails too returns reference to empty
     *  image. */
    static const QImage& getImage(const QString& name);

#ifdef HAVE_OPENGL
    /** Bind OpenGL texture. Acts similarly to getImage but does
     *  nothing if image is not found in the end */
    static void bindTexture(const QString& name, QGLWidget* cxt);

    /** Bind OpenGL texture using QImage as source */
    static void bindFromImage(const QImage& image, QGLWidget* cxt);
#endif

private:
    /** Shorthand for iterator to hashtable */
    typedef QHash<QString,QImage>::const_iterator CacheIter;

    /** Private constructor */
    TextureManager(QObject* parent = 0);
    /** Try find image in the cache and then to load it from disk if
     *  it's not found */
    static CacheIter findTexture(const QString& name);


    // Pointer to singleton instance
    static TextureManager* m_p;
    // List of named textures
    QHash<QString,QImage> m_textures;

    // Prohibit copying
    TextureManager(const TextureManager&);
    TextureManager& operator = (const TextureManager&);
};

#endif // TEXTUREMANAGER_H
