/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include <QVector>

class QDir;

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
            // Path of the containing folder relative to the inspected root, using '/'
            // separators; empty for a file directly inside the root itself (e.g. a file
            // under <root>/Lights/HA gets "Lights/HA" here). Lets a flattened FileInfo
            // list still be traced back to its place in the tree.
            QString directory;
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
         * @brief One folder in the inspected tree, e.g. <root>/Lights/HA.
         *
         * Every node reports two views of what it contains: `files`/`groups` cover only
         * files directly inside this folder (matching the old non-recursive behavior),
         * while `totalFileCount`/`totalGroups` roll up this folder plus every descendant
         * — so e.g. the "Lights" node's totalFileCount is the combined count across
         * Lights/HA, Lights/SII, Lights/OIII and any files directly under Lights itself.
         */
        struct DirectoryNode
        {
            QString name;         // this folder's own name, e.g. "HA" (root node: the inspected folder's own name)
            QString relativePath; // path from the inspected root, e.g. "Lights/HA"; empty for the root node
            QVector<FileInfo> files;     // files directly inside this folder only
            QVector<Group> groups;       // groups for `files` only, sorted by exptime ascending
            QVector<DirectoryNode> subdirs; // immediate child folders, each recursively inspected
            int totalFileCount { 0 };       // this folder + all descendants
            QVector<Group> totalGroups;     // groups across this folder + all descendants, sorted by exptime ascending
        };

        /**
         * @brief Recursively inspect `dir` and every subdirectory beneath it (e.g. a
         * Lights/Darks/Flats/Bias layout with Lights further split into HA/SII/OIII),
         * reading FITS headers only (no pixel decoding). Symlinked subdirectories are
         * skipped to avoid cycles.
         * @param dir folder to inspect
         * @param outFiles receives one entry per FITS-loadable file found anywhere in
         * the tree, in directory-then-name order; FileInfo::directory says which folder
         * (relative to `dir`) each one came from
         * @param outGroups receives one entry per distinct (exptime, filter, binning,
         * imagetyp) combination found anywhere in the tree, with a count of how many
         * files share it across the whole tree, sorted by exptime ascending. Equivalent
         * to outTree.totalGroups. Files whose header couldn't be read (see
         * FileInfo::error) are excluded from grouping.
         * @param outTree receives the root DirectoryNode for `dir`, with per-folder
         * breakdowns (both direct and rolled-up) for every folder in the tree — this is
         * what a caller wants to show something like "Lights: 100 total, Lights/HA: 20"
         * @param error receives a human-readable failure reason if `dir` itself doesn't
         * exist, or if no FITS-loadable file was found anywhere in the tree (a single
         * file's header failing to read does NOT fail the whole call — it's recorded
         * per-file in FileInfo::error instead, so one corrupt file doesn't hide what's in
         * the rest of the tree)
         * @return success
         */
        static bool inspect(const QString &dir, QVector<FileInfo> &outFiles, QVector<Group> &outGroups,
                            DirectoryNode &outTree, QString &error);

    private:
        // Header-only read via cfitsio directly (fits_read_key) — no pixel decoding.
        static bool readFileInfo(const QString &path, FileInfo &outInfo);

        // Recursively builds the DirectoryNode for `directory`, whose path relative to
        // the inspected root is `relativePath` (empty for the root itself).
        static DirectoryNode buildNode(const QDir &directory, const QString &relativePath);
};
