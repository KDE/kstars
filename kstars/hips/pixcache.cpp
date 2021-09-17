/*
    SPDX-FileCopyrightText: 2015-2017 Pavel Mraz

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "pixcache.h"

static int qHash(const pixCacheKey_t &key, uint seed)
{
  return qHash(QString("%1_%2_%3").arg(key.level).arg(key.pix).arg(key.uid), seed);
}

inline bool operator<(const pixCacheKey_t &k1, const pixCacheKey_t &k2)
{
  if (k1.uid != k2.uid)
  {
    return k1.uid < k2.uid;
  }

  if (k1.level != k2.level)
  {
    return k1.level < k2.level;
  }

  return k1.pix < k2.pix;
}

inline bool operator==(const pixCacheKey_t &k1, const pixCacheKey_t &k2)
{
  return (k1.uid == k2.uid) && (k1.level == k2.level) && (k1.pix == k2.pix);
}

void PixCache::add(pixCacheKey_t &key, pixCacheItem_t *item, int cost)
{
  Q_ASSERT(cost < m_cache.maxCost());

  m_cache.insert(key, item, cost);
}

pixCacheItem_t *PixCache::get(pixCacheKey_t &key)
{
  return m_cache.object(key);
}

void PixCache::setMaxCost(int maxCost)
{
  m_cache.setMaxCost(maxCost);
}

void PixCache::printCache()
{
  qDebug() << " -- cache ---------------";
  qDebug() << m_cache.size() << m_cache.totalCost() << m_cache.maxCost();
}

int PixCache::used()
{
  return m_cache.totalCost();
}
