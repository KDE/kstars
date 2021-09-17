/*
    SPDX-FileCopyrightText: 2010 Henry de Valence <hdevalence@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "texturemanager.h"

#include "kspaths.h"
#include "auxiliary/kspaths.h"

#ifdef KSTARS_LITE
#include <QStandardPaths>
#include <QImage>
#else
#include "skymap.h"
#include "kstars.h"
#endif

#ifdef HAVE_OPENGL
#include <QGLWidget>
#endif

// We returning reference to image. We refer to this image when search
// for image fails
const static QImage emptyImage;

TextureManager *TextureManager::m_p = nullptr;

TextureManager *TextureManager::Create()
{
    if (!m_p)
    {
        m_p = new TextureManager();
        discoverTextureDirs();
    }
    return m_p;
}

void TextureManager::Release()
{
    delete m_p;
    m_p = nullptr;
}

const QImage &TextureManager::getImage(const QString &name)
{
    Create();
    if (name.isEmpty())
        return emptyImage;
    CacheIter it = findTexture(name);
    if (it != m_p->m_textures.constEnd())
    {
        return *it;
    }
    else
    {
        return emptyImage;
    }
}

TextureManager::CacheIter TextureManager::findTexture(const QString &name)
{
    Create();
    // Lookup in cache first
    CacheIter it = m_p->m_textures.constFind(name);
    if (it != m_p->m_textures.constEnd())
    {
        return it;
    }

    for (const auto &dir : m_p->m_texture_directories)
    {
        const auto &filename = QString("%1/%2.png").arg(dir).arg(name);
        QFile file{ filename };
        if (file.exists())
            return (TextureManager::CacheIter)m_p->m_textures.insert(
                name, QImage(filename, "PNG"));
    }

    //Try to load from the file in 'skycultures/western' subdirectory for western constellation art
    QString filename = KSPaths::locate(QStandardPaths::AppDataLocation,
                                       QString("skycultures/western/%1.png").arg(name));
    if (!filename.isNull())
    {
        return (TextureManager::CacheIter)m_p->m_textures.insert(name,
                                                                 QImage(filename, "PNG"));
    }

    //Try to load from the file in 'skycultures/inuit' subdirectory for Inuit constellation art
    filename = KSPaths::locate(QStandardPaths::AppDataLocation,
                               QString("skycultures/inuit/%1.png").arg(name));
    if (!filename.isNull())
    {
        return (TextureManager::CacheIter)m_p->m_textures.insert(name,
                                                                 QImage(filename, "PNG"));
    }

    // Try to load from file in main data directory
    filename = KSPaths::locate(QStandardPaths::AppDataLocation,
                               QString("textures/%1.png").arg(name));

    if (!filename.isNull())
    {
        return (TextureManager::CacheIter)m_p->m_textures.insert(name,
                                                                 QImage(filename, "PNG"));
    }

    return (TextureManager::CacheIter)m_p->m_textures.insert(name, QImage());
}

#ifdef HAVE_OPENGL
static void bindImage(const QImage &img, QGLWidget *cxt)
{
    GLuint tid = cxt->bindTexture(img, GL_TEXTURE_2D, GL_RGBA, QGLContext::DefaultBindOption);
    glBindTexture(GL_TEXTURE_2D, tid);
}

// FIXME: should we check that image have appropriate size as bindFromImage do?
void TextureManager::bindTexture(const QString &name, QGLWidget *cxt)
{
    Create();
    Q_ASSERT("Must be called only with valid GL context" && cxt);

    CacheIter it = findTexture(name);
    if (it != m_p->m_textures.constEnd())
        bindImage(*it, cxt);
}

void TextureManager::bindFromImage(const QImage &image, QGLWidget *cxt)
{
    Create();
    Q_ASSERT("Must be called only with valid GL context" && cxt);

    if (image.width() != image.height() || (image.width() & (image.width() - 1)))
    {
        // Compute texture size
        int longest  = qMax(image.width(), image.height());
        int tex_size = 2;
        while (tex_size < longest)
        {
            tex_size *= 2;
        }
        // FIXME: Check if Qt does this for us already. [Note that it does scale to the nearest power of two]
        bindImage(image.scaled(tex_size, tex_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation), cxt);
    }
    else
    {
        bindImage(image, cxt);
    }
}
#endif

TextureManager::TextureManager(QObject *parent) : QObject(parent) {}

void TextureManager::discoverTextureDirs()
{
    // clear the cache
    m_p->m_textures = {};

    const auto &base = KSPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDirIterator search(base, QStringList() << "textures_*", QDir::Dirs);

    auto &dirs = m_p->m_texture_directories;
    while (search.hasNext())
    {
        dirs.push_back(search.next());
    }

    dirs.push_back(base);
};
