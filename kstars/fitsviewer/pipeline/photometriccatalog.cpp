/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "photometriccatalog.h"

#include <QFile>
#include <QDataStream>

#include <algorithm>
#include <cmath>

std::vector<PhotometricCatalog::Entry> PhotometricCatalog::s_entries;
QString PhotometricCatalog::s_loadedPath;

bool PhotometricCatalog::load(const QString &path, QString &error)
{
    if (path == s_loadedPath && !s_entries.empty())
        return true; // already loaded

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        error = QString("Could not open photometric catalog: %1").arg(path);
        return false;
    }

    const qint64 size = file.size();
    constexpr qint64 recordSize = 4 * sizeof(float);
    if (size % recordSize != 0)
    {
        error = QString("Photometric catalog %1 has an invalid size (%2 bytes, not a multiple of %3)")
                .arg(path).arg(size).arg(recordSize);
        return false;
    }

    const qint64 count = size / recordSize;
    std::vector<Entry> entries(count);
    const qint64 bytesRead = file.read(reinterpret_cast<char *>(entries.data()), size);
    if (bytesRead != size)
    {
        error = QString("Short read loading photometric catalog: %1").arg(path);
        return false;
    }

    // Defensive: sort by decDeg even though the expected build pipeline already does,
    // so findNearest()'s binary-search bound is always valid regardless of how the
    // file was produced.
    std::sort(entries.begin(), entries.end(), [](const Entry & a, const Entry & b)
    {
        return a.decDeg < b.decDeg;
    });

    s_entries = std::move(entries);
    s_loadedPath = path;
    return true;
}

bool PhotometricCatalog::findNearest(double raDeg, double decDeg, double radiusDeg, float &v, float &bv)
{
    if (s_entries.empty())
        return false;

    // Binary-search bound on declination first -- cheap necessary-but-not-sufficient
    // prefilter before the more expensive per-candidate angular-distance check below.
    const Entry decLow { 0, static_cast<float>(decDeg - radiusDeg), 0, 0 };
    const Entry decHigh { 0, static_cast<float>(decDeg + radiusDeg), 0, 0 };
    auto cmp = [](const Entry & a, const Entry & b)
    {
        return a.decDeg < b.decDeg;
    };
    auto lo = std::lower_bound(s_entries.begin(), s_entries.end(), decLow, cmp);
    auto hi = std::upper_bound(s_entries.begin(), s_entries.end(), decHigh, cmp);

    const double decRad = decDeg * M_PI / 180.0;
    const double cosDec = std::cos(decRad);

    bool found = false;
    double bestSepDeg = radiusDeg;
    for (auto it = lo; it != hi; ++it)
    {
        // Small-angle-safe angular separation (both stars within a few degrees of each
        // other, so no need for full spherical law of cosines): flat-sky approximation
        // scaled by cos(dec), same convention as ChannelBlendOperation's WCS grid math.
        const double dRa = (it->raDeg - raDeg) * cosDec;
        const double dDec = it->decDeg - decDeg;
        const double sepDeg = std::sqrt(dRa * dRa + dDec * dDec);
        if (sepDeg <= bestSepDeg)
        {
            bestSepDeg = sepDeg;
            v = it->v;
            bv = it->bv;
            found = true;
        }
    }

    return found;
}
