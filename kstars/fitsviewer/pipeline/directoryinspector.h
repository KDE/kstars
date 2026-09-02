/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include <QVector>

/**
 * @class DirectoryInspector
 * @brief Reports what's actually in a folder of FITS subs, via header-only reads (no
 * pixel decoding) — EXPTIME, FILTER, binning, IMAGETYP for every file, plus how many
 * files share each distinct combination of those.
 *
 * Exists because callers building a master (MasterBuilder, with its matchExptime
 * filter) or starting a stack have no way to discover what exposure/filter/binning a
 * folder actually contains short of external tooling — a real gap surfaced by a real
 * shared calibration library that silently mixed 7 different exposures under one
 * undifferentiated "Dark Frame" IMAGETYP (see MasterBuilder's matchExptime comment).
 * Useful for any kind of folder (bias/dark/flat/light), not darks specifically: flats
 * are often auto-exposed (so individual subs vary slightly, and the actual spread
 * matters for picking a matchExptime tolerance), and any folder can have a stray
 * wrong-filter or wrong-exposure file mixed in by mistake.
 */
class DirectoryInspector
{
    public:
        struct FileInfo
        {
            QString filename;
            double exptime { -1.0 };
            QString filter;
            QString binning;
            QString imagetyp;
            QString error; // non-empty if the file's header couldn't be read at all
        };

        struct Group
        {
            double exptime { -1.0 };
            QString filter;
            QString binning;
            QString imagetyp;
            int count { 0 };
        };

        /**
         * @brief Inspect every FITS-loadable file directly inside `dir` (non-recursive,
         * matching MasterBuilder's own folder semantics).
         * @param dir folder to inspect
         * @param outFiles receives one entry per file, in directory order
         * @param outGroups receives one entry per distinct (exptime, filter, binning,
         * imagetyp) combination found, with a count of how many files share it, sorted
         * by exptime ascending. Files whose header couldn't be read (see FileInfo::error)
         * are excluded from grouping.
         * @param error receives a human-readable failure reason if the directory itself
         * can't be listed (a single file's header failing does NOT fail the whole call —
         * it's recorded per-file in FileInfo::error instead, so one corrupt file doesn't
         * hide what's in the rest of the folder)
         * @return success
         */
        static bool inspect(const QString &dir, QVector<FileInfo> &outFiles, QVector<Group> &outGroups,
                            QString &error);

    private:
        // Header-only read via cfitsio directly (fits_read_key) — no pixel decoding.
        static bool readFileInfo(const QString &path, FileInfo &outInfo);
};
