/*
    SPDX-FileCopyrightText: 2021 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <memory>
#include <QObject>
#include <QImage>
#include "projections/projector.h"

class TerrainLookup;

class TerrainRenderer : public QObject
{
        Q_OBJECT
    public:
        // Create an instance of TerrainRenderer. We only have one.
        static TerrainRenderer *Instance();

        // Render terrainImage according to the loaded image and the projection.
        bool render(uint16_t w, uint16_t h, QImage *terrainImage, const Projector *proj);
    signals:

    public slots:

    private:
        // Constructor is private. Only make it with Instance().
        TerrainRenderer();

        // Speed-up the image calculations by downsampling azimuth and altitude
        // computations of the pixels in the input view.
        void setupLookup(uint16_t w, uint16_t h, int sampling, const Projector *proj,
                         TerrainLookup *azLookup, TerrainLookup *altLookup);

        // Returns the pixel in sourceImage for the given coordinates.
        QRgb getPixel(double az, double alt) const;

        // Checks to see if we can use the old rendering.
        // If not, copies the view for the next call.
        bool sameView(const Projector *proj, bool forceRefresh);

        // This is the only instance we'll make.
        static TerrainRenderer * _terrainRenderer;

        // True if the terrain image is setup.
        bool initialized = false;

        // The terrain image projection.
        QImage sourceImage;

        // Save the input view and the computed image in case the image can be re-used.
        ViewParams savedViewParams;
        double savedAz, savedAlt;
        QImage savedImage;

        // Keep the parameters used to display the last image
        // to see if something's changed and we need to redisplay.
        QString sourceFilename;
        int terrainDownsampling = 0;
        bool terrainSkipSpeedup = false;
        bool terrainSmoothPixels = false;
        bool terrainTransparencySpeedup = false;
        bool terrainSourceCorrectAz = 0;
        bool terrainSourceCorrectAlt = 0;
};
