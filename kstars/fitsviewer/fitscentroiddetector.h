/*
    SPDX-FileCopyrightText: 2004 Jasem Mutlaq
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later

    Some code fragments were adapted from Peter Kirchgessner's FITS plugin
    SPDX-FileCopyrightText: Peter Kirchgessner <http://members.aol.com/pkirchg>
*/

#ifndef FITSCENTROIDDETECTOR_H
#define FITSCENTROIDDETECTOR_H

#include <QObject>
#include "fitsstardetector.h"

class FITSCentroidDetector: public FITSStarDetector
{
        Q_OBJECT

    public:
        explicit FITSCentroidDetector(FITSData * data): FITSStarDetector(data) {};

    public:
        /** @brief Find sources in the parent FITS data file.
         * @see FITSStarDetector::findSources().
         */
        QFuture<bool> findSources(QRect const &boundary = QRect()) override;

        /** @brief Configure the detection method.
         * @see FITSStarDetector::configure().
         * @see Detection parameters.
         */
        //void configure(const QString &setting, const QVariant &value) override;

    protected:
        /** @group Detection parameters. Use the names as strings for FITSStarDetector::configure().
         * @{ */
        /** @brief Initial variation, decreasing as search progresses. Configurable. */
        //int MINIMUM_STDVAR { 5 };
        /** @brief Initial source width, decreasing as search progresses. Configurable. */
        //int MINIMUM_PIXEL_RANGE { 5 };
        /** @brief Custom image contrast index from the frame histogram. Configurable. */
        //double JMINDEX { 100 };
        /** @brief Initial source count over which search stops. */
        int MINIMUM_EDGE_LIMIT { 2 };
        /** @brief Maximum source count over which search aborts. */
        int MAX_EDGE_LIMIT { 10000 };
        /** @brief Minimum value of JMINDEX under which the custom image contrast index from the histogram is used to redefine edge width and count. */
        double DIFFUSE_THRESHOLD { 0.15 };
        /** @brief */
        int MINIMUM_ROWS_PER_CENTER { 3 };
        /** @brief */
        int LOW_EDGE_CUTOFF_1  { 50 };
        /** @brief */
        int LOW_EDGE_CUTOFF_2  { 10 };
        /** @} */

    protected:
        /** @internal Find sources in the parent FITS data file, dependent of the pixel depth.
         * @see FITSGradientDetector::findSources.
         */
        template <typename T>
        bool findSources(const QRect &boundary);

        /** @internal Check whether two sources overlap.
         * @param s1, s2 are the two sources to check collision on.
         * @return true if the sources collide, else false.
         */
        bool checkCollision(Edge * s1, Edge * s2) const;
};

#endif // FITSCENTROIDDETECTOR_H
