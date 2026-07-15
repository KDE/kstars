/*
    SPDX-FileCopyrightText: 2026 Pavan <pk6122004@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "mount_guider_factory.h"
#include "worm_gear_guider.h"
#include "direct_drive_guider.h"
#include "harmonic_guider.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "kspaths.h"

QString MountGuiderFactory::detectMountType(const QString &mountName)
{
    QString filePath = KSPaths::locate(QStandardPaths::AppLocalDataLocation, "mount_types.json");

    if (filePath.isEmpty())
        return "NOT_FOUND";

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return "NOT_FOUND";

    QByteArray data = file.readAll();
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return "NOT_FOUND";

    QJsonObject obj = doc.object();
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
    {
        if (it.key().startsWith("_"))
            continue;

        if (it.value().isArray())
        {
            QJsonArray arr = it.value().toArray();
            for (int i = 0; i < arr.size(); ++i)
            {
                if (arr[i].toString().compare(mountName, Qt::CaseInsensitive) == 0)
                    return it.key();
            }
        }
    }
    return "NOT_FOUND";
}

QString MountGuiderFactory::readMountTypeFromWeights(const QString &weightsPath)
{
    QFile file(weightsPath);
    if (!file.open(QIODevice::ReadOnly))
        return QString();

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull() || !doc.isObject())
        return QString();

    return doc.object()["mount_type"].toString();
}

std::unique_ptr<MountSpecificGuider> MountGuiderFactory::create(const QString &mountType)
{
    if (mountType == "WORM_GEAR")
        return std::make_unique<WormGearGuider>();

    if (mountType == "DIRECT_DRIVE")
        return std::make_unique<DirectDriveGuider>();

    if (mountType == "HARMONIC_DRIVE")
        return std::make_unique<HarmonicGuider>();

    return nullptr;
}

std::unique_ptr<MountSpecificGuider> MountGuiderFactory::createFromWeights(const QString &weightsPath)
{
    const QString mountType = readMountTypeFromWeights(weightsPath);
    if (mountType.isEmpty())
        return nullptr;

    return create(mountType);
}
