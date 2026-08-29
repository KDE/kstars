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
