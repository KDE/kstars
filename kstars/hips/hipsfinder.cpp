/*
    SPDX-FileCopyrightText: 2021 Jasem Mutlaq

    Static version of the HIPS Renderer for a single point in the sky.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "hipsfinder.h"

#include "kstars_debug.h"
#include "Options.h"
#include "skymap.h"
#include "kstars.h"
#include "skyqpainter.h"
#include "kspaths.h"
#include "projections/projector.h"
#include "projections/lambertprojector.h"

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
HIPSFinder * HIPSFinder::m_Instance = nullptr;

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
HIPSFinder *HIPSFinder::Instance()
{
    if (m_Instance == nullptr)
    {
        m_Instance = new HIPSFinder();
    }

    return m_Instance;
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
HIPSFinder::HIPSFinder()
{
    m_ScanRender.reset(new ScanRender());
    m_HEALpix.reset(new HEALPix());
}

///////////////////////////////////////////////////////////////////////////////////////////
/// Static
///////////////////////////////////////////////////////////////////////////////////////////
bool HIPSFinder::render(SkyPoint *center, uint8_t level, double zoom, QImage *destinationImage, double &fov_w, double &fov_h)
{
    double ra = center->ra0().radians();
    double de = center->dec0().radians();
    // do we need this? or updateCoords?
    //center.catalogueCoord(KStarsData::Instance()->updateNum()->julianDay());

    if (std::isnan(ra) || std::isnan(de))
    {
        qCWarning(KSTARS) << "NAN Center, HiPS rendering failed.";
        return false;
    }

    m_RenderedMap.clear();

    //const Projector *m = KStars::Instance()->map()->projector();

    // Setup sample projector
    ViewParams viewParams;
    viewParams.width = destinationImage->width();
    viewParams.height = destinationImage->height();
    viewParams.fillGround = false;
    viewParams.useAltAz = false;
    viewParams.zoomFactor = zoom;
    viewParams.focus = center;

    m_Projector.reset(new LambertProjector(viewParams));

    // Get the ID of the face at this level containing the coordinates.
    int centerPix = m_HEALpix->getPix(level, ra, de);

    SkyPoint cornerSkyCoords[4];
    QPointF tileLine[2];

    // Get corners for this face
    m_HEALpix->getCornerPoints(level, centerPix, cornerSkyCoords);

    fov_w = cornerSkyCoords[0].angularDistanceTo(&cornerSkyCoords[1]).Degrees();
    fov_h = cornerSkyCoords[1].angularDistanceTo(&cornerSkyCoords[2]).Degrees();

    // Map the tile lines to the corners
    for (int i = 0; i < 2; i++)
        tileLine[i] = m_Projector->toScreen(&cornerSkyCoords[i]);

    int size = std::sqrt(std::pow(tileLine[0].x() - tileLine[1].x(), 2) + std::pow(tileLine[0].y() - tileLine[1].y(), 2));
    if (size < 0)
        size = HIPSManager::Instance()->getCurrentTileWidth();

    m_ScanRender->setBilinearInterpolationEnabled(size >= HIPSManager::Instance()->getCurrentTileWidth());

    renderRec(level, centerPix, destinationImage);

    return !m_RenderedMap.isEmpty();
}

///////////////////////////////////////////////////////////////////////////////////////////
/// Static
///////////////////////////////////////////////////////////////////////////////////////////
void HIPSFinder::renderRec(uint8_t level, int pix, QImage *destinationImage)
{
    if (m_RenderedMap.contains(pix))
        return;

    if (renderPix(level, pix, destinationImage))
    {
        m_RenderedMap.insert(pix);
        int dirs[8];
        int nside = 1 << level;

        m_HEALpix->neighbours(nside, pix, dirs);

        renderRec(level, dirs[0], destinationImage);
        renderRec(level, dirs[2], destinationImage);
        renderRec(level, dirs[4], destinationImage);
        renderRec(level, dirs[6], destinationImage);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
bool HIPSFinder::renderPix(int level, int pix, QImage *destinationImage)
{
    SkyPoint cornerSkyCoords[4];
    QPointF cornerScreenCoords[4];
    bool isVisible = false;

    m_HEALpix->getCornerPoints(level, pix, cornerSkyCoords);

    for (int i = 0; i < 4; i++)
    {
        cornerScreenCoords[i] = m_Projector->toScreen(&cornerSkyCoords[i]);
        isVisible |= m_Projector->checkVisibility(&cornerSkyCoords[i]);
    }

    if (isVisible)
    {
        int dir = (pix / 10000) * 10000;
        QString path = KSPaths::locate(QStandardPaths::AppDataLocation,
                                       QString("/HIPS/Norder%1/Dir%2/Npix%3.jpg").arg(level).arg(dir).arg(pix));
        QImage sourceImage(path);

        if (!sourceImage.isNull())
        {
            QPointF uv[16][4] = {{QPointF(.25, .25), QPointF(0.25, 0), QPointF(0, .0), QPointF(0, .25)},
                {QPointF(.25, .5), QPointF(0.25, 0.25), QPointF(0, .25), QPointF(0, .5)},
                {QPointF(.5, .25), QPointF(0.5, 0), QPointF(.25, .0), QPointF(.25, .25)},
                {QPointF(.5, .5), QPointF(0.5, 0.25), QPointF(.25, .25), QPointF(.25, .5)},

                {QPointF(.25, .75), QPointF(0.25, 0.5), QPointF(0, 0.5), QPointF(0, .75)},
                {QPointF(.25, 1), QPointF(0.25, 0.75), QPointF(0, .75), QPointF(0, 1)},
                {QPointF(.5, .75), QPointF(0.5, 0.5), QPointF(.25, .5), QPointF(.25, .75)},
                {QPointF(.5, 1), QPointF(0.5, 0.75), QPointF(.25, .75), QPointF(.25, 1)},

                {QPointF(.75, .25), QPointF(0.75, 0), QPointF(0.5, .0), QPointF(0.5, .25)},
                {QPointF(.75, .5), QPointF(0.75, 0.25), QPointF(0.5, .25), QPointF(0.5, .5)},
                {QPointF(1, .25), QPointF(1, 0), QPointF(.75, .0), QPointF(.75, .25)},
                {QPointF(1, .5), QPointF(1, 0.25), QPointF(.75, .25), QPointF(.75, .5)},

                {QPointF(.75, .75), QPointF(0.75, 0.5), QPointF(0.5, .5), QPointF(0.5, .75)},
                {QPointF(.75, 1), QPointF(0.75, 0.75), QPointF(0.5, .75), QPointF(0.5, 1)},
                {QPointF(1, .75), QPointF(1, 0.5), QPointF(.75, .5), QPointF(.75, .75)},
                {QPointF(1, 1), QPointF(1, 0.75), QPointF(.75, .75), QPointF(.75, 1)},
            };

            int childPixelID[4];

            // Find all the 4 children of the current pixel
            m_HEALpix->getPixChilds(pix, childPixelID);

            int j = 0;
            for (int id : childPixelID)
            {
                int grandChildPixelID[4];
                // Find the children of this child (i.e. grand child)
                // Then we have 4x4 pixels under the primary pixel
                // The image is interpolated and rendered over these pixels
                // coordinate to minimize any distortions due to the projection
                // system.
                m_HEALpix->getPixChilds(id, grandChildPixelID);

                QPointF fineScreenCoords[4];

                for (int id2 : grandChildPixelID)
                {
                    SkyPoint fineSkyPoints[4];
                    m_HEALpix->getCornerPoints(level + 2, id2, fineSkyPoints);

                    for (int i = 0; i < 4; i++)
                        fineScreenCoords[i] = m_Projector->toScreen(&fineSkyPoints[i]);

                    m_ScanRender->renderPolygon(3, fineScreenCoords, destinationImage, &sourceImage, uv[j]);
                    j++;
                }
            }

            return true;
        }
    }

    return false;
}
