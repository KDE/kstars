/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "fitsviewer/fitsdata.h"

#include <QSharedPointer>
#include <QVector>

#include <fitsio.h>

#include <cstdlib>

namespace DarkFrames
{

/**
 * @brief Build an unsigned 16-bit frame in memory and load it the way KStars loads camera frames.
 * @param pixels planar values: width x height for each plane
 * @param planes 1 for a mono or raw colour (CFA) frame, 3 for a debayered RGB frame
 * @param bayer Bayer pattern written to BAYERPAT, empty for none
 * @param mode the mode the frame is loaded in, e.g. FITS_GUIDE for a guide camera frame
 */
inline QSharedPointer<FITSData> makeFrame(int width, int height, const QVector<quint16> &pixels, int planes = 1,
        const QString &bayer = QString(), FITSMode mode = FITS_NORMAL)
{
    fitsfile *fptr = nullptr;
    void *fitsBuffer = nullptr;
    size_t fitsSize = 0;
    int status = 0;
    long naxes[3] = { width, height, planes };
    fits_create_memfile(&fptr, &fitsBuffer, &fitsSize, 2880, realloc, &status);
    fits_create_img(fptr, USHORT_IMG, planes > 1 ? 3 : 2, naxes, &status);
    if (!bayer.isEmpty())
    {
        QByteArray pattern = bayer.toLatin1();
        fits_update_key(fptr, TSTRING, "BAYERPAT", pattern.data(), "Bayer color pattern", &status);
    }
    QVector<quint16> values = pixels;
    fits_write_img(fptr, TUSHORT, 1, values.size(), values.data(), &status);
    fits_flush_file(fptr, &status);
    fits_close_file(fptr, &status);
    if (status != 0)
    {
        free(fitsBuffer);
        return {};
    }
    QByteArray buffer(static_cast<const char *>(fitsBuffer), static_cast<int>(fitsSize));
    free(fitsBuffer);

    QSharedPointer<FITSData> data(new FITSData(mode));
    data->setExtension("fits");
    if (!data->loadFromBuffer(buffer))
        return {};
    return data;
}

/** @brief Value of one pixel of an unsigned 16-bit frame */
inline quint16 pixel(const QSharedPointer<FITSData> &data, int plane, int x, int y)
{
    const auto *buffer = reinterpret_cast<const quint16 *>(data->getImageBuffer());
    return buffer[plane * data->samplesPerChannel() + y * data->width() + x];
}

}
