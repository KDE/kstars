/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "indexfiles.h"

#include <QtMath>
#include <QHash>
#include <QSet>

#include <algorithm>

namespace Ekos
{
namespace IndexFiles
{

namespace
{
// Skymark size in arcmin of each index size, 00 to 19
constexpr float SKYMARK_SIZES[SIZE_COUNT] =
{
    2.8f, 4.0f, 5.6f, 8.0f, 11.0f, 16.0f, 22.0f, 30.0f, 42.0f, 60.0f,
    85.0f, 120.0f, 170.0f, 240.0f, 340.0f, 480.0f, 680.0f, 1000.0f, 1400.0f, 2000.0f
};
}

float skymarkSize(int size)
{
    if (size < 0 || size >= SIZE_COUNT)
        return -1;
    return SKYMARK_SIZES[size];
}

SizeRole sizeRole(int size, double fovMaxArcmin)
{
    if (size < 0 || size >= SIZE_COUNT)
        return Optional;

    // Same rules as the icons of the Index Files page
    const float skymark = SKYMARK_SIZES[size];
    const float lastSkymark = size == 0 ? 2.0f : SKYMARK_SIZES[size - 1];

    if ((skymark >= 0.40 * fovMaxArcmin && skymark <= 0.9 * fovMaxArcmin) ||
            (fovMaxArcmin > lastSkymark && fovMaxArcmin < skymark))
        return Required;
    if (skymark >= 0.10 * fovMaxArcmin && skymark <= fovMaxArcmin)
        return Recommended;
    return Optional;
}

QList<int> requiredIndexSizes(double fovMaxArcmin)
{
    QList<int> sizes;
    for (int size = 0; size < SIZE_COUNT; size++)
    {
        if (sizeRole(size, fovMaxArcmin) == Required)
            sizes << size;
    }
    return sizes;
}

int expectedFileCount(const QString &indexName)
{
    if (indexName.contains("4207") || indexName.contains("4206") || indexName.contains("4205"))
        return 12;
    if (indexName.contains("4204") || indexName.contains("4203") || indexName.contains("4202")
            || indexName.contains("4201") || indexName.contains("4200") || indexName.contains("5206")
            || indexName.contains("5205") || indexName.contains("5204") || indexName.contains("5203")
            || indexName.contains("5202") || indexName.contains("5201") || indexName.contains("5200"))
        return 48;
    return 1;
}

bool seriesComplete(const QDir &directory, const QString &indexName)
{
    const QString indexNameMatch = indexName.left(10) + "*.fits";
    const QStringList list = directory.entryList(QStringList(indexNameMatch));
    return list.count() == expectedFileCount(indexName);
}

QList<int> installedIndexSeries(const QStringList &directories)
{
    QSet<int> series;
    for (const auto &path : directories)
    {
        QDir directory(path);
        if (!directory.exists())
            continue;

        // List the folder once and count the files of each series. seriesComplete() lists the folder again
        // for every call, and is called for every file of an incomplete series.
        QHash<QString, int> files;
        for (const auto &fileName : directory.entryList(QStringList("index-*.fits"), QDir::Files))
            files[fileName.left(10)]++;

        for (auto it = files.cbegin(); it != files.cend(); ++it)
        {
            bool ok = false;
            const int number = it.key().mid(6, 4).toInt(&ok);
            if (ok && it.value() == expectedFileCount(it.key()))
                series.insert(number);
        }
    }

    QList<int> sorted(series.begin(), series.end());
    std::sort(sorted.begin(), sorted.end());
    return sorted;
}

QList<int> installedIndexSizes(const QList<int> &series)
{
    QSet<int> sizes;
    for (int number : series)
    {
        const int family = number / 100;
        const int size = number % 100;
        if ((family == 41 || family == 42 || family == 52) && size < SIZE_COUNT)
            sizes.insert(size);
    }

    QList<int> sorted(sizes.begin(), sizes.end());
    std::sort(sorted.begin(), sorted.end());
    return sorted;
}

int installSeries(int size)
{
    if (size < 0 || size >= SIZE_COUNT)
        return -1;
    return (size <= 6 ? 5200 : 4200) + size;
}

QString installName(int size)
{
    const int series = installSeries(size);
    if (series < 0)
        return QString();
    if (expectedFileCount(QString::number(series)) > 1)
        return QString("index-%1-*.fits").arg(series);
    return QString("index-%1.fits").arg(series);
}

qint64 estimatedDownloadBytes(int size)
{
    const float skymark = skymarkSize(size);
    if (skymark <= 0)
        return 0;
    return static_cast<qint64>(1E11 * qPow(skymark, -1.909));
}

}
}
