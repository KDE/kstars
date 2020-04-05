/***************************************************************************
                          fitsgradientdetector.h  -  FITS Image
                             -------------------
    begin                : Sat March 28 2020
    copyright            : (C) 2004 by Jasem Mutlaq, (C) 2020 by Eric Dejouhanet
    email                : eric.dejouhanet@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Some code fragments were adapted from Peter Kirchgessner's FITS plugin*
 *   See http://members.aol.com/pkirchg for more details.                  *
 ***************************************************************************/

#ifndef FITSGRADIENTDETECTOR_H
#define FITSGRADIENTDETECTOR_H

#include "fitsstardetector.h"

class FITSGradientDetector: public FITSStarDetector
{
    Q_OBJECT

public:
    explicit FITSGradientDetector(FITSData *parent): FITSStarDetector(parent) {};

public:
    /** @brief Find sources in the parent FITS data file.
     * @see FITSStarDetector::findSources().
     */
    int findSources(QList<Edge*> &starCenters, QRect const &boundary = QRect()) override;

    /** @brief Configure the detection method.
     * @see FITSStarDetector::configure().
     * @note No parameters are currently available for configuration.
     */
    FITSStarDetector & configure(const QString &setting, const QVariant &value) override;

protected:
    /** @internal Find sources in the parent FITS data file, dependent of the pixel depth.
     * @see FITSGradientDetector::findSources.
     */
    template <typename T>
    int findSources(QList<Edge*> &starCenters, const QRect &boundary);

    /** @internal Implementation of the Canny Edge detection (CannyEdgeDetector).
     * @copyright 2015 Gonzalo Exequiel Pedone (https://github.com/hipersayanX/CannyDetector).
     * @param data is the FITS data to run the detection onto.
     * @param gradient is the vector storing the amount of change in pixel sequences.
     * @param direction is the vector storing the four directions (horizontal, vertical and two diagonals) the changes stored in 'gradient' are detected in.
     */
    template <typename T>
    void sobel(FITSData const * data, QVector<float> &gradient, QVector<float> &direction) const;

    /** @internal Identify gradient connections.
     * @param width, height are the dimensions of the frame to work on.
     * @param gradient is the vector holding the amount of change in pixel sequences.
     * @param ids is the vector storing which gradient was identified for each pixel.
     */
    int partition(int width, int height, QVector<float> &gradient, QVector<int> &ids) const;

    /** @internal Trace gradient neighbors.
     * @param width, height are the dimensions of the frame to work on.
     * @param image is the image to work on, actually gradients extracted using the sobel algorithm.
     * @param ids is the vector storing which gradient was identified for each pixel.
     * @param x, y locate the pixel to trace from.
     */
    void trace(int width, int height, int id, QVector<float> &image, QVector<int> &ids, int x, int y) const;
};

#endif // FITSGRADIENTDETECTOR_H
