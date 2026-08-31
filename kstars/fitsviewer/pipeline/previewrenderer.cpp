/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "previewrenderer.h"
#include "autostretch.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include <QByteArray>

#include <algorithm>

QString PreviewRenderer::renderBase64Jpeg(const cv::Mat &image, QString &error, int maxDimension, int jpegQuality)
{
    if (image.empty())
    {
        error = QStringLiteral("No image to preview");
        return QString();
    }

    try
    {
        // Downscale first — every subsequent step (stretch statistics, encode) then
        // operates on the small copy, keeping this cheap enough to call on every
        // interactive adjustment regardless of the source's full resolution.
        cv::Mat small;
        const int longSide = std::max(image.cols, image.rows);
        if (longSide > maxDimension && maxDimension > 0)
        {
            const double scale = (double)maxDimension / longSide;
            cv::resize(image, small, cv::Size(), scale, scale, cv::INTER_AREA);
        }
        else
            small = image.clone();

        // Auto-detect whether this still needs a preview stretch, or is already
        // display-ready (e.g. a session that already baked in a real autostretch, or
        // is mid-curve-adjustment on already-toned [0,1] data). Re-stretching an
        // already-correctly-toned image would flatten it right back toward gray, so
        // skip it when the data is already comfortably within [0,1].
        double minVal, maxVal;
        cv::minMaxLoc(small.reshape(1), &minVal, &maxVal);
        if (maxVal > 1.5 || minVal < -0.01)
        {
            QString stretchError;
            // Linked: a preview is a quick sanity check of the overall result, not a
            // final artistic call — the safer, more broadly-correct default.
            AutoStretch::apply(small, stretchError, 0.25, 2.8, true);
            // Deliberately not treated as fatal — worst case the preview looks like
            // raw linear data (very dark/low-contrast) rather than failing outright.
        }

        cv::Mat clamped;
        cv::max(small, 0.0f, clamped);
        cv::min(clamped, 1.0f, clamped);

        cv::Mat display8u;
        clamped.convertTo(display8u, CV_8U, 255.0);

        std::vector<uchar> buffer;
        const std::vector<int> params { cv::IMWRITE_JPEG_QUALITY, jpegQuality };
        if (!cv::imencode(".jpg", display8u, buffer, params))
        {
            error = QStringLiteral("Failed to JPEG-encode preview");
            return QString();
        }

        const QByteArray bytes(reinterpret_cast<const char *>(buffer.data()), (int)buffer.size());
        return QString::fromLatin1(bytes.toBase64());
    }
    catch (const cv::Exception &ex)
    {
        error = QString("OpenCV exception in PreviewRenderer::renderBase64Jpeg: %1").arg(ex.what());
        return QString();
    }
}
