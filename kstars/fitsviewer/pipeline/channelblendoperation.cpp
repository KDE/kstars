/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "channelblendoperation.h"

bool ChannelBlendOperation::blendChannel(const QVector<WeightedInput> &inputs, cv::Mat &outChannel, QString &error)
{
    if (inputs.isEmpty())
    {
        error = QStringLiteral("A channel needs at least one weighted input");
        return false;
    }

    const cv::Mat &first = inputs.front().image;
    if (first.empty())
    {
        error = QStringLiteral("Empty input image");
        return false;
    }
    if (first.depth() != CV_32F || first.channels() != 1)
    {
        error = QStringLiteral("ChannelBlendOperation expects single-channel CV_32F inputs "
                              "(one already-stacked mono session per input, not a combined RGB result)");
        return false;
    }

    outChannel = cv::Mat::zeros(first.size(), CV_32F);
    for (const auto &input : inputs)
    {
        if (input.image.empty() || input.image.size() != first.size() || input.image.channels() != 1
                || input.image.depth() != CV_32F)
        {
            error = QStringLiteral("All inputs to a blended channel must be the same size and a single-channel "
                                  "CV_32F image");
            return false;
        }
        outChannel += input.image * static_cast<float>(input.weight);
    }
    return true;
}

bool ChannelBlendOperation::blendRGB(const QVector<WeightedInput> &red, const QVector<WeightedInput> &green,
                                     const QVector<WeightedInput> &blue, cv::Mat &outImage, QString &error)
{
    cv::Mat r, g, b;
    if (!blendChannel(red, r, error) || !blendChannel(green, g, error) || !blendChannel(blue, b, error))
        return false;

    if (r.size() != g.size() || g.size() != b.size())
    {
        error = QStringLiteral("Red/green/blue channels must all be the same size");
        return false;
    }

    std::vector<cv::Mat> channels { r, g, b };
    cv::merge(channels, outImage);
    return true;
}
