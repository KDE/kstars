/*
    SPDX-FileCopyrightText: 2010 Henry de Valence <hdevalence@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QHash>

#include <config-kstars.h>

class QGLWidget;
class QImage;

/**
 * @brief a singleton class to manage texture loading/retrieval
 */
class TextureManager : public QObject
{
    Q_OBJECT
  public:
    /** @short Create the instance of TextureManager */
    static TextureManager *Create();
    /** @short Release the instance of TextureManager */
    static void Release();

    /**
     * Return texture image. If image is not found in cache tries to
     * load it from disk if that fails too returns reference to empty image.
     */
    static const QImage &getImage(const QString &name);

    /**
     * Clear the cache and discover the directories to load textures from.
     */
    static void discoverTextureDirs();
#ifdef HAVE_OPENGL
    /**
     *  Bind OpenGL texture. Acts similarly to getImage but does
     *  nothing if image is not found in the end
     */
    static void bindTexture(const QString &name, QGLWidget *cxt);

    /** Bind OpenGL texture using QImage as source */
    static void bindFromImage(const QImage &image, QGLWidget *cxt);
#endif

  private:
    /** Shorthand for iterator to hashtable */
    typedef QHash<QString, QImage>::const_iterator CacheIter;

    /** Private constructor */
    explicit TextureManager(QObject *parent = nullptr);
    /** Try find image in the cache and then to load it from disk if it's not found */
    static CacheIter findTexture(const QString &name);

    // Pointer to singleton instance
    static TextureManager *m_p;
    // List of named textures
    QHash<QString, QImage> m_textures;

    // List of texture directories
    std::vector<QString> m_texture_directories;

    // Prohibit copying
    TextureManager(const TextureManager &);
    TextureManager &operator=(const TextureManager &);
};
