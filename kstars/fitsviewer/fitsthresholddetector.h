/***************************************************************************
                          fitsthresholddetector.h  -  FITS Image
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

#ifndef FITSTHRESHOLDDETECTOR_H
#define FITSTHRESHOLDDETECTOR_H

#include "fitsstardetector.h"

class FITSThresholdDetector: public FITSStarDetector
{
        Q_OBJECT

    public:
        explicit FITSThresholdDetector(FITSData *parent): FITSStarDetector(parent) {};

    public:
        /** @brief Find sources in the parent FITS data file.
         * @see FITSStarDetector::findSources().
         */
        QFuture<bool> findSources(QRect const &boundary = QRect()) override;

        /** @brief Configure the detection method.
         * @see FITSStarDetector::configure().
         * @note Parameter "threshold" defaults to THRESHOLD_PERCENTAGE of the mean pixel value of the frame.
         * @todo Provide parameters for detection configuration.
         */
        //void configure(const QString &setting, const QVariant &value) override;

    public:
        /** @group Detection parameters.
         * @{ */
        //int THRESHOLD_PERCENTAGE { 120 };
        /** @} */

    protected:
        /** @internal Find sources in the parent FITS data file, dependent of the pixel depth.
         * @see FITSGradientDetector::findSources.
         */
        template <typename T>
        bool findOneStar(const QRect &boundary) const;
};

#endif // FITSTHRESHOLDDETECTOR_H
