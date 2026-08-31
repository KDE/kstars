/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include <vector>

/**
 * @class PhotometricCatalog
 * @brief Optional, supplementary (RA, Dec, V, B-V) lookup for star colors, consulted
 * by FITSData::applyPhotometricCalibration() when KStars' own bundled star catalog has
 * no usable photometry for a given match.
 *
 * KStars' own StarObject::getBVIndex() is the primary source and stays that way — this
 * exists only to fill a real, observed gap: on a dev system carrying just the stock
 * unnamedstars.dat/deepstars.dat files, the large majority of non-named stars in a
 * given field returned KStars' "undefined" B-V sentinel (see StarObject::getBVIndex()),
 * even though real photometry for those same stars exists in the actual Tycho-2
 * catalog (near-100% BT/VT completeness for its ~2.5M entries). Rather than replacing
 * or patching KStars' own star catalog machinery (StarComponent/DeepStarComponent), a
 * small standalone binary of just (RA, Dec, V, B-V) — Tycho-2 positions with BT/VT
 * transformed to Johnson V/B-V via the standard ESA relation, magnitude-limited to
 * keep it compact — is loaded and consulted only as a color fallback.
 *
 * File format: a flat array of 16-byte native-endian records, each
 * {float raDeg, float decDeg, float v, float bv}, sorted ascending by decDeg (enables
 * a binary-search bound on declination before the final angular-distance scan, without
 * pulling in KStars' own HTM/trixel machinery for what's a much smaller, magnitude-
 * limited dataset).
 */
class PhotometricCatalog
{
    public:
        /**
         * @brief Load (or reload, if `path` differs from what's already loaded) the
         * catalog from disk. A no-op success if `path` is already loaded.
         */
        static bool load(const QString &path, QString &error);

        static bool isLoaded()
        {
            return !s_entries.empty();
        }

        /**
         * @brief Find the nearest catalog star to (raDeg, decDeg) within radiusDeg.
         * @return true if a star was found within radiusDeg (v/bv populated), false
         * otherwise (nothing in range, or the catalog isn't loaded).
         */
        static bool findNearest(double raDeg, double decDeg, double radiusDeg, float &v, float &bv);

    private:
        struct Entry
        {
            float raDeg, decDeg, v, bv;
        };

        static std::vector<Entry> s_entries;
        static QString s_loadedPath;
};
