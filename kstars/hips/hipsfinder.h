/*
  Copyright (C) 2021, Jasem Mutlaq

  Static version of the HIPS Renderer for a single point in the sky.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
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
