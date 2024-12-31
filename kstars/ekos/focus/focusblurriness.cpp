/*
    SPDX-FileCopyrightText: 2024 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "focusblurriness.h"

namespace Ekos
{

FocusBlurriness::FocusBlurriness()
{
    m_maxX = m_maxY = -1.0;
}

FocusBlurriness::~FocusBlurriness()
{
}

ImageMosaicMask * FocusBlurriness::getMosaicMask(const QSharedPointer<ImageMask> &mask, const int tile)
{
    ImageMosaicMask *mosaicMask = nullptr;
    if (mask)
    {
        if (tile < 0 || tile >= NUM_TILES)
            qCDebug(KSTARS_EKOS_FOCUS) << QString("%1 called with invalid mosaic tile %2").arg(__FUNCTION__)
                                       .arg(tile);
        else
        {
            mosaicMask = dynamic_cast<ImageMosaicMask *>(mask.get());
            if (!mosaicMask)
                qCDebug(KSTARS_EKOS_FOCUS) << QString("%1 called with invalid 2 mosaic tile %2").arg(__FUNCTION__)
                                           .arg(tile);
        }
    }
    else
        qCDebug(KSTARS_EKOS_FOCUS) << QString("%1 called for mosaic tile but no mask").arg(__FUNCTION__);

    return mosaicMask;
}

int FocusBlurriness::getCVType(const int type)
{
    int cvType = -1;
    switch (type)
    {
        case TBYTE:
            cvType = CV_MAKETYPE(CV_8U, 1); // uint8_t
            break;

        case TSHORT:
            cvType = CV_MAKETYPE(CV_16S, 1); // short
            break;

        case TUSHORT:
            cvType = CV_MAKETYPE(CV_16U, 1); // unsigned short
            break;

        case TLONG:
            cvType = CV_MAKETYPE(CV_32S, 1); // long
            break;

        case TULONG:
            qCDebug(KSTARS_EKOS_FOCUS) << "OpenCV does not support " << type << " Cannot calc blurriness";
            break;

        case TFLOAT:
            cvType = CV_MAKETYPE(CV_32F, 1); // long
            break;

        case TLONGLONG:
            qCDebug(KSTARS_EKOS_FOCUS) << "OpenCV does not support " << type << " Cannot calc blurriness";
            break;

        case TDOUBLE:
            cvType = CV_MAKETYPE(CV_64F, 1); // double
            break;

        default:
            qCDebug(KSTARS_EKOS_FOCUS) << "Unknown image buffer datatype " << type << " Cannot calc blurriness";
            break;
    }
    return cvType;
}

cv::Rect FocusBlurriness::calcROIfromTile(const int tile, ImageMosaicMask *mosaicMask)
{
    unsigned int topLeftX = 0, topLeftY = 0, width = 0, height = 0;

    if (tile >= 0 && tile < NUM_TILES)
    {
        // Calculating for a single (square) mosaic tile
        topLeftX = mosaicMask->tiles()[tile].topLeft().x();
        topLeftY = mosaicMask->tiles()[tile].topLeft().y();
        width = mosaicMask->tiles()[tile].width();
        height = width;
    }
    cv::Rect cvROI(topLeftX, topLeftY, width, height);
    return cvROI;
}

void FocusBlurriness::applyRingMaskToImage(cv::Mat &img, const int width, const int height, ImageRingMask *ringMask)
{
    // Setup a mask for img with all elements zero except the ring with elements 1
    // Since its the same mask for each datapoint in an Autofocus I could calculate it once
    // but it seems quite fast.
    cv::Mat cvMask = cv::Mat::zeros(img.size(), CV_8UC1);
    const float diagonalRadius = std::sqrt((width * width + height * height) / 4.0);
    const float innerRadius = ringMask->innerRadius() * diagonalRadius;
    const float outerRadius = ringMask->outerRadius() * diagonalRadius;
    const cv::Point center(width / 2, height / 2);
    cv::circle(cvMask, center, outerRadius, 255, cv::FILLED, cv::FILLED);
    cv::circle(cvMask, center, innerRadius, 0, cv::FILLED, cv::FILLED);

    // Apply the mask and convert back to img for further processing. For some reason not using an
    // intermediate Mat (res) works while just using img does not.
    cv::Mat res;
    img.copyTo(res, cvMask);
    img = res;
    if (debug)
    {
        cv::imshow("Mask", cvMask);
        cv::imshow("Image with mask", img);
    }
}
}  // namespace
