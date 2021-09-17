/*
    SPDX-FileCopyrightText: 2001 Jason Harris <jharris@30doradus.org>
    SPDX-FileCopyrightText: 2021 Valentin Boettcher <hiro at protagon.space; @hiro98:tchncs.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once
#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <vector>
#include <list>

/**
 * \brief A simple integer indexed elastically cached wrapper around
 * std::vector to hold container types `content` which are cheap to
 * construct empty and provide a default constructor, as well as `[]`,
 * `size` and `swap` members (think `std::vector` and `std::list`). The
 * caching mechanism will be short-circuited if the cache size equals the
 * data size.
 *
 * The container is resized to a given data_size by default
 * initializing its elements. The elements are stored as
 * `TrixelCache::element` objects and retrieved by index and LRU
 * cached on the fly. To test if a given element is cached (or not
 * empty) the `TrixelCache::element::is_set()` can be queried. Setting
 * an element works by just assigning it with the new data. The data
 * from an element can be retrieved through the
 * `TrixelCache::element::data()` member. Modifying the data through
 * that reference is allowed.
 *
 * Setting an element is done by assigning it the new data.
 *
 * When it is convenient `TrixelCache::prune()` may be called, which
 * clears the least recently used elements (by default initializing them)
 * until the number of elements does not exceed the cache size. This is
 * expensive relative to setting an elment which has almost no cost.
 *
 * \tparam content The content type to use. Most likely a QList,
 * `std::vector or std::list.`
 *
 * \todo Use `std::optional` when C++17 is adopted in kstars.
 *
 * ### Example
 *
 * ~~~~~~~~{cpp}
 * TrixelCache<std::vector<int>> cache(100, 10);
 * auto& element = chache[0];
 *
 * if(!elmenent.is_set())
 *   elment = std::vector<int> {1, 2, 3, 4};
 *
 * std::cout << cache[0].data(); // {1, 2, 3, 4}
 * ~~~~~~~~
 */

template <typename content>
class TrixelCache
{
  public:
    /**
     * @brief A container to hold cache elements.
     *
     * The holds the data and the information if the data has been set
     * already. To this end, the assignment operator has been
     * overloaded.
     */
    class element
    {
      public:
        /** @return wether the element contains a cached object */
        bool is_set() { return _set; }

        /** @return the data held by element */
        content &data() { return _data; }

        element &operator=(const content &rhs)
        {
            _data = rhs;
            _set  = true;
            return *this;
        }

        element &operator=(content &&rhs)
        {
            _data.swap(rhs);
            _set = true;
            return *this;
        }

        /** resets the element to the empty state */
        void reset()
        {
            content().swap(_data);
            _set = false;
        };

      private:
        bool _set{ false };
        content _data;
    };

    /**
     * Constructs a cache with \p data_size default constructed elements
     * with an elastic ceiling capacity of \p cache_size.
     */
    TrixelCache(const size_t data_size, const size_t cache_size)
        : _cache_size{ cache_size }, _noop{ cache_size == data_size }
    {
        if (_cache_size > data_size)
            throw std::range_error("cache_size cannot exceet data_size");

        _data.resize(data_size);
    };

    /** Retrieve an element at \p index. */
    element &operator[](const size_t index) noexcept
    {
        if (!_noop)
            add_index(index);

        return _data[index];
    }

    /**
     * Remove excess elements from the cache
     * The capacity can be temporarily readjusted to \p keep.
     * \p keep must be greater than the cache size to be of effect.
     */
    void prune(size_t keep = 0) noexcept
    {
        if (_noop)
            return;

        remove_dublicate_indices();
        const int delta =
            _used_indices.size() - (keep > _cache_size ? keep : _cache_size);

        if (delta <= 0)
            return;

        auto begin = _used_indices.begin();
        std::advance(begin, delta);

        std::for_each(begin, _used_indices.end(),
                      [&](size_t index) { _data[index].reset(); });

        _used_indices = {};
    }

    /**
     * Reset the cache size to \p size. This does clear the cache.
     */
    void set_size(const size_t size)
    {
        if (size > _data.size())
            throw std::range_error("cache_size cannot exceet data_size");

        clear();

        _cache_size = size;
        _noop       = (_cache_size == _data.size());
    }

    /** @return the size of the cache */
    size_t size() const { return _cache_size; };

    /** @return the number of set elements in the cache, slow */
    size_t current_usage()
    {
        remove_dublicate_indices();
        return _used_indices.size();
    };

    /** @return a list of currently primed indices, slow */
    std::list<size_t> primed_indices()
    {
        remove_dublicate_indices();
        return _used_indices;
    };

    /** @return wether the cache is just a wrapped vector */
    bool noop() const { return _noop; }

    /** Clear the cache without changing it's size. */
    void clear() noexcept
    {
        auto size = _data.size();
        std::vector<element>().swap(_data);
        _data.resize(size);
        _used_indices.clear();
    }

  private:
    size_t _cache_size;
    bool _noop;
    std::vector<element> _data;
    std::list<size_t> _used_indices;

    /** Add an index to the lru caching list */
    void add_index(const size_t index) { _used_indices.push_front(index); }

    void remove_dublicate_indices()
    {
        std::vector<bool> found(_data.size(), false);
        _used_indices.remove_if([&](size_t index) {
            if (found[index])
                return true;

            found[index] = true;
            return false;
        });
    };
};
