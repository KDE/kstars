/*
    SPDX-FileCopyrightText: 2004 Jasem Mutlaq
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later

    Some code fragments were adapted from Peter Kirchgessner's FITS plugin
    SPDX-FileCopyrightText: Peter Kirchgessner <http://members.aol.com/pkirchg>
*/

#pragma once

#include "fitsstardetector.h"
#include "skybackground.h"

class FITSSEPDetector : public FITSStarDetector
{
        Q_OBJECT

    public:
        explicit FITSSEPDetector(FITSData * data): FITSStarDetector(data) {};

        /** @brief Find sources in the parent FITS data file.
         * @see FITSStarDetector::findSources().
         */
        QFuture<bool> findSources(QRect const &boundary = QRect()) override;

        /** @brief Find sources in the parent FITS data file as well as background sky information.
         */
        bool findSourcesAndBackground(QRect const &boundary = QRect());

    protected:
        /** @internal Consolidate a float data buffer from FITS data.
         * @param buffer is the destination float block.
         * @param x, y, w, h define a (x,y)-(x+w,y+h) sub-frame to extract from the FITS data out to block 'buffer'.
         * @param image_data is the FITS data block to extract from.
         */
        template <typename T>
        void getFloatBuffer(float * buffer, int x, int y, int w, int h, FITSData const * image_data) const;

    private:

        void clearSolver();

        //        int numStars = 100;
        //        double fractionRemoved = 0.2;
        //        int deblendNThresh = 32;
        //        double deblendMincont = 0.005;
        //        bool radiusIsBoundary = true;
};

