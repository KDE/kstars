/*
  Copyright (C) 2015-2017, Pavel Mraz

  Copyright (C) 2017, Jasem Mutlaq

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef HIPS_H
#define HIPS_H

#include <QString>
#include <QImage>
#include <QDebug>

#define HIPS_FRAME_EQT          0
#define HIPS_FRAME_GAL          1

typedef struct
{
  QString cachePath;
  qint64  discCacheSize;
  int     memoryCacheSize;   // count
} hipsCache_t;

typedef struct
{  
  bool    render;
  bool    showGrid;
  bool    billinear;
  QString name;
  QString imageExtension; // JPG, PNG, etc.
  QString url;  
  int     max_level;
  int     frame;          // HIPS_FRAME_xxx
  int     tileWidth;
} hipsParams_t;

class pixCacheItem_t
{  
public:
   pixCacheItem_t()
   {
     image = nullptr;
   }

  ~pixCacheItem_t()
   {
     //qDebug() << "delete";
     Q_ASSERT(image);
     delete image;
   }

  QImage *image;  
};

typedef struct
{
  int    level;
  int    pix;
  qint64 uid;  
} pixCacheKey_t;

Q_DECLARE_METATYPE(pixCacheKey_t)

#endif // HIPS_H
