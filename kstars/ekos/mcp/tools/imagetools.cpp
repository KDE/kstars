/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "imagetools.h"
#include "../mcpserver.h"
#include "../mcptoolregistry.h"

#include "fitsviewer/fitsdata.h"

#include <QBuffer>
#include <QByteArray>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QJsonObject>
#include <QJsonValue>

namespace MCP::Tools
{

void initImageTools(ToolRegistry *registry, Server *server)
{
    // -----------------------------------------------------------------------
    // image_last_info
    // -----------------------------------------------------------------------
    registry->registerTool(
    {
        QStringLiteral("image_last_info"),
        QStringLiteral("Returns metadata for the most recently captured frame on the rig: path, camera, exposure, "
                       "filter, target, date, CCD temperature, HFR (arcsec), star count, and dimensions. "
                       "Frames from every producer are seen: Capture queue, PAA, Focus, Align, ad-hoc camera_capture, "
                       "and raw INDI control. On a multi-camera rig pass `camera` (device name) to query a specific "
                       "camera; omit it to get the most recent frame across all cameras. "
                       "Returns {\"available\": false} if no image is available."),
        {
            {
                QStringLiteral("camera"), QStringLiteral("string"),
                QStringLiteral("Optional camera device name. Defaults to the most recently captured frame across all cameras."), false
            }
        },
        [server](const QJsonObject & args, QString &) -> QJsonValue
        {
            const QString cam = args.value(QStringLiteral("camera")).toString();
            const auto &li = cam.isEmpty() ? server->lastImage() : server->lastImageFor(cam);
            if (!li.available)
                return QJsonObject { { QStringLiteral("available"), false } };

            // Read filename lazily — see LastImage comment in mcpserver.h.
            const QString path = li.data ? li.data->filename() : QString();

            QJsonObject result;
            result[QStringLiteral("available")] = true;
            result[QStringLiteral("camera")]    = li.cameraName;
            result[QStringLiteral("receivedAt")] = li.receivedAt.toString(Qt::ISODateWithMs);
            result[QStringLiteral("path")]      = path;
            result[QStringLiteral("width")]     = li.width;
            result[QStringLiteral("height")]    = li.height;
            result[QStringLiteral("hfr")]       = li.hfr;
            result[QStringLiteral("starCount")] = li.starCount;
            if (!li.filter.isEmpty())  result[QStringLiteral("filter")]   = li.filter;
            if (!li.target.isEmpty())  result[QStringLiteral("target")]   = li.target;
            if (!li.dateObs.isEmpty()) result[QStringLiteral("dateObs")]  = li.dateObs;
            if (li.exposure > 0)       result[QStringLiteral("exposure")] = li.exposure;
            result[QStringLiteral("ccdTemp")]   = li.ccdTemp;
            return result;
        }
    });

    // -----------------------------------------------------------------------
    // image_last_thumbnail
    // -----------------------------------------------------------------------
    registry->registerTool(
    {
        QStringLiteral("image_last_thumbnail"),
        QStringLiteral("Returns a base64-encoded JPEG preview of the most recently captured frame, useful for "
                       "vision-capable LLMs. Frames from every producer are seen: Capture queue, PAA, Focus, Align, "
                       "ad-hoc camera_capture, and raw INDI control. On a multi-camera rig pass `camera` (device name) "
                       "to query a specific camera; omit it to get the most recent frame across all cameras. "
                       "Set maxSize to control the long-edge pixel count (default 512, capped at 1024 to keep "
                       "response under ~1 MB). Returns {\"available\": false} if no image is available."),
        {
            {
                QStringLiteral("camera"),  QStringLiteral("string"),
                QStringLiteral("Optional camera device name. Defaults to the most recently captured frame across all cameras."), false
            },
            {
                QStringLiteral("maxSize"), QStringLiteral("integer"),
                QStringLiteral("Long-edge pixel cap for the thumbnail. Default 512, max 1024."), false
            }
        },
        [server](const QJsonObject & args, QString & error) -> QJsonValue
        {
            const QString cam = args.value(QStringLiteral("camera")).toString();
            const auto &li = cam.isEmpty() ? server->lastImage() : server->lastImageFor(cam);
            if (!li.available || !li.data)
                return QJsonObject { { QStringLiteral("available"), false } };

            int maxSize = args.value(QStringLiteral("maxSize")).toInt(512);
            if (maxSize <= 0)   maxSize = 512;
            if (maxSize > 1024) maxSize = 1024;

            // FITSData::FITSToImage loads from disk. If the consumer has set
            // a filename (Capture queue) use it; otherwise (ad-hoc capture,
            // PAA preview) round-trip via a temp file written from the
            // in-memory FITSData so we still get a thumbnail.
            QString path = li.data->filename();
            QString tempPath;
            if (path.isEmpty())
            {
                tempPath = QDir::temp().filePath(QStringLiteral("mcp_thumb_%1.fits")
                                                 .arg(QString::number(reinterpret_cast<quintptr>(li.data.data()), 16)));
                if (!li.data->saveImage(tempPath))
                {
                    error = "Failed to materialize in-memory FITS for thumbnail";
                    return {};
                }
                path = tempPath;
            }

            QElapsedTimer timer;
            timer.start();
            QImage img = FITSData::FITSToImage(path);
            if (!tempPath.isEmpty()) QFile::remove(tempPath);
            if (img.isNull())
            {
                error = "Failed to render FITS to image";
                return {};
            }
            if (timer.elapsed() > 3000)
            {
                error = "FITS rendering exceeded 3 seconds";
                return {};
            }

            const QImage scaled = (img.width() > maxSize || img.height() > maxSize)
                                  ? img.scaled(maxSize, maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                                  : img;

            QByteArray jpg;
            QBuffer buf(&jpg);
            buf.open(QIODevice::WriteOnly);
            if (!scaled.save(&buf, "JPEG", 85))
            {
                error = "JPEG encoding failed";
                return {};
            }

            QJsonObject result;
            result[QStringLiteral("available")] = true;
            result[QStringLiteral("camera")]    = li.cameraName;
            result[QStringLiteral("mediaType")] = QStringLiteral("image/jpeg");
            result[QStringLiteral("encoding")]  = QStringLiteral("base64");
            result[QStringLiteral("width")]     = scaled.width();
            result[QStringLiteral("height")]    = scaled.height();
            result[QStringLiteral("data")]      = QString::fromLatin1(jpg.toBase64());
            return result;
        }
    });

    registry->classify(QStringLiteral("image_last_info"),      /*ro*/true, /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("image_last_thumbnail"), /*ro*/true, /*destr*/false, /*idemp*/true);
}

} // namespace MCP::Tools
