/*
    SPDX-FileCopyrightText: 2021 Jasem Mutlaq

    Static version of the HIPS Renderer for a single point in the sky.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "healpix.h"
#include "hipsmanager.h"
#include "scanrender.h"

#include <memory>

class Projector;

class HIPSFinder
{
    public:
        static HIPSFinder *Instance();

        /**
         * @brief render Renders an image at the specified center with the specific level and zoom.
         * @param center Sky point of image center.
         * @param level HiPS level. Minimum 2 and Maximum 20
         * @param zoom Projector zoom factor
         * @param destinationImage Pointer to an already initialized QImage
         * @param fov_w output image horizontal field of view in arcminutes.
         * @param fov_h output image vertical field of view in arcminutes.
         * @param projector projection system to be used to map image --> screen projection transformation.
         * @return True if successful, false otherwise.
         */
        bool render(SkyPoint *center, uint8_t level, double zoom, QImage *destinationImage, double &fov_w, double &fov_h);
        void renderRec(uint8_t level, int pix, QImage *destinationImage);
        bool renderPix(int level, int pix, QImage *destinationImage);

    private:
        explicit HIPSFinder();
        static HIPSFinder *m_Instance;

        QSet<int>  m_RenderedMap;
        QScopedPointer<HEALPix> m_HEALpix;
        QScopedPointer<ScanRender> m_ScanRender;
        QScopedPointer<Projector> m_Projector;
};
