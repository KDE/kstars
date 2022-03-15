/*  Structure Definitions for KStars and StellarSolver Internal Library, developed by Robert Lancaster, 2020

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef STRUCTUREDEFINITIONS_H
#define STRUCTUREDEFINITIONS_H

//system includes
#include <stdint.h>
#include <math.h>
#include <QString>

namespace FITSImage
{

typedef enum
{
    POSITIVE,
    NEGATIVE,
    BOTH
} Parity;

static const QString getParityText(Parity parity){
    return parity == FITSImage::NEGATIVE ? "negative" : "positive";
}

static const QString getShortParityText(Parity parity){
    return parity == FITSImage::NEGATIVE ? "neg" : "pos";
}

// Stats struct to hold statisical data about the FITS data
typedef struct Statistic
{
    double min[3] = {0}, max[3] = {0};  // Minimum and Maximum R, G, B pixel values in the image
    double mean[3] = {0};               // Average R, G, B value of the pixels in the image
    double stddev[3] = {0};             // Standard Deviation of the R, G, B pixel values in the image
    double median[3] = {0};             // Median R, G, B pixel value in the image
    double SNR { 0 };                   // Signal to noise ratio
    uint32_t dataType { 0 };            // FITS image data type (TBYTE, TUSHORT, TULONG, TFLOAT, TLONGLONG, TDOUBLE)
    int bytesPerPixel { 1 };            // Number of bytes used for each pixel, size of datatype above
    int ndim { 2 };                     // Number of dimensions in a fits image
    int64_t size { 0 };                 // Filesize in bytes
    uint32_t samples_per_channel { 0 }; // area of the image in pixels
    uint16_t width { 0 };               // width of the image in pixels
    uint16_t height { 0 };              // height of the image in pixels
    uint8_t channels { 1 };             // Mono Images have 1 channel, RGB has 3 channels
} Statistic;

// This structure holds data about sources that are found within
// an image.  It is returned by Source Extraction
typedef struct Star
{
    float x;        // The x position of the star in Pixels
    float y;        // The y position of the star in Pixels
    float mag;      // The magnitude of the star, note that this is a relative magnitude based on the star extraction options.
    float flux;     // The calculated total flux
    float peak;     // The peak value of the star
    float HFR;      // The half flux radius of the star
    float a;        // The semi-major axis of the star
    float b;        // The semi-minor axis of the star
    float theta;    // The angle of orientation of the star
    float ra;       // The right ascension of the star
    float dec;      // The declination of the star
    int numPixels;  // The number of pixels occupied by the star in the image.
} Star;

// This struct holds data about the background in an image
// It is returned by source extraction
typedef struct Background
{
    int bw, bh;             // single tile width, height
    float global;           // global mean
    float globalrms;        // global sigma
    int num_stars_detected; // Number of stars detected before any reduction.
} Background;

// This struct contains information about the astrometric solution
// for an image.
typedef struct Solution
{
    double fieldWidth;  // The calculated width of the field in arcminutes
    double fieldHeight; // The calculated height of the field in arcminutes
    double ra;          // The Right Ascension of the center of the field in degrees
    double dec;         // The Declination of the center of the field in degrees
    double orientation; // The orientation angle of the image from North in degrees
    double pixscale;    // The pixel scale of the image in arcseconds per pixel
    Parity parity;      // The parity of the solved image. (Whether it has been flipped)  JPEG images tend to have negative parity while FITS files tend to have positive parity.
    double raError;     // The error between the search_ra position and the solution ra position in arcseconds
    double decError;    // The error between the search_dec position and the solution dec position in arcseconds
} Solution;

// This is point in the World Coordinate System with both RA and DEC.
typedef struct wcs_point
{
    float ra;           // The Right Ascension in degrees
    float dec;          // The Declination in degrees
} wcs_point;

} // FITSImage

#endif // STRUCTUREDEFINITIONS_H
