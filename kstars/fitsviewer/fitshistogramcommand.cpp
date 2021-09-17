/*
    SPDX-FileCopyrightText: 2021 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <zlib.h>
#include <QApplication>

#include "fitshistogramcommand.h"
#include "fitshistogrameditor.h"
#include "fitsviewer.h"
#include "fits_debug.h"

FITSHistogramCommand::FITSHistogramCommand(const QSharedPointer<FITSData> &data,
        FITSHistogramEditor * inHisto,
        FITSScale newType,
        const QVector<double> &lmin,
        const QVector<double> &lmax) : m_ImageData(data), histogram(inHisto),
    type(newType), min(lmin), max(lmax)
{
}

FITSHistogramCommand::~FITSHistogramCommand()
{
    delete[] delta;
}

bool FITSHistogramCommand::calculateDelta(const uint8_t * buffer)
{
    uint8_t const * image_buffer = m_ImageData->getImageBuffer();
    uint32_t totalPixels = m_ImageData->samplesPerChannel() * m_ImageData->channels();
    unsigned long totalBytes = totalPixels * m_ImageData->getBytesPerPixel();

    auto * raw_delta = new uint8_t[totalBytes];

    if (raw_delta == nullptr)
    {
        qCWarning(KSTARS_FITS) << "Error! not enough memory to create image delta";
        return false;
    }

    for (uint32_t i = 0; i < totalBytes; i++)
        raw_delta[i] = buffer[i] ^ image_buffer[i];

    compressedBytes = sizeof(uint8_t) * totalBytes + totalBytes / 64 + 16 + 3;
    delete[] delta;
    delta = new uint8_t[compressedBytes];

    if (delta == nullptr)
    {
        delete[] raw_delta;
        qCCritical(KSTARS_FITS)
                << "FITSHistogram Error: Ran out of memory compressing delta";
        return false;
    }

    int r = compress2(delta, &compressedBytes, raw_delta, totalBytes, 5);

    if (r != Z_OK)
    {
        delete[] raw_delta;
        /* this should NEVER happen */
        qCCritical(KSTARS_FITS)
                << "FITSHistogram Error: Failed to compress raw_delta";
        return false;
    }

    delete[] raw_delta;
    return true;
}

bool FITSHistogramCommand::reverseDelta()
{
    uint8_t const * image_buffer = m_ImageData->getImageBuffer();
    uint32_t totalPixels = m_ImageData->samplesPerChannel() * m_ImageData->channels();
    unsigned long totalBytes = totalPixels * m_ImageData->getBytesPerPixel();

    auto * output_image = new uint8_t[totalBytes];

    if (output_image == nullptr)
    {
        qCWarning(KSTARS_FITS) << "Error! not enough memory to create output image";
        return false;
    }

    auto * raw_delta = new uint8_t[totalBytes];

    if (raw_delta == nullptr)
    {
        delete[] output_image;
        qCWarning(KSTARS_FITS) << "Error! not enough memory to create image delta";
        return false;
    }

    int r = uncompress(raw_delta, &totalBytes, delta, compressedBytes);
    if (r != Z_OK)
    {
        qCCritical(KSTARS_FITS)
                << "FITSHistogram compression error in reverseDelta()";
        delete[] output_image;
        delete[] raw_delta;
        return false;
    }

    for (unsigned int i = 0; i < totalBytes; i++)
        output_image[i] = raw_delta[i] ^ image_buffer[i];

    m_ImageData->setImageBuffer(output_image);

    delete[] raw_delta;

    return true;
}

void FITSHistogramCommand::redo()
{
    uint8_t const * image_buffer = m_ImageData->getImageBuffer();
    uint8_t * buffer = nullptr;
    uint32_t totalPixels = m_ImageData->samplesPerChannel() * m_ImageData->channels();
    int BBP = m_ImageData->getBytesPerPixel();

    QApplication::setOverrideCursor(Qt::WaitCursor);

    if (delta != nullptr)
    {
        FITSImage::Statistic prevStats;
        m_ImageData->saveStatistics(prevStats);

        reverseDelta();

        m_ImageData->restoreStatistics(stats);

        stats = prevStats;
    }
    else
    {
        m_ImageData->saveStatistics(stats);

        // If it's rotation of flip, no need to calculate delta
        if (type >= FITS_ROTATE_CW && type <= FITS_FLIP_V)
        {
            m_ImageData->applyFilter(type);
        }
        else
        {
            buffer = new uint8_t[totalPixels * BBP];

            if (buffer == nullptr)
            {
                qWarning(KSTARS_FITS()) << "Error! not enough memory to create image buffer in redo()";
                QApplication::restoreOverrideCursor();
                return;
            }

            memcpy(buffer, image_buffer, totalPixels * BBP);

            QVector<double> dataMin = min, dataMax = max;
            switch (type)
            {
                case FITS_AUTO:
                case FITS_LINEAR:
                    m_ImageData->applyFilter(FITS_LINEAR, nullptr, &dataMin, &dataMax);
                    break;

                case FITS_LOG:
                    m_ImageData->applyFilter(FITS_LOG, nullptr, &dataMin, &dataMax);
                    break;

                case FITS_SQRT:
                    m_ImageData->applyFilter(FITS_SQRT, nullptr, &dataMin, &dataMax);
                    break;

                default:
                    m_ImageData->applyFilter(type);
                    break;
            }

            calculateDelta(buffer);
            delete[] buffer;
        }
    }

    //    if (histogram != nullptr)
    //    {
    //        histogram->construct();

    //        //        if (tab->getViewer()->isStarsMarked())
    //        //            imageData->findStars().waitForFinished();
    //    }

    //    image->pushFilter(type);
    //    image->rescale(ZOOM_KEEP_LEVEL);
    //    image->updateFrame();

    QApplication::restoreOverrideCursor();
}

void FITSHistogramCommand::undo()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);

    if (delta != nullptr)
    {
        FITSImage::Statistic prevStats;
        m_ImageData->saveStatistics(prevStats);

        reverseDelta();

        m_ImageData->restoreStatistics(stats);

        stats = prevStats;
    }
    else
    {
        switch (type)
        {
            case FITS_ROTATE_CW:
                m_ImageData->applyFilter(FITS_ROTATE_CCW);
                break;
            case FITS_ROTATE_CCW:
                m_ImageData->applyFilter(FITS_ROTATE_CW);
                break;
            case FITS_FLIP_H:
            case FITS_FLIP_V:
                m_ImageData->applyFilter(type);
                break;
            default:
                break;
        }
    }

    //    if (histogram != nullptr)
    //    {
    //        histogram->construct();

    //        //        if (tab->getViewer()->isStarsMarked())
    //        //            imageData->findStars().waitForFinished();
    //    }

    //    image->popFilter();
    //    image->rescale(ZOOM_KEEP_LEVEL);
    //    image->updateFrame();

    QApplication::restoreOverrideCursor();
}

QString FITSHistogramCommand::text() const
{
    switch (type)
    {
        case FITS_AUTO:
            return i18n("Auto Scale");
        case FITS_LINEAR:
            return i18n("Linear Scale");
        case FITS_LOG:
            return i18n("Logarithmic Scale");
        case FITS_SQRT:
            return i18n("Square Root Scale");

        default:
            if (type - 1 <= FITSViewer::filterTypes.count())
                return FITSViewer::filterTypes.at(type - 1);
            break;
    }

    return i18n("Unknown");
}

