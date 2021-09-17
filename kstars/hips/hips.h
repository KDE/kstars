/*
    SPDX-FileCopyrightText: 2015-2017 Pavel Mraz

    SPDX-FileCopyrightText: 2017 Jasem Mutlaq

    SPDX-License-Identifier: GPL-2.0-or-later
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

class pixCacheItem_t
{  
public:
   pixCacheItem_t() = default;

  ~pixCacheItem_t()
   {
     //qDebug() << "delete";
     Q_ASSERT(image);
     delete image;
   }

  QImage *image { nullptr };
};

typedef struct
{
  int    level;
  int    pix;
  qint64 uid;  
} pixCacheKey_t;

Q_DECLARE_METATYPE(pixCacheKey_t)

#endif // HIPS_H
