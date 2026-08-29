/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "saturationoperation.h"

#include <opencv2/imgproc.hpp>

bool SaturationOperation::apply(cv::Mat &image, double amt, QString &error)
{
    if (image.empty())
    {
        error = QStringLiteral("No image to adjust saturation on");
        return false;
    }
    if (image.depth() != CV_32F)
    {
        error = QStringLiteral("SaturationOperation expects a CV_32F image");
        return false;
    }
    if (image.channels() == 1)
        return true;
    if (image.channels() != 3)
    {
        error = QString("SaturationOperation expects a 1 or 3 channel image, got %1").arg(image.channels());
        return false;
    }

    double minVal, maxVal;
    cv::minMaxLoc(image.reshape(1), &minVal, &maxVal);
    if (maxVal > 1.5)
    {
        error = QString("SaturationOperation expects a normalized [0,1] image (e.g. after "
                        "AutoStretch) — got values up to %1").arg(maxVal);
        return false;
    }

    cv::Mat hsv;
    cv::cvtColor(image, hsv, cv::COLOR_RGB2HSV);

    std::vector<cv::Mat> hsvChannels;
    cv::split(hsv, hsvChannels);
    hsvChannels[1] *= amt;
    cv::min(hsvChannels[1], 1.0, hsvChannels[1]);
    cv::max(hsvChannels[1], 0.0, hsvChannels[1]);
    cv::merge(hsvChannels, hsv);

    cv::cvtColor(hsv, image, cv::COLOR_HSV2RGB);
    return true;
}
