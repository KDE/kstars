/*
    SPDX-FileCopyrightText: 2021 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <dms.h>
#include <skypoint.h>

class FITSData;
class TestPolarAlign;

/*********************************************************************
 Polar alignment support class.  Note, the telescope can be pointing anywhere.
 It doesn't need to point at the pole.

 Use this class as follows:
 1) Construct with the geo information.
        PolarAlign polarAlign(geoLocation);
 2) Start the polar align procedure. Capture, solve and add wcs from the
    solve to the FITSData image. Then:
        polarAlign.addPoint(image);
 3) Rotate the mount in RA ~30 degrees (could be less or more) east (or west)
    and capture, solve and add wcs from the solve to the the new image. Then:
        polarAlign.addPoint(image);
 4) Rotate the mount in RA another ~30 degrees east (or west)
    and capture, solve and add wcs from the solve to the the new image. Then:
        polarAlign.addPoint(image);
 5) Find the mount's axis of rotation as follows:
        if (!polarAlign.findAxis())
          error();
 6) Compute the azimuth and altitude offset for the mount.
        double altitudeError = axisAlt - latitudeDegrees;
        double azimuthError = axisAz - 0;
 7) Compute the overall error
        dms polarError(hypot(altitudeError, azimuthError));
 8) Compute a target correction
        int correctedX, correctedY;
        if (!polarAlign.findCorrectedPixel(
               imageData, x, y, altitudeError, azimuthError, &correctedX, &correctedY))
          error();
 9) The user should use the GEM azimuth and altitude adjustments to move the
    object at position x,y in the image to position correctedX, correctedY.
 *********************************************************************/

class PolarAlign
{
    public:

        // The polealignment scheme requires the GeoLocation to operate properly.
        // Certain aspects can be tested without it.
        PolarAlign(const GeoLocation *geo = nullptr);

        // Add a sample point.
        bool addPoint(const QSharedPointer<FITSData> &image);

        // Finds the mount's axis of rotation. Three points must have been added.
        // Returns false if the axis can't be found.
        bool findAxis();

        // Returns the image coordinate that pixel x,y should be moved to to correct
        // the mount's axis. Image is usually the 3rd PAA image. x,y are image coordinates.
        // 3 Points must have been added and findAxis() must have been called.
        // Uses the axis determined by findAxis().  Returns correctedX and correctedY,
        // the target position that the x,y pixel should move to.
        bool findCorrectedPixel(const QSharedPointer<FITSData> &image, const QPointF &pixel,
                                QPointF *corrected, bool altOnly = false);

        // Returns the mount's azimuth and altitude error given the known geographic location
        // and the azimuth center and altitude center computed in findAxis().
        void calculateAzAltError(double *azError, double *altError) const;

        // Given the current axis, fill in azError and altError with the polar alignment
        // error, if a star at location pixel were move in the camera view to pixel2.
        // image would be the 3rd PAA image.
        // Returns false if the paa error couldn't be computer.
        bool pixelError(const QSharedPointer<FITSData> &image, const QPointF &pixel, const QPointF &pixel2,
                        double *azError, double *altError);

        /// reset starts the process over, removing the points.
        void reset();

        // Returns the mount's axis--for debugging.
        void getAxis(double *azAxis, double *altAxis) const;

    private:
        // returns true in the northern hemisphere.
        // if no geo location available, defaults to northern.
        bool northernHemisphere() const;

        // These internal methods find the pixel with the desired azimuth and altitude.
        bool findAzAlt(const QSharedPointer<FITSData> &image, double azimuth, double altitude, QPointF *pixel) const;

        // Does the necessary processing so that azimuth and altitude values
        // can be retrieved for the x,y pixel in image.
        bool prepareAzAlt(const QSharedPointer<FITSData> &image, const QPointF &pixel, SkyPoint *point) const;


        // Internal utility used by the external findCorrectedPixel and by pixelError().
        // Similar args as the public findCorrectedPixel().
        bool findCorrectedPixel(const QSharedPointer<FITSData> &image, const QPointF &pixel,
                                QPointF *corrected, double azError, double altError);

        // Internal utility used by the public pixelError, which iterates at different
        // resolutions passed in to this method. As the resoltion can be coarse, actualPixel
        // is the one used (as opposed to pixel2) for the error returned.
        void pixelError(const QSharedPointer<FITSData> &image, const QPointF &pixel, const QPointF &pixel2,
                        double minAz, double maxAz, double azInc,
                        double minAlt, double maxAlt, double altInc,
                        double *azError, double *altError, QPointF *actualPixel);

        // These three positions are used to estimate the polar alignment error.
        QVector<SkyPoint> points;
        QVector<KStarsDateTime> times;

        // The geographic location used to compute altitude and azimuth.
        const GeoLocation *geoLocation;

        // Values set by the last call to findAxis() that correspond to the mount's axis.
        double azimuthCenter { 0 };
        double altitudeCenter { 0 };

        friend TestPolarAlign;
};
