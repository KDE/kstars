/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "directoryinspector.h"
#include "fitsviewer/fitsdata.h"

#include <fitsio.h>

#include <QDir>
#include <QFileInfo>

#include <algorithm>

namespace
{
// Reads one string-valued keyword, tolerating a missing key (leaves outValue
// untouched) rather than failing the whole file — different frame types
// legitimately have different header sets (e.g. bias frames often lack FILTER).
void readOptionalString(fitsfile *fptr, const char *key, QString &outValue)
{
    char buffer[FLEN_VALUE] = {0};
    int status = 0;
    if (!fits_read_key(fptr, TSTRING, key, buffer, nullptr, &status))
        outValue = QString(buffer).trimmed();
}

void readOptionalInt(fitsfile *fptr, const char *key, int &outValue, bool &outFound)
{
    int status = 0;
    if (!fits_read_key(fptr, TINT, key, &outValue, nullptr, &status))
        outFound = true;
}

void sortGroups(QVector<DirectoryInspector::Group> &groups)
{
    std::sort(groups.begin(), groups.end(), [](const DirectoryInspector::Group & a, const DirectoryInspector::Group & b)
    {
        return a.exptime < b.exptime;
    });
}

// Builds the per-folder groups for a set of files, ignoring ones whose header
// couldn't be read (FileInfo::error non-empty).
QVector<DirectoryInspector::Group> groupFiles(const QVector<DirectoryInspector::FileInfo> &files)
{
    QVector<DirectoryInspector::Group> groups;
    for (const auto &info : files)
    {
        if (!info.error.isEmpty())
            continue;

        auto match = std::find_if(groups.begin(), groups.end(), [&info](const DirectoryInspector::Group & g)
        {
            return g.exptime == info.exptime && g.filter == info.filter &&
                   g.binning == info.binning && g.imagetyp == info.imagetyp;
        });

        if (match != groups.end())
            match->count++;
        else
            groups.push_back({ info.exptime, info.filter, info.binning, info.imagetyp, 1 });
    }

    sortGroups(groups);
    return groups;
}

// Folds `addition` into `accumulator`, merging counts for matching (exptime, filter,
// binning, imagetyp) combinations rather than duplicating them — used to roll a child
// folder's groups up into its parent's totalGroups.
void mergeGroupsInto(QVector<DirectoryInspector::Group> &accumulator, const QVector<DirectoryInspector::Group> &addition)
{
    for (const auto &g : addition)
    {
        auto match = std::find_if(accumulator.begin(), accumulator.end(), [&g](const DirectoryInspector::Group & existing)
        {
            return existing.exptime == g.exptime && existing.filter == g.filter &&
                   existing.binning == g.binning && existing.imagetyp == g.imagetyp;
        });

        if (match != accumulator.end())
            match->count += g.count;
        else
            accumulator.push_back(g);
    }
}

void flattenFiles(const DirectoryInspector::DirectoryNode &node, QVector<DirectoryInspector::FileInfo> &outFiles)
{
    outFiles += node.files;
    for (const auto &child : node.subdirs)
        flattenFiles(child, outFiles);
}
}

bool DirectoryInspector::readFileInfo(const QString &path, FileInfo &outInfo)
{
    outInfo.filename = QFileInfo(path).fileName();

    fitsfile *fptr = nullptr;
    int status = 0;
    QByteArray pathBytes = path.toLocal8Bit();

    if (fits_open_diskfile(&fptr, pathBytes.constData(), READONLY, &status))
    {
        char errStatus[FLEN_STATUS] = {0};
        fits_get_errstatus(status, errStatus);
        outInfo.error = QString("Failed to open: %1").arg(errStatus);
        return false;
    }

    double exptime = 0.0;
    if (!fits_read_key(fptr, TDOUBLE, "EXPTIME", &exptime, nullptr, &status))
        outInfo.exptime = exptime;

    readOptionalString(fptr, "FILTER", outInfo.filter);
    readOptionalString(fptr, "IMAGETYP", outInfo.imagetyp);

    int xBinning = 0, yBinning = 0;
    bool haveX = false, haveY = false;
    readOptionalInt(fptr, "XBINNING", xBinning, haveX);
    readOptionalInt(fptr, "YBINNING", yBinning, haveY);
    if (haveX)
        outInfo.binning = QString("%1x%2").arg(xBinning).arg(haveY ? yBinning : xBinning);

    status = 0;
    fits_close_file(fptr, &status);
    return true;
}

// Subdirectories reached via a symlink are skipped so a cyclic link (e.g. a folder
// linked into itself) can't cause unbounded recursion.
DirectoryInspector::DirectoryNode DirectoryInspector::buildNode(const QDir &directory, const QString &relativePath)
{
    DirectoryNode node;
    node.relativePath = relativePath;
    node.name = relativePath.isEmpty() ? directory.dirName() : relativePath.section('/', -1);

    for (const auto &fileInfo : directory.entryInfoList(QDir::Files, QDir::Name))
    {
        if (!FITSData::readableFilename(fileInfo.absoluteFilePath()))
            continue;

        FileInfo info;
        info.directory = relativePath;
        readFileInfo(fileInfo.absoluteFilePath(), info); // per-file failure recorded in info.error, not fatal
        node.files.push_back(info);
    }

    node.groups = groupFiles(node.files);
    node.totalFileCount = node.files.size();
    node.totalGroups = node.groups;

    const auto subdirFilter = QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks;
    for (const auto &subdirInfo : directory.entryInfoList(subdirFilter, QDir::Name))
    {
        QString childRelativePath = relativePath.isEmpty() ? subdirInfo.fileName()
                                     : relativePath + '/' + subdirInfo.fileName();
        DirectoryNode child = buildNode(QDir(subdirInfo.absoluteFilePath()), childRelativePath);

        node.totalFileCount += child.totalFileCount;
        mergeGroupsInto(node.totalGroups, child.totalGroups);
        node.subdirs.push_back(child);
    }

    sortGroups(node.totalGroups);
    return node;
}

bool DirectoryInspector::inspect(const QString &dir, QVector<FileInfo> &outFiles, QVector<Group> &outGroups,
                                 DirectoryNode &outTree, QString &error)
{
    QDir directory(dir);
    if (!directory.exists())
    {
        error = QString("Directory %1 does not exist").arg(dir);
        return false;
    }

    outTree = buildNode(directory, QString());

    outFiles.clear();
    flattenFiles(outTree, outFiles);
    outGroups = outTree.totalGroups;

    if (outFiles.isEmpty())
    {
        error = QString("No FITS-loadable files found in %1 or its subdirectories").arg(dir);
        return false;
    }

    return true;
}
