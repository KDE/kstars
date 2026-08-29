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

bool DirectoryInspector::inspect(const QString &dir, QVector<FileInfo> &outFiles, QVector<Group> &outGroups,
                                 QString &error)
{
    QDir directory(dir);
    if (!directory.exists())
    {
        error = QString("Directory %1 does not exist").arg(dir);
        return false;
    }

    QStringList files;
    for (const auto &info : directory.entryInfoList(QDir::Files, QDir::Name))
    {
        if (FITSData::readableFilename(info.absoluteFilePath()))
            files << info.absoluteFilePath();
    }

    if (files.isEmpty())
    {
        error = QString("No FITS-loadable files found in %1").arg(dir);
        return false;
    }

    outFiles.clear();
    for (const auto &file : files)
    {
        FileInfo info;
        readFileInfo(file, info); // per-file failure recorded in info.error, not fatal
        outFiles.push_back(info);
    }

    outGroups.clear();
    for (const auto &info : outFiles)
    {
        if (!info.error.isEmpty())
            continue;

        auto match = std::find_if(outGroups.begin(), outGroups.end(), [&info](const Group & g)
        {
            return g.exptime == info.exptime && g.filter == info.filter &&
                   g.binning == info.binning && g.imagetyp == info.imagetyp;
        });

        if (match != outGroups.end())
            match->count++;
        else
            outGroups.push_back({ info.exptime, info.filter, info.binning, info.imagetyp, 1 });
    }

    std::sort(outGroups.begin(), outGroups.end(), [](const Group & a, const Group & b)
    {
        return a.exptime < b.exptime;
    });

    return true;
}
