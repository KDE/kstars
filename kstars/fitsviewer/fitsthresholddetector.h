/*
    SPDX-FileCopyrightText: 2004 Jasem Mutlaq
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later

    Some code fragments were adapted from Peter Kirchgessner's FITS plugin
    SPDX-FileCopyrightText: Peter Kirchgessner <http://members.aol.com/pkirchg>
*/

#ifndef FITSTHRESHOLDDETECTOR_H
#define FITSTHRESHOLDDETECTOR_H

#include "fitsstardetector.h"

class FITSThresholdDetector: public FITSStarDetector
{
        Q_OBJECT

    public:
        explicit FITSThresholdDetector(FITSData * data): FITSStarDetector(data) {};

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
