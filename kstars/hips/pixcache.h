/*
    SPDX-FileCopyrightText: 2015-2017 Pavel Mraz

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "hips.h"

#include <QCache>

class PixCache
{
public:
  PixCache() = default;

  void add(pixCacheKey_t &key, pixCacheItem_t *item, int cost);
  pixCacheItem_t *get(pixCacheKey_t &key);
  void setMaxCost(int maxCost);
  void printCache();
  int  used();

private:  
  QCache <pixCacheKey_t, pixCacheItem_t> m_cache;
};

