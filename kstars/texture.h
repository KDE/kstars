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

#ifndef KSTEXTURE_H
#define KSTEXTURE_H

#include <GL/gl.h>

#include <QImage>

class Texture : public QObject
{
    Q_OBJECT
    friend class TextureManager;
public:
    ///Returns true if the texture is ready to be used. If false, don't try to use the texture.
    bool isReady() const;
    ///Get a pointer to the image associated with this texture
    const QImage& image() const;
    ///Bind the texture for use with GL -- return true if successful
    bool bind() const;
    
protected:
    Texture(QObject *parent = 0);
protected slots:
    void setImage(const QImage& img);
private:
    QImage m_image;
    bool m_ready;
    GLuint m_tid;
};

#endif // KSTEXTURE_H
