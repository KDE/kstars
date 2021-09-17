/*
    SPDX-FileCopyrightText: 2015 M.S.Adityan <msadityan@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skyobjects/skyobject.h"

#include <QImage>
#include <QString>

class dms;

/**
 * @class ConstellationsArt
 * @short Information about a ConstellationsArt object. This class represents a constellation image.
 *
 * Provides all necessary information about a constellationsart object
 * data inherited from SkyObject includes RA/DEC coordinate pairs.
 * Data specific to ConstellationsArt objects includes the abbreviation
 * filename, constellation image, position angle, width and height.
 *
 * @author M.S.Adityan
 * @version 0.1
 */
class ConstellationsArt : public SkyObject
{
  public:
    /**
     * Constructor. Set ConstellationsArt data according to parameters.
     * @param midpointra RA of the midpoint of the constellation
     * @param midpointdec DEC of the midpoint of the constellation
     * @param pa position angle
     * @param w width of the constellation image
     * @param h height of the constellation image
     * @param abbreviation abbreviation of the constellation
     * @param filename the file name of the image of the constellation.
     */
    explicit ConstellationsArt(dms &midpointra, dms &midpointdec, double pa, double w, double h,
                               const QString &abbreviation, const QString &filename);

    /** @return an object's image */
    const QImage &image()
    {
        if (imageLoaded)
            return constellationArtImage;
        else
        {
            loadImage();
            return constellationArtImage;
        }
    }

    /** Load the object's image. This also scales the object's image to 1024x1024 pixels. */
    void loadImage();

    /** @return an object's abbreviation */
    inline QString getAbbrev() const { return abbrev; }

    /** @return an object's image file name*/
    inline QString getImageFileName() const { return imageFileName; }

    /** @return an object's position angle */
    inline double pa() const override { return positionAngle; }

    /** Set the position angle */
    inline void setPositionAngle(double pa) { positionAngle = pa; }

    /** @return an object's width */
    inline double getWidth() { return width; }

    /** @return an object's height*/
    inline double getHeight() { return height; }

  private:
    QString abbrev;
    QString imageFileName;
    QImage constellationArtImage;
    double positionAngle { 0 };
    double width { 0 };
    double height { 0 };
    bool imageLoaded { false };
};
