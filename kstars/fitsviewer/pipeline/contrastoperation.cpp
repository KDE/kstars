/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "contrastoperation.h"

bool ContrastOperation::apply(cv::Mat &image, double amt, QString &error)
{
    if (image.empty())
    {
        error = QStringLiteral("No image to adjust contrast on");
        return false;
    }
    if (image.depth() != CV_32F)
    {
        error = QStringLiteral("ContrastOperation expects a CV_32F image");
        return false;
    }

    // Output is unconditionally clamped to [0,1] below, so on still-linear/
    // un-stretched data (background in the hundreds-thousands, saturation
    // ~65535) this would blow every pixel out to solid white — even at
    // amt == 1.0, nominally a no-op. Refuse instead, matching
    // SaturationOperation's own guard for the same precondition.
    double minVal, maxVal;
    cv::minMaxLoc(image.reshape(1), &minVal, &maxVal);
    if (maxVal > 1.5)
    {
        error = QString("ContrastOperation expects a normalized [0,1] image (e.g. after "
                        "AutoStretch) — got values up to %1").arg(maxVal);
        return false;
    }

    const cv::Scalar meanScalar = cv::mean(image);
    double pivot = 0.0;
    for (int c = 0; c < image.channels(); c++)
        pivot += meanScalar[c];
    pivot /= image.channels();

    image = (image - pivot) * amt + pivot;
    cv::max(image, 0.0, image);
    cv::min(image, 1.0, image);
    return true;
}
