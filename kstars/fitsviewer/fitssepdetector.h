/***************************************************************************
                          fitssepdetector.h  -  FITS Image
                             -------------------
    begin                : Sun March 29 2020
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

#ifndef FITSSEPDETECTOR_H
#define FITSSEPDETECTOR_H

#include <QObject>

#include "fitsstardetector.h"

class FITSSEPDetector : public FITSStarDetector
{
    Q_OBJECT

public:
    explicit FITSSEPDetector(FITSData *parent): FITSStarDetector(parent) {};

public:
    /** @brief Find sources in the parent FITS data file.
     * @see FITSStarDetector::findSources().
     */
    int findSources(QList<Edge*> &starCenters, QRect const &boundary = QRect()) override;

    /** @brief Configure the detection method.
     * @see FITSStarDetector::configure().
     * @note No parameters are currently available for configuration.
     * @todo Provide parameters for detection configuration.
     */
    FITSStarDetector & configure(const QString &setting, const QVariant &value) override;

protected:
    /** @internal Consolidate a float data buffer from FITS data.
     * @param buffer is the destination float block.
     * @param x, y, w, h define a (x,y)-(x+w,y+h) sub-frame to extract from the FITS data out to block 'buffer'.
     * @param image_data is the FITS data block to extract from.
     */
    template <typename T>
    void getFloatBuffer(float * buffer, int x, int y, int w, int h, FITSData const * image_data) const;
};

#endif // FITSSEPDETECTOR_H
