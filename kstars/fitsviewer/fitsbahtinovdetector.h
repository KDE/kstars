/*
    SPDX-FileCopyrightText: 2020 Patrick Molenaar <pr_molenaar@hotmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef FITSBAHTINOVDETECTOR_H
#define FITSBAHTINOVDETECTOR_H

#include "fitsstardetector.h"

class BahtinovLineAverage
{
    public:
        BahtinovLineAverage()
        {
            average = 0.0;
            offset = 0;
        }
        virtual ~BahtinovLineAverage() = default;

        double average;
        size_t offset;
};

class FITSBahtinovDetector: public FITSStarDetector
{
        Q_OBJECT

    public:
        explicit FITSBahtinovDetector(FITSData *parent): FITSStarDetector(parent) {};

    public:
        /** @brief Find sources in the parent FITS data file.
         * @see FITSStarDetector::findSources().
         */
        QFuture<bool> findSources(QRect const &boundary = QRect()) override;

        /** @brief Configure the detection method.
         * @see FITSStarDetector::configure().
         * @note Parameter "numaveragerows" defaults to NUMBER_OF_AVERAGE_ROWS of the mean pixel value of the frame.
         * @todo Provide parameters for detection configuration.
         */
        //void configure(const QString &setting, const QVariant &value) override;

    public:
        /** @group Detection parameters.
         * @{ */
        //int NUMBER_OF_AVERAGE_ROWS { 1 };
        /** @} */

    protected:
        /** @internal Find sources in the parent FITS data file, dependent of the pixel depth.
         * @see FITSGradientDetector::findSources.
         */
        template <typename T>
        bool findBahtinovStar(const QRect &boundary);

    private:
        template <typename T>
        BahtinovLineAverage calculateMaxAverage(const FITSData *data, int angle);
        template <typename T>
        bool rotateImage(const FITSData *data, int angle, T * rotimage);
};

#endif // FITSBAHTINOVDETECTOR_H
