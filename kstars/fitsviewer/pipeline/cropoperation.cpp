/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "cropoperation.h"

#include <wcs.h>

bool CropOperation::apply(cv::Mat &image, const QRect &roi, struct wcsprm *wcs, QString &error)
{
    if (image.empty())
    {
        error = QStringLiteral("No image to crop");
        return false;
    }

    const QRect bounds(0, 0, image.cols, image.rows);
    if (roi.width() <= 0 || roi.height() <= 0 || !bounds.contains(roi))
    {
        error = QString("Crop region %1,%2 %3x%4 is outside the image bounds %5x%6")
                .arg(roi.x()).arg(roi.y()).arg(roi.width()).arg(roi.height())
                .arg(image.cols).arg(image.rows);
        return false;
    }

    image = image(cv::Rect(roi.x(), roi.y(), roi.width(), roi.height())).clone();

    if (wcs != nullptr)
    {
        // CRPIX is the reference pixel in the *uncropped* image, 1-indexed. Removing
        // roi.x() columns from the left and roi.y() rows from the top shifts every
        // pixel's coordinate by that same offset, reference pixel included — a point
        // that was at column c is now at column c - roi.x(). No axis flip needed: this
        // codebase stores FITS pixel data and the CRPIX/CD WCS convention in the same
        // row order throughout (see stackSetupWCS()/convertMatToFITS() — neither flips
        // between them), so cv::Mat row i corresponds directly to CRPIX2 without inversion.
        wcs->crpix[0] -= roi.x();
        wcs->crpix[1] -= roi.y();

        int status = wcsset(wcs);
        if (status != 0)
        {
            error = QString("wcsset error %1: %2 after crop").arg(status).arg(wcs_errmsg[status]);
            return false;
        }
    }

    return true;
}
