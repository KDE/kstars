/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDir>
#include <QList>
#include <QString>
#include <QStringList>

namespace Ekos
{
/**
 * @namespace Ekos::IndexFiles
 * @short Which astrometry.net index files a field of view needs, and which are installed.
 *
 * Index sizes are numbered 00 to 19, after the skymark size each one covers (2.8 to 2000 arcmin). A size comes
 * in series 41nn (4107 to 4119), 42nn (4200 to 4219) and 52nn (5200 to 5206, LITE). Some series are split
 * into several files per size, e.g. index-4207-00.fits to index-4207-11.fits.
 */
namespace IndexFiles
{

/** Result of an index file download, reported by OpsAstrometryIndexFiles::indexDownloadFinished. */
enum DownloadError
{
    NoError = 0,
    NetworkError,
    WriteError,
    PermissionError,
    Timeout,
    Cancelled,
    ServerUnavailable,
    NoWritableDirectory
};

/** How useful an index size is for a field of view, as shown in the Index Files page. */
enum SizeRole
{
    Optional,
    Recommended,
    Required
};

/** Number of index sizes (00 to 19). */
constexpr int SIZE_COUNT = 20;

/** Skymark size in arcmin covered by an index size, or -1 for an invalid size. */
float skymarkSize(int size);

/** Role of an index size for a field of view whose larger side is fovMaxArcmin. */
SizeRole sizeRole(int size, double fovMaxArcmin);

/** Index sizes required for a field of view whose larger side is fovMaxArcmin, in increasing order. */
QList<int> requiredIndexSizes(double fovMaxArcmin);

/**
 * @brief expectedFileCount Number of files in a complete series.
 * @param indexName any name starting with the series, e.g. "index-4207-03.fits" or "index-5205-*.fits"
 */
int expectedFileCount(const QString &indexName);

/** True if the directory holds every file of the series that indexName belongs to. */
bool seriesComplete(const QDir &directory, const QString &indexName);

/** Series numbers (e.g. 4207, 5205) installed completely in at least one of the directories, sorted. */
QList<int> installedIndexSeries(const QStringList &directories);

/** Index sizes covered by the installed series, sorted. */
QList<int> installedIndexSizes(const QList<int> &series);

/**
 * @brief installName The name to pass to OpsAstrometryIndexFiles::installIndexFile() for an index size.
 * Sizes 00 to 06 install the 52nn LITE series, size 07 the 4207 series, larger sizes the single 42nn file.
 * @return empty string for an invalid size
 */
QString installName(int size);

/** Series number installName() installs for a size, e.g. 5205 or 4208. -1 for an invalid size. */
int installSeries(int size);

/** Estimated download size in bytes of the series installName() installs, as OpsAstrometryIndexFiles estimates it. */
qint64 estimatedDownloadBytes(int size);

}
}
