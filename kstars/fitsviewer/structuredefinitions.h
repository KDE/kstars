/*  Structure Definitions for KStars and SexySolver Internal Library, developed by Robert Lancaster, 2020

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/
#ifndef STRUCTUREDEFINITIONS_H
#define STRUCTUREDEFINITIONS_H

//system includes
#include "stdint.h"
#include <QString>
#include <QVector>
#include "math.h"

namespace FITSImage {

/// Stats struct to hold statisical data about the FITS data
/// This is defined in both KStars and SexySolver
typedef struct
{
    double min[3] = {0}, max[3] = {0};
    double mean[3] = {0};
    double stddev[3] = {0};
    double median[3] = {0};
    double SNR { 0 };
    /// FITS image data type (TBYTE, TUSHORT, TULONG, TFLOAT, TLONGLONG, TDOUBLE)
    uint32_t dataType { 0 };
    int bytesPerPixel { 1 };
    int ndim { 2 };
    int64_t size { 0 };
    uint32_t samples_per_channel { 0 };
    uint16_t width { 0 };
    uint16_t height { 0 };
} Statistic;

// This structure holds data about sources that are found within
// an image.  It is returned by Source Extraction
typedef struct
{
    float x;        // The x position of the star in Pixels
    float y;        // The y position of the star in Pixels
    float mag;      // The magnitude of the star
    float flux;     // The calculated total flux
    float peak;     // The peak value of the star
    float HFR;      // The half flux radius
    float a;        // The semi-major axis of the star
    float b;        // The semi-minor axis of the star
    float theta;    // The angle of orientation of the star
    float ra;       // The right ascension of the star
    float dec;      // The declination of the star
} Star;

// This struct holds data about the background in an image
// It is returned by source extraction
typedef struct
{
    int bw, bh;        // single tile width, height
    float global;      // global mean
    float globalrms;   // global sigma
} Background;

// This struct contains information about the astrometric solution
// for an image.
typedef struct
{
    double fieldWidth;  // The calculated width of the field in arcminutes
    double fieldHeight; // The calculated height of the field in arcminutes
    double ra;          // The Right Ascension of the center of the field
    double dec;         // The Declination of the center of the field
    double orientation; // The orientation angle of the image from North in degrees
    double pixscale;    // The pixel scale of the image
    QString parity;     // The parity of the solved image. (Whether it has been flipped)  JPEG images tend to have negative parity while FITS files tend to have positive parity.
    double raError;     // The error between the search_ra position and the solution ra position in arcseconds
    double decError;    // The error between the search_dec position and the solution dec position in arcseconds
} Solution;

// This is point in the World Coordinate System with both RA and DEC.
// It is used to create an array of positions for the image pixels
typedef struct
{
    float ra;           // The Right Ascension in degrees
    float dec;          // The Declination in degrees
} wcs_point;

} // FITSImage

#endif // STRUCTUREDEFINITIONS_H
