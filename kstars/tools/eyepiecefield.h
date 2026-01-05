/*
    SPDX-FileCopyrightText: 2014 Akarsh Simha <akarsh.simha@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "dms.h"

#include <QPixmap>

#include <memory>

class FOV;
class SkyPoint;

/**
 * @class EyepieceField
 * @short Renders the view through the eyepiece of various telescope types
 *
 * @author Akarsh Simha <akarsh.simha@kdemail.net>
 */
namespace EyepieceField
{
/**
 * @short Generate the eyepiece field view and corresponding image view
 * @param sp Sky point to draw the render the eyepiece field around
 * @param skyChart A non-null pointer to replace with the eyepiece field image
 * @param skyImage An optionally non-null pointer to replace with the re-oriented sky image
 * @param fovWidth width of the field-of-view in arcminutes
 * @param fovHeight height of field-of-view in arcminutes (if not supplied, is set to fovWidth)
 * @param imagePath Optional path to DSS or other image. North
 * should be on the top of the image, and the size should be in
 * the metadata; otherwise 1.01 arcsec/pixel is assumed.
 * @note fovWidth can be zero/negative if imagePath is non-empty. If it is, the image
 * size is used for the FOV.
 * @note fovHeight can be zero/negative. If it is, fovWidth will be used. If fovWidth is also
 * zero, image size is used.
 */
void generateEyepieceView(SkyPoint *sp, QImage *skyChart, QImage *skyImage = nullptr, double fovWidth = -1.0,
                          double fovHeight = -1.0, const QString &imagePath = QString());

/**
 * @short Overloaded method provided for convenience. Obtains fovWidth/fovHeight from
 * FOV if non-null, else uses image
 */
void generateEyepieceView(SkyPoint *sp, QImage *skyChart, QImage *skyImage = nullptr,
                          const FOV *fov = nullptr, const QString &imagePath = QString());

/**
 * @short Orients the eyepiece view as needed, performs overlaying etc.
 * @param skyChart image which contains the sky chart, possibly generated using generateEyepieceView
 * @param skyImage optional image which contains the sky image, possibly generated using generateEyepieceView
 * @param renderChart pixmap onto which the sky chart is to be rendered
 * @param renderImage optional pixmap onto which the sky image is to be rendered
 * @param rotation optional, number of degrees by which to rotate the image(s)
 * @param scale optional, factor by which to scale the image(s)
 * @param flip optional, if true, the image is mirrored horizontally
 * @param invert optional, if true, the image is inverted, i.e. rotated by 180 degrees
 * @param overlay optional, if true, the sky image is overlaid on the sky map
 * @param invertColors optional, if true, the sky image is color-inverted
 */
void renderEyepieceView(const QImage *skyChart, QPixmap *renderChart, const double rotation = 0,
                        const double scale = 1.0, const bool flip = false, const bool invert = false,
                        const QImage *skyImage = nullptr, QPixmap *renderImage = nullptr,
                        const bool overlay = false, const bool invertColors = false);

/**
 * @short Convenience method that generates and the renders the eyepiece view
 * @note calls generateEyepieceView() followed by the raw form of renderEyepieceView() to render an eyepiece view
 */
void renderEyepieceView(SkyPoint *sp, QPixmap *renderChart, double fovWidth = -1.0, double fovHeight = -1.0,
                        const double rotation = 0, const double scale = 1.0, const bool flip = false,
                        const bool invert = false, const QString &imagePath = QString(),
                        QPixmap *renderImage = nullptr, const bool overlay = false,
                        const bool invertColors = false);

/**
 * @short Finds the angle between "up" (i.e. direction of increasing altitude) and "north" (i.e. direction of increasing declination) at a given point in the sky
 * @attention Procedure does not account for precession and nutation at the moment
 * @note SkyPoint must already have Equatorial and Horizontal coordinate synced
 */
dms findNorthAngle(const SkyPoint *sp, const dms *lat);
};
