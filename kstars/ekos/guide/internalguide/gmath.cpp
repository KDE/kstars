/*  Ekos guide tool
    Copyright (C) 2012 Andrew Stepanenko

    Modified by Jasem Mutlaq <mutlaqja@ikarustech.com> for KStars.

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "gmath.h"

#include "imageautoguiding.h"
#include "Options.h"
#include "fitsviewer/fitsdata.h"
#include "fitsviewer/fitsview.h"
#include "auxiliary/kspaths.h"

#include "ekos_guide_debug.h"

#include <cmath>
#include <set>

#define DEF_SQR_0 (8 - 0)
#define DEF_SQR_1 (16 - 0)
#define DEF_SQR_2 (32 - 0)
#define DEF_SQR_3 (64 - 0)
#define DEF_SQR_4 (128 - 0)

const guide_square_t guide_squares[] = {
    { DEF_SQR_0, DEF_SQR_0 *DEF_SQR_0 * 1.0 }, { DEF_SQR_1, DEF_SQR_1 *DEF_SQR_1 * 1.0 },
    { DEF_SQR_2, DEF_SQR_2 *DEF_SQR_2 * 1.0 }, { DEF_SQR_3, DEF_SQR_3 *DEF_SQR_3 * 1.0 },
    { DEF_SQR_4, DEF_SQR_4 *DEF_SQR_4 * 1.0 }, { -1, -1 }
};

const square_alg_t guide_square_alg[] = { { SMART_THRESHOLD, "Smart" },
                                          { SEP_THRESHOLD, "SEP" },
                                          { CENTROID_THRESHOLD, "Fast" },
                                          { AUTO_THRESHOLD, "Auto" },
                                          { NO_THRESHOLD, "No thresh." },
                                          { -1, { 0 } } };

struct Peak
{
    int x;
    int y;
    float val;

    Peak() = default;
    Peak(int x_, int y_, float val_) : x(x_), y(y_), val(val_) { }
    bool operator<(const Peak& rhs) const { return val < rhs.val; }
};

// JM: Why not use QPoint?
typedef struct
{
    int x, y;
} point_t;

cgmath::cgmath() : QObject()
{
    // sys...
    ROT_Z            = Ekos::Matrix(0);

    // sky coord. system vars.
    star_pos        = Vector(0);
    scr_star_pos    = Vector(0);
    reticle_pos     = Vector(0);
    reticle_orts[0] = Vector(0);
    reticle_orts[1] = Vector(0);
    reticle_angle   = 0;

    ditherRate[0] = ditherRate[1] = -1;

    // processing
    in_params.reset();
    out_params.reset();
    channel_ticks[GUIDE_RA] = channel_ticks[GUIDE_DEC] = 0;
    accum_ticks[GUIDE_RA] = accum_ticks[GUIDE_DEC] = 0;
    drift[GUIDE_RA]                                = new double[MAX_ACCUM_CNT];
    drift[GUIDE_DEC]                               = new double[MAX_ACCUM_CNT];
    memset(drift[GUIDE_RA], 0, sizeof(double) * MAX_ACCUM_CNT);
    memset(drift[GUIDE_DEC], 0, sizeof(double) * MAX_ACCUM_CNT);
    drift_integral[GUIDE_RA] = drift_integral[GUIDE_DEC] = 0;

    QString logFileName = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "guide_log.txt";
    logFile.setFileName(logFileName);
}

cgmath::~cgmath()
{
    delete[] drift[GUIDE_RA];
    delete[] drift[GUIDE_DEC];

    foreach (float *region, referenceRegions)
        delete[] region;

    referenceRegions.clear();
}

bool cgmath::setVideoParameters(int vid_wd, int vid_ht, int binX, int binY)
{
    if (vid_wd <= 0 || vid_ht <= 0)
        return false;

    video_width  = vid_wd / binX;
    video_height = vid_ht / binY;

    subBinX = binX;
    subBinY = binY;

    //set_reticle_params( video_width/2, video_height/2, -1 ); // keep orientation

    return true;
}

void cgmath::setGuideView(FITSView *image)
{
    guideView = image;

    /*if (guideView)
    {
        FITSData *image_data = guideView->getImageData();
        setDataBuffer(image_data->getImageBuffer());
        setVideoParameters(image_data->getWidth(), image_data->getHeight());
    }*/
}

bool cgmath::setGuiderParameters(double ccd_pix_wd, double ccd_pix_ht, double guider_aperture, double guider_focal)
{
    if (ccd_pix_wd < 0)
        ccd_pix_wd = 0;
    if (ccd_pix_ht < 0)
        ccd_pix_ht = 0;
    if (guider_focal <= 0)
        guider_focal = 1;

    ccd_pixel_width  = ccd_pix_wd / 1000.0; // from mkm to mm
    ccd_pixel_height = ccd_pix_ht / 1000.0; // from mkm to mm
    aperture         = guider_aperture;
    focal            = guider_focal;

    return true;
}

void cgmath::getGuiderParameters(double *ccd_pix_wd, double *ccd_pix_ht, double *guider_aperture, double *guider_focal)
{
    *ccd_pix_wd      = ccd_pixel_width * 1000.0;
    *ccd_pix_ht      = ccd_pixel_height * 1000.0;
    *guider_aperture = aperture;
    *guider_focal    = focal;
}

void cgmath::createGuideLog()
{
    logFile.close();
    logFile.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&logFile);

    out << "Guiding rate,x15 arcsec/sec: " << Options::guidingRate() << endl;
    out << "Focal,mm: " << focal << endl;
    out << "Aperture,mm: " << aperture << endl;
    out << "F/D: " << focal / aperture << endl;
    out << "Frame #, Time Elapsed (ms), RA Error (arcsec), RA Correction (ms), RA Correction Direction, DEC Error "
           "(arcsec), DEC Correction (ms), DEC Correction Direction"
        << endl;

    logTime.restart();
}

bool cgmath::setReticleParameters(double x, double y, double ang)
{
    // check frame ranges
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    if (x >= (double)video_width - 1)
        x = (double)video_width - 1;
    if (y >= (double)video_height - 1)
        y = (double)video_height - 1;

    reticle_pos = Vector(x, y, 0);

    if (ang >= 0)
        reticle_angle = ang;

    ROT_Z = Ekos::RotateZ(-M_PI * reticle_angle / 180.0); // NOTE!!! sing '-' derotates star coordinate system

    reticle_orts[0] = Vector(1, 0, 0) * 100;
    reticle_orts[1] = Vector(0, 1, 0) * 100;

    reticle_orts[0] = reticle_orts[0] * ROT_Z;
    reticle_orts[1] = reticle_orts[1] * ROT_Z;

    return true;
}

bool cgmath::getReticleParameters(double *x, double *y, double *ang) const
{
    *x = reticle_pos.x;
    *y = reticle_pos.y;

    if (ang)
        *ang = reticle_angle;

    return true;
}

int cgmath::getSquareAlgorithmIndex(void) const
{
    return square_alg_idx;
}

info_params_t cgmath::getInfoParameters(void) const
{
    info_params_t ret;
    Vector p;

    ret.aperture    = aperture;
    ret.focal       = focal;
    ret.focal_ratio = focal / aperture;
    p               = Vector(video_width, video_height, 0);
    p               = point2arcsec(p);
    p /= 60; // convert to minutes
    ret.fov_wd = p.x;
    ret.fov_ht = p.y;

    return ret;
}

uint32_t cgmath::getTicks(void) const
{
    return ticks;
}

void cgmath::getStarDrift(double *dx, double *dy) const
{
    *dx = star_pos.x;
    *dy = star_pos.y;
}

void cgmath::getStarScreenPosition(double *dx, double *dy) const
{
    *dx = scr_star_pos.x;
    *dy = scr_star_pos.y;
}

bool cgmath::reset(void)
{
//    square_alg_idx = AUTO_THRESHOLD;

//    // sky coord. system vars.
//    star_pos     = Vector(0);
//    scr_star_pos = Vector(0);

//    setReticleParameters(video_width / 2, video_height / 2, 0.0);

    ticks = 0;
    channel_ticks[GUIDE_RA] = channel_ticks[GUIDE_DEC] = 0;
    accum_ticks[GUIDE_RA] = accum_ticks[GUIDE_DEC] = 0;
    drift_integral[GUIDE_RA] = drift_integral[GUIDE_DEC] = 0;
    out_params.reset();

    memset(drift[GUIDE_RA], 0, sizeof(double) * MAX_ACCUM_CNT);
    memset(drift[GUIDE_DEC], 0, sizeof(double) * MAX_ACCUM_CNT);

    // cleanup stat vars.
    sum = 0;

    return true;
}

/*void cgmath::move_square( double newx, double newy )
{
    square_pos.x = newx;
    square_pos.y = newy;

    // check frame ranges
    if (lastBinX == subBinX)
    {
        if( square_pos.x < 0 )
            square_pos.x = 0;
        if( square_pos.y < 0 )
            square_pos.y = 0;
        if( square_pos.x+(double)square_size > (double)video_width )
            square_pos.x = (double)(video_width - square_size);
        if( square_pos.y+(double)square_size > (double)video_height )
            square_pos.y = (double)(video_height - square_size);
    }

    // FITS Image takes center coords
    if (guide_frame)
    {
        guide_frame->setTrackingBoxEnabled(true);
        //guide_frame->setTrackingBoxCenter(QPointF(square_pos.x+square_size/2, square_pos.y+square_size/2));
        guide_frame->setTrackingBox(QRect(square_pos.x, square_pos.y, square_size/subBinX, square_size/subBinY));
    }
}

void cgmath::resize_square( int size_idx )
{
    if( size_idx < 0 || size_idx >= (int)(sizeof(guide_squares)/sizeof(guide_square_t))-1)
        return;

    if (square_size != guide_squares[size_idx].size)
    {
      square_pos.x += (square_size-guide_squares[size_idx].size)/2;
      square_pos.y += (square_size-guide_squares[size_idx].size)/2;
    }

    square_size = guide_squares[size_idx].size;
    square_square = guide_squares[size_idx].square;
    square_idx = size_idx;

    // check position
    if (guide_frame)
    {
        guide_frame->setTrackingBoxEnabled(true);
        //guide_frame->setTrackingBoxSize(QSize(square_size,square_size));
        guide_frame->setTrackingBox(QRect(square_pos.x/subBinX, square_pos.y/subBinY, square_size/subBinX, square_size/subBinY));
    }

}*/

void cgmath::setSquareAlgorithm(int alg_idx)
{
    if (alg_idx < 0 || alg_idx >= (int)(sizeof(guide_square_alg) / sizeof(square_alg_t)) - 1)
        return;

    square_alg_idx = alg_idx;

    in_params.threshold_alg_idx = square_alg_idx;
}

Vector cgmath::point2arcsec(const Vector &p) const
{
    Vector arcs;

    // arcs = 3600*180/pi * (pix*ccd_pix_sz) / focal_len
    arcs.x = 206264.8062470963552 * p.x * ccd_pixel_width / focal;
    arcs.y = 206264.8062470963552 * p.y * ccd_pixel_height / focal;

    return arcs;
}

bool cgmath::calculateAndSetReticle1D(double start_x, double start_y, double end_x, double end_y, int RATotalPulse)
{
    double phi;

    phi = calculatePhi(start_x, start_y, end_x, end_y);

    if (phi < 0)
        return false;

    setReticleParameters(start_x, start_y, phi);

    if (RATotalPulse > 0)
    {
        double x   = end_x - start_x;
        double y   = end_y - start_y;
        double len = sqrt(x * x + y * y);

        // Total pulse includes start --> end --> start
        ditherRate[GUIDE_RA] = RATotalPulse / (2 * len);

        qCDebug(KSTARS_EKOS_GUIDE) << "Dither RA Rate " << ditherRate[GUIDE_RA] << " ms/Pixel";
    }

    return true;
}

bool cgmath::calculateAndSetReticle2D(double start_ra_x, double start_ra_y, double end_ra_x, double end_ra_y,
                                      double start_dec_x, double start_dec_y, double end_dec_x, double end_dec_y,
                                      bool *swap_dec, int RATotalPulse, int DETotalPulse)
{
    double phi_ra  = 0; // angle calculated by GUIDE_RA drift
    double phi_dec = 0; // angle calculated by GUIDE_DEC drift
    double phi     = 0;

    Vector ra_vect  = Normalize(Vector(end_ra_x - start_ra_x, -(end_ra_y - start_ra_y), 0));
    Vector dec_vect = Normalize(Vector(end_dec_x - start_dec_x, -(end_dec_y - start_dec_y), 0));

    Vector try_increase = dec_vect * Ekos::RotateZ(M_PI / 2);
    Vector try_decrease = dec_vect * Ekos::RotateZ(-M_PI / 2);

    double cos_increase = try_increase & ra_vect;
    double cos_decrease = try_decrease & ra_vect;

    bool do_increase = cos_increase > cos_decrease ? true : false;

    phi_ra = calculatePhi(start_ra_x, start_ra_y, end_ra_x, end_ra_y);
    if (phi_ra < 0)
        return false;

    phi_dec = calculatePhi(start_dec_x, start_dec_y, end_dec_x, end_dec_y);
    if (phi_dec < 0)
        return false;

    if (do_increase)
        phi_dec += 90;
    else
        phi_dec -= 90;

    if (phi_dec > 360)
        phi_dec -= 360.0;
    if (phi_dec < 0)
        phi_dec += 360.0;

    if (fabs(phi_dec - phi_ra) > 180)
    {
        if (phi_ra > phi_dec)
            phi_ra -= 360;
        else
            phi_dec -= 360;
    }

    // average angles
    phi = (phi_ra + phi_dec) / 2;
    if (phi < 0)
        phi += 360.0;

    // check DEC
    if (swap_dec)
        *swap_dec = dec_swap = do_increase ? false : true;

    setReticleParameters(start_ra_x, start_ra_y, phi);

    if (RATotalPulse > 0)
    {
        double x   = end_ra_x - start_ra_x;
        double y   = end_ra_y - start_ra_y;
        double len = sqrt(x * x + y * y);

        ditherRate[GUIDE_RA] = RATotalPulse / (2 * len);

        qCDebug(KSTARS_EKOS_GUIDE) << "Dither RA Rate " << ditherRate[GUIDE_RA] << " ms/Pixel";
    }

    if (DETotalPulse > 0)
    {
        double x   = end_dec_x - start_dec_x;
        double y   = end_dec_y - start_dec_y;
        double len = sqrt(x * x + y * y);

        ditherRate[GUIDE_DEC] = DETotalPulse / (2 * len);

        qCDebug(KSTARS_EKOS_GUIDE) << "Dither DEC Rate " << ditherRate[GUIDE_DEC] << " ms/Pixel";
    }

    return true;
}

double cgmath::calculatePhi(double start_x, double start_y, double end_x, double end_y) const
{
    double delta_x, delta_y;
    double phi;

    delta_x = end_x - start_x;
    delta_y = -(end_y - start_y);

    //if( (!Vector(delta_x, delta_y, 0)) < 2.5 )
    // JM 2015-12-10: Lower threshold to 1 pixel
    if ((!Vector(delta_x, delta_y, 0)) < 1)
        return -1;

    // 90 or 270 degrees
    if (fabs(delta_x) < fabs(delta_y) / 1000000.0)
    {
        phi = delta_y > 0 ? 90.0 : 270;
    }
    else
    {
        phi = 180.0 / M_PI * atan2(delta_y, delta_x);
        if (phi < 0)
            phi += 360.0;
    }

    return phi;
}

void cgmath::do_ticks(void)
{
    ticks++;

    channel_ticks[GUIDE_RA]++;
    channel_ticks[GUIDE_DEC]++;
    if (channel_ticks[GUIDE_RA] >= MAX_ACCUM_CNT)
        channel_ticks[GUIDE_RA] = 0;
    if (channel_ticks[GUIDE_DEC] >= MAX_ACCUM_CNT)
        channel_ticks[GUIDE_DEC] = 0;

    accum_ticks[GUIDE_RA]++;
    accum_ticks[GUIDE_DEC]++;
    if (accum_ticks[GUIDE_RA] >= in_params.accum_frame_cnt[GUIDE_RA])
        accum_ticks[GUIDE_RA] = 0;
    if (accum_ticks[GUIDE_DEC] >= in_params.accum_frame_cnt[GUIDE_DEC])
        accum_ticks[GUIDE_DEC] = 0;
}

//-------------------- Processing ---------------------------
void cgmath::start(void)
{
    ticks                   = 0;
    channel_ticks[GUIDE_RA] = channel_ticks[GUIDE_DEC] = 0;
    accum_ticks[GUIDE_RA] = accum_ticks[GUIDE_DEC] = 0;
    drift_integral[GUIDE_RA] = drift_integral[GUIDE_DEC] = 0;
    out_params.reset();

    memset(drift[GUIDE_RA], 0, sizeof(double) * MAX_ACCUM_CNT);
    memset(drift[GUIDE_DEC], 0, sizeof(double) * MAX_ACCUM_CNT);

    // cleanup stat vars.
    sum = 0;

    preview_mode = false;

    if (focal > 0 && aperture > 0)
        createGuideLog();

    // Create reference Image
    if (imageGuideEnabled)
    {
        foreach (float *region, referenceRegions)
            delete[] region;

        referenceRegions.clear();

        referenceRegions = partitionImage();

        reticle_pos = Vector(0, 0, 0);
    }
}

void cgmath::stop(void)
{
    preview_mode = true;
}

void cgmath::suspend(bool mode)
{
    suspended = mode;
}

bool cgmath::isSuspended(void) const
{
    return suspended;
}

bool cgmath::isStarLost(void) const
{
    return lost_star;
}

void cgmath::setLostStar(bool is_lost)
{
    lost_star = is_lost;
}

float *cgmath::createFloatImage(FITSData *target) const
{
    FITSData *imageData = target;
    if (imageData == nullptr)
        imageData = guideView->getImageData();

    // #1 Convert to float array
    // We only process 1st plane if it is a color image
    uint32_t imgSize = imageData->getSamplesPerChannel();
    float *imgFloat  = new float[imgSize];

    if (imgFloat == nullptr)
    {
        qCritical() << "Not enough memory for float image array!";
        return nullptr;
    }

    switch (imageData->getDataType())
    {
        case TBYTE:
        {
            uint8_t *buffer = imageData->getImageBuffer();
            for (uint32_t i = 0; i < imgSize; i++)
                imgFloat[i] = buffer[i];
        }
        break;

        case TSHORT:
        {
            int16_t *buffer = reinterpret_cast<int16_t *>(imageData->getImageBuffer());
            for (uint32_t i = 0; i < imgSize; i++)
                imgFloat[i] = buffer[i];
        }
        break;

        case TUSHORT:
        {
            uint16_t *buffer = reinterpret_cast<uint16_t *>(imageData->getImageBuffer());
            for (uint32_t i = 0; i < imgSize; i++)
                imgFloat[i] = buffer[i];
        }
        break;

        case TLONG:
        {
            int32_t *buffer = reinterpret_cast<int32_t *>(imageData->getImageBuffer());
            for (uint32_t i = 0; i < imgSize; i++)
                imgFloat[i] = buffer[i];
        }
        break;

        case TULONG:
        {
            uint32_t *buffer = reinterpret_cast<uint32_t *>(imageData->getImageBuffer());
            for (uint32_t i = 0; i < imgSize; i++)
                imgFloat[i] = buffer[i];
        }
        break;

        case TFLOAT:
        {
            float *buffer = reinterpret_cast<float *>(imageData->getImageBuffer());
            for (uint32_t i = 0; i < imgSize; i++)
                imgFloat[i] = buffer[i];
        }
        break;

        case TLONGLONG:
        {
            int64_t *buffer = reinterpret_cast<int64_t *>(imageData->getImageBuffer());
            for (uint32_t i = 0; i < imgSize; i++)
                imgFloat[i] = buffer[i];
        }
        break;

        case TDOUBLE:
        {
            double *buffer = reinterpret_cast<double *>(imageData->getImageBuffer());
            for (uint32_t i = 0; i < imgSize; i++)
                imgFloat[i] = buffer[i];
        }
        break;

        default:
            delete[] imgFloat;
            return nullptr;
    }

    return imgFloat;
}

QVector<float *> cgmath::partitionImage() const
{
    QVector<float *> regions;

    FITSData *imageData = guideView->getImageData();

    float *imgFloat = createFloatImage();

    if (imgFloat == nullptr)
        return regions;

    const uint32_t width  = imageData->getWidth();
    const uint32_t height = imageData->getHeight();

    uint8_t xRegions = floor(width / regionAxis);
    uint8_t yRegions = floor(height / regionAxis);
    // Find number of regions to divide the image
    //uint8_t regions =  xRegions * yRegions;

    float *regionPtr = imgFloat;

    for (uint8_t i = 0; i < yRegions; i++)
    {
        for (uint8_t j = 0; j < xRegions; j++)
        {
            // Allocate space for one region
            float *oneRegion = new float[regionAxis * regionAxis];
            // Create points to region and current location of the source image in the desired region
            float *oneRegionPtr = oneRegion, *imgFloatPtr = regionPtr + j * regionAxis;

            // copy from image to region line by line
            for (uint32_t line = 0; line < regionAxis; line++)
            {
                memcpy(oneRegionPtr, imgFloatPtr, regionAxis);
                oneRegionPtr += regionAxis;
                imgFloatPtr += width;
            }

            regions.append(oneRegion);
        }

        // Move regionPtr block by (width * regionAxis) elements
        regionPtr += width * regionAxis;
    }

    // We're done with imgFloat
    delete[] imgFloat;

    return regions;
}

void cgmath::setRegionAxis(const uint32_t &value)
{
    regionAxis = value;
}

Vector cgmath::findLocalStarPosition(void) const
{
    if (useRapidGuide)
    {
        return Vector(rapidDX, rapidDY, 0);
    }

    FITSData *imageData = guideView->getImageData();

    if (imageGuideEnabled)
    {
        float xshift = 0, yshift = 0;

        QVector<Vector> shifts;
        float xsum = 0, ysum = 0;

        QVector<float *> imageParition = partitionImage();

        if (imageParition.isEmpty())
        {
            qWarning() << "Failed to partiion regions in image!";
            return Vector(-1, -1, -1);
        }

        if (imageParition.count() != referenceRegions.count())
        {
            qWarning() << "Mismatch between reference regions #" << referenceRegions.count()
                       << "and image parition regions #" << imageParition.count();
            // Clear memory in case of mis-match
            foreach (float *region, imageParition)
            {
                delete[] region;
            }

            return Vector(-1, -1, -1);
        }

        for (uint8_t i = 0; i < imageParition.count(); i++)
        {
            ImageAutoGuiding::ImageAutoGuiding1(referenceRegions[i], imageParition[i], regionAxis, &xshift, &yshift);
            Vector shift(xshift, yshift, -1);
            qCDebug(KSTARS_EKOS_GUIDE) << "Region #" << i << ": X-Shift=" << xshift << "Y-Shift=" << yshift;

            xsum += xshift;
            ysum += yshift;
            shifts.append(shift);
        }

        // Delete partitions
        foreach (float *region, imageParition)
        {
            delete[] region;
        }
        imageParition.clear();

        float average_x = xsum / referenceRegions.count();
        float average_y = ysum / referenceRegions.count();

        float median_x = shifts[referenceRegions.count() / 2 - 1].x;
        float median_y = shifts[referenceRegions.count() / 2 - 1].y;

        qCDebug(KSTARS_EKOS_GUIDE) << "Average : X-Shift=" << average_x << "Y-Shift=" << average_y;
        qCDebug(KSTARS_EKOS_GUIDE) << "Median  : X-Shift=" << median_x << "Y-Shift=" << median_y;

        return Vector(median_x, median_y, -1);
    }

    switch (imageData->getDataType())
    {
        case TBYTE:
            return findLocalStarPosition<uint8_t>();
            break;

        case TSHORT:
            return findLocalStarPosition<int16_t>();
            break;

        case TUSHORT:
            return findLocalStarPosition<uint16_t>();
            break;

        case TLONG:
            return findLocalStarPosition<int32_t>();
            break;

        case TULONG:
            return findLocalStarPosition<uint32_t>();
            break;

        case TFLOAT:
            return findLocalStarPosition<float>();
            break;

        case TLONGLONG:
            return findLocalStarPosition<int64_t>();
            break;

        case TDOUBLE:
            return findLocalStarPosition<double>();
            break;

        default:
            break;
    }

    return Vector(-1, -1, -1);
}

template <typename T>
Vector cgmath::findLocalStarPosition(void) const
{
    static double P0 = 0.906, P1 = 0.584, P2 = 0.365, P3 = 0.117, P4 = 0.049, P5 = -0.05, P6 = -0.064, P7 = -0.074,
                  P8 = -0.094;

    Vector ret;
    int i, j;
    double resx, resy, mass, threshold, pval;
    T *psrc    = nullptr;
    T *porigin = nullptr;
    T *pptr;

    QRect trackingBox = guideView->getTrackingBox();

    if (trackingBox.isValid() == false)
        return Vector(-1, -1, -1);

    FITSData *imageData = guideView->getImageData();

    if (imageData == nullptr)
    {
        qCWarning(KSTARS_EKOS_GUIDE) << "Cannot process a nullptr image.";
        return Vector(-1, -1, -1);
    }

    if (square_alg_idx == SEP_THRESHOLD)
    {
        int count = imageData->findStars(ALGORITHM_SEP, trackingBox);
        if (count > 0)
        {
            imageData->getHFR(HFR_MAX);
            Edge *star = imageData->getMaxHFRStar();
            if (star)
                ret = Vector(star->x, star->y, 0);
            else
                ret = Vector(-1, -1, -1);
                //ret = Vector(star->x, star->y, 0) - Vector(trackingBox.x(), trackingBox.y(), 0);
        }
        else
            ret = Vector(-1, -1, -1);

        return ret;
    }

    T *pdata = reinterpret_cast<T *>(imageData->getImageBuffer());

    qCDebug(KSTARS_EKOS_GUIDE) << "Tracking Square " << trackingBox;

    double square_square = trackingBox.width() * trackingBox.width();

    psrc = porigin = pdata + trackingBox.y() * video_width + trackingBox.x();

    resx = resy = 0;
    threshold = mass = 0;

    // several threshold adaptive smart agorithms
    switch (square_alg_idx)
    {
        case CENTROID_THRESHOLD:
        {
            int width  = trackingBox.width();
            int height = trackingBox.width();
            float i0, i1, i2, i3, i4, i5, i6, i7, i8;
            int ix = 0, iy = 0;
            int xM4;
            T *p;
            double average, fit, bestFit = 0;
            int minx = 0;
            int maxx = width;
            int miny = 0;
            int maxy = height;
            for (int x = minx; x < maxx; x++)
                for (int y = miny; y < maxy; y++)
                {
                    i0 = i1 = i2 = i3 = i4 = i5 = i6 = i7 = i8 = 0;
                    xM4                                        = x - 4;
                    p                                          = psrc + (y - 4) * video_width + xM4;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    p = psrc + (y - 3) * video_width + xM4;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i7 += *p++;
                    i6 += *p++;
                    i7 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    p = psrc + (y - 2) * video_width + xM4;
                    i8 += *p++;
                    i8 += *p++;
                    i5 += *p++;
                    i4 += *p++;
                    i3 += *p++;
                    i4 += *p++;
                    i5 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    p = psrc + (y - 1) * video_width + xM4;
                    i8 += *p++;
                    i7 += *p++;
                    i4 += *p++;
                    i2 += *p++;
                    i1 += *p++;
                    i2 += *p++;
                    i4 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    p = psrc + (y + 0) * video_width + xM4;
                    i8 += *p++;
                    i6 += *p++;
                    i3 += *p++;
                    i1 += *p++;
                    i0 += *p++;
                    i1 += *p++;
                    i3 += *p++;
                    i6 += *p++;
                    i8 += *p++;
                    p = psrc + (y + 1) * video_width + xM4;
                    i8 += *p++;
                    i7 += *p++;
                    i4 += *p++;
                    i2 += *p++;
                    i1 += *p++;
                    i2 += *p++;
                    i4 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    p = psrc + (y + 2) * video_width + xM4;
                    i8 += *p++;
                    i8 += *p++;
                    i5 += *p++;
                    i4 += *p++;
                    i3 += *p++;
                    i4 += *p++;
                    i5 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    p = psrc + (y + 3) * video_width + xM4;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i7 += *p++;
                    i6 += *p++;
                    i7 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    p = psrc + (y + 4) * video_width + xM4;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    average = (i0 + i1 + i2 + i3 + i4 + i5 + i6 + i7 + i8) / 85.0;
                    fit     = P0 * (i0 - average) + P1 * (i1 - 4 * average) + P2 * (i2 - 4 * average) +
                          P3 * (i3 - 4 * average) + P4 * (i4 - 8 * average) + P5 * (i5 - 4 * average) +
                          P6 * (i6 - 4 * average) + P7 * (i7 - 8 * average) + P8 * (i8 - 48 * average);
                    if (bestFit < fit)
                    {
                        bestFit = fit;
                        ix      = x;
                        iy      = y;
                    }
                }

            if (bestFit > 50)
            {
                double sumX  = 0;
                double sumY  = 0;
                double total = 0;
                for (int y = iy - 4; y <= iy + 4; y++)
                {
                    p = psrc + y * width + ix - 4;
                    for (int x = ix - 4; x <= ix + 4; x++)
                    {
                        double w = *p++;
                        sumX += x * w;
                        sumY += y * w;
                        total += w;
                    }
                }
                if (total > 0)
                {
                    ret = (Vector(trackingBox.x(), trackingBox.y(), 0) + Vector(sumX / total, sumY / total, 0));
                    return ret;
                }
            }

            return Vector(-1, -1, -1);
        }
        break;
        // Alexander's Stepanenko smart threshold algorithm
        case SMART_THRESHOLD:
        {
            point_t bbox_lt = { trackingBox.x() - SMART_FRAME_WIDTH, trackingBox.y() - SMART_FRAME_WIDTH };
            point_t bbox_rb = { trackingBox.x() + trackingBox.width() + SMART_FRAME_WIDTH,
                                trackingBox.y() + trackingBox.width() + SMART_FRAME_WIDTH };
            int offset      = 0;

            // clip frame
            if (bbox_lt.x < 0)
                bbox_lt.x = 0;
            if (bbox_lt.y < 0)
                bbox_lt.y = 0;
            if (bbox_rb.x > video_width)
                bbox_rb.x = video_width;
            if (bbox_rb.y > video_height)
                bbox_rb.y = video_height;

            // calc top bar
            int box_wd  = bbox_rb.x - bbox_lt.x;
            int box_ht  = trackingBox.y() - bbox_lt.y;
            int pix_cnt = 0;
            if (box_wd > 0 && box_ht > 0)
            {
                pix_cnt += box_wd * box_ht;
                for (j = bbox_lt.y; j < trackingBox.y(); ++j)
                {
                    offset = j * video_width;
                    for (i = bbox_lt.x; i < bbox_rb.x; ++i)
                    {
                        pptr = pdata + offset + i;
                        threshold += *pptr;
                    }
                }
            }
            // calc left bar
            box_wd = trackingBox.x() - bbox_lt.x;
            box_ht = trackingBox.width();
            if (box_wd > 0 && box_ht > 0)
            {
                pix_cnt += box_wd * box_ht;
                for (j = trackingBox.y(); j < trackingBox.y() + box_ht; ++j)
                {
                    offset = j * video_width;
                    for (i = bbox_lt.x; i < trackingBox.x(); ++i)
                    {
                        pptr = pdata + offset + i;
                        threshold += *pptr;
                    }
                }
            }
            // calc right bar
            box_wd = bbox_rb.x - trackingBox.x() - trackingBox.width();
            box_ht = trackingBox.width();
            if (box_wd > 0 && box_ht > 0)
            {
                pix_cnt += box_wd * box_ht;
                for (j = trackingBox.y(); j < trackingBox.y() + box_ht; ++j)
                {
                    offset = j * video_width;
                    for (i = trackingBox.x() + trackingBox.width(); i < bbox_rb.x; ++i)
                    {
                        pptr = pdata + offset + i;
                        threshold += *pptr;
                    }
                }
            }
            // calc bottom bar
            box_wd = bbox_rb.x - bbox_lt.x;
            box_ht = bbox_rb.y - trackingBox.y() - trackingBox.width();
            if (box_wd > 0 && box_ht > 0)
            {
                pix_cnt += box_wd * box_ht;
                for (j = trackingBox.y() + trackingBox.width(); j < bbox_rb.y; ++j)
                {
                    offset = j * video_width;
                    for (i = bbox_lt.x; i < bbox_rb.x; ++i)
                    {
                        pptr = pdata + offset + i;
                        threshold += *pptr;
                    }
                }
            }
            // find maximum
            double max_val = 0;
            for (j = 0; j < trackingBox.width(); ++j)
            {
                for (i = 0; i < trackingBox.width(); ++i)
                {
                    pptr = psrc + i;
                    if (*pptr > max_val)
                        max_val = *pptr;
                }
                psrc += video_width;
            }
            if (pix_cnt != 0)
                threshold /= (double)pix_cnt;

            // cut by 10% higher then average threshold
            if (max_val > threshold)
                threshold += (max_val - threshold) * SMART_CUT_FACTOR;

            //log_i("smart thr. = %f cnt = %d", threshold, pix_cnt);
            break;
        }
        // simple adaptive threshold
        case AUTO_THRESHOLD:
        {
            for (j = 0; j < trackingBox.width(); ++j)
            {
                for (i = 0; i < trackingBox.width(); ++i)
                {
                    pptr = psrc + i;
                    threshold += *pptr;
                }
                psrc += video_width;
            }
            threshold /= square_square;
            break;
        }
        // no threshold subtracion
        default:
        {
        }
    }

    psrc = porigin;
    for (j = 0; j < trackingBox.width(); ++j)
    {
        for (i = 0; i < trackingBox.width(); ++i)
        {
            pptr = psrc + i;
            pval = *pptr - threshold;
            pval = pval < 0 ? 0 : pval;

            resx += (double)i * pval;
            resy += (double)j * pval;

            mass += pval;
        }
        psrc += video_width;
    }

    if (mass == 0)
        mass = 1;

    resx /= mass;
    resy /= mass;

    ret = Vector(trackingBox.x(), trackingBox.y(), 0) + Vector(resx, resy, 0);

    return ret;
}

void cgmath::process_axes(void)
{
    int cnt        = 0;
    double t_delta = 0;

    qCDebug(KSTARS_EKOS_GUIDE) << "Processing Axes";

    in_params.proportional_gain[0] = Options::rAProportionalGain();
    in_params.proportional_gain[1] = Options::dECProportionalGain();

    in_params.integral_gain[0] = Options::rAIntegralGain();
    in_params.integral_gain[1] = Options::rAIntegralGain();

    in_params.derivative_gain[0] = Options::rADerivativeGain();
    in_params.derivative_gain[1] = Options::dECDerivativeGain();

    in_params.enabled[0] = Options::rAGuideEnabled();
    in_params.enabled[1] = Options::dECGuideEnabled();

    in_params.min_pulse_length[0] = Options::rAMinimumPulse();
    in_params.min_pulse_length[1] = Options::dECMinimumPulse();

    in_params.max_pulse_length[0] = Options::rAMaximumPulse();
    in_params.max_pulse_length[1] = Options::dECMaximumPulse();

    // RA W/E enable
    // East RA+ enabled?
    in_params.enabled_axis1[0] = Options::eastRAGuideEnabled();
    // West RA- enabled?
    in_params.enabled_axis2[0] = Options::westRAGuideEnabled();

    // DEC N/S enable
    // North DEC+ enabled?
    in_params.enabled_axis1[1] = Options::northDECGuideEnabled();
    // South DEC- enabled?
    in_params.enabled_axis2[1] = Options::southDECGuideEnabled();

    // process axes...
    for (int k = GUIDE_RA; k <= GUIDE_DEC; k++)
    {
        // zero all out commands
        out_params.pulse_dir[k] = NO_DIR;

        if (accum_ticks[k] < in_params.accum_frame_cnt[k] - 1)
            continue;

        t_delta           = 0;
        drift_integral[k] = 0;

        cnt = in_params.accum_frame_cnt[k];

        for (int i = 0, idx = channel_ticks[k]; i < cnt; ++i)
        {
            t_delta += drift[k][idx];

             qCDebug(KSTARS_EKOS_GUIDE) << "At #" << idx << "drift[" << k << "][" << idx << "] = " << drift[k][idx] << " , t_delta: " << t_delta;

            if (idx > 0)
                --idx;
            else
                idx = MAX_ACCUM_CNT - 1;
        }

        for (int i = 0; i < MAX_ACCUM_CNT; ++i)
            drift_integral[k] += drift[k][i];

        out_params.delta[k] = t_delta / (double)cnt;
        drift_integral[k] /= (double)MAX_ACCUM_CNT;

        qCDebug(KSTARS_EKOS_GUIDE) << "delta         [" << k << "]= " << out_params.delta[k];
        qCDebug(KSTARS_EKOS_GUIDE) << "drift_integral[" << k << "]= " << drift_integral[k];

        out_params.pulse_length[k] =
            fabs(out_params.delta[k] * in_params.proportional_gain[k] + drift_integral[k] * in_params.integral_gain[k]);
        out_params.pulse_length[k] = out_params.pulse_length[k] <= in_params.max_pulse_length[k] ?
                                         out_params.pulse_length[k] :
                                         in_params.max_pulse_length[k];

        qCDebug(KSTARS_EKOS_GUIDE) << "pulse_length  [" << k << "]= " << out_params.pulse_length[k];

        // calc direction
        // We do not send pulse if direction is disabled completely, or if direction in a specific axis (e.g. N or S) is disabled
        if (!in_params.enabled[k] || (out_params.delta[k] > 0 && !in_params.enabled_axis1[k]) ||
            (out_params.delta[k] < 0 && !in_params.enabled_axis2[k]))
        {
            out_params.pulse_dir[k]    = NO_DIR;
            out_params.pulse_length[k] = 0;
            continue;
        }

        if (out_params.pulse_length[k] >= in_params.min_pulse_length[k])
        {
            if (k == GUIDE_RA)
                out_params.pulse_dir[k] =
                    out_params.delta[k] > 0 ? RA_DEC_DIR : RA_INC_DIR; // GUIDE_RA. right dir - decreases GUIDE_RA
            else
            {
                out_params.pulse_dir[k] = out_params.delta[k] > 0 ? DEC_INC_DIR : DEC_DEC_DIR; // GUIDE_DEC.

                // Reverse DEC direction if we are looking eastward
                //if (ROT_Z.x[0][0] > 0 || (ROT_Z.x[0][0] ==0 && ROT_Z.x[0][1] > 0))
                //out_params.pulse_dir[k] = (out_params.pulse_dir[k] == DEC_INC_DIR) ? DEC_DEC_DIR : DEC_INC_DIR;
            }
        }
        else
            out_params.pulse_dir[k] = NO_DIR;

        qCDebug(KSTARS_EKOS_GUIDE) << "Direction     : " << get_direction_string(out_params.pulse_dir[k]);
    }

    //emit newAxisDelta(out_params.delta[0], out_params.delta[1]);

    if (Options::guideLogging())
    {
        QTextStream out(&logFile);
        out << ticks << "," << logTime.elapsed() << "," << out_params.delta[0] << "," << out_params.pulse_length[0] << ","
            << get_direction_string(out_params.pulse_dir[0]) << "," << out_params.delta[1] << ","
            << out_params.pulse_length[1] << "," << get_direction_string(out_params.pulse_dir[1]) << endl;
    }
}

void cgmath::performProcessing(void)
{
    Vector arc_star_pos, arc_reticle_pos;

    // do nothing if suspended
    if (suspended)
        return;

    // find guiding star location in
    scr_star_pos = star_pos = findLocalStarPosition();

    if (star_pos.x == -1 || std::isnan(star_pos.x))
    {
        lost_star = true;
        return;
    }
    else
        lost_star = false;

    // move square overlay

    //TODO FIXME
    //moveSquare( round(star_pos.x) - (double)square_size/(2*subBinX), round(star_pos.y) - (double)square_size/(2*subBinY) );

    QVector3D starCenter(star_pos.x, star_pos.y, 0);
    emit newStarPosition(starCenter, true);

    if (preview_mode)
        return;

    qCDebug(KSTARS_EKOS_GUIDE) << "################## BEGIN PROCESSING ##################";

    // translate star coords into sky coord. system

    // convert from pixels into arcsecs
    arc_star_pos    = point2arcsec(star_pos);
    arc_reticle_pos = point2arcsec(reticle_pos);

    qCDebug(KSTARS_EKOS_GUIDE) << "Star    X : " << star_pos.x << " Y  : " << star_pos.y;
    qCDebug(KSTARS_EKOS_GUIDE) << "Reticle X : " << reticle_pos.x << " Y  :" << reticle_pos.y;
    qCDebug(KSTARS_EKOS_GUIDE) << "Star    RA: " << arc_star_pos.x << " DEC: " << arc_star_pos.y;
    qCDebug(KSTARS_EKOS_GUIDE) << "Reticle RA: " << arc_reticle_pos.x << " DEC: " << arc_reticle_pos.y;

    // translate into sky coords.
    star_pos   = arc_star_pos - arc_reticle_pos;
    star_pos.y = -star_pos.y; // invert y-axis as y picture axis is inverted

    qCDebug(KSTARS_EKOS_GUIDE) << "-------> BEFORE ROTATION Diff RA: " << star_pos.x << " DEC: " << star_pos.y;

    star_pos = star_pos * ROT_Z;

    // both coords are ready for math processing
    //put coord to drift list
    drift[GUIDE_RA][channel_ticks[GUIDE_RA]]   = star_pos.x;
    drift[GUIDE_DEC][channel_ticks[GUIDE_DEC]] = star_pos.y;

    qCDebug(KSTARS_EKOS_GUIDE) << "-------> AFTER ROTATION  Diff RA: " << star_pos.x << " DEC: " << star_pos.y;
    qCDebug(KSTARS_EKOS_GUIDE) << "RA channel ticks: " << channel_ticks[GUIDE_RA]
                 << " DEC channel ticks: " << channel_ticks[GUIDE_DEC];
    // make decision by axes
    process_axes();

    // process statistics
    calc_square_err();

    // finally process tickers
    do_ticks();

    qCDebug(KSTARS_EKOS_GUIDE) << "################## FINISH PROCESSING ##################";
}

void cgmath::calc_square_err(void)
{
    if (!do_statistics)
        return;
    // through MAX_ACCUM_CNT values
    if (ticks == 0)
        return;

    for (int k = GUIDE_RA; k <= GUIDE_DEC; k++)
    {
        double sqr_avg = 0;
        for (int i = 0; i < MAX_ACCUM_CNT; ++i)
            sqr_avg += drift[k][i] * drift[k][i];

        out_params.sigma[k] = sqrt(sqr_avg / (double)MAX_ACCUM_CNT);
    }
}

void cgmath::setRapidGuide(bool enable)
{
    useRapidGuide = enable;
}

double cgmath::getDitherRate(int axis)
{
    if (axis < 0 || axis > 1)
        return -1;

    return ditherRate[axis];
}

void cgmath::setRapidStarData(double dx, double dy)
{
    rapidDX = dx;
    rapidDY = dy;
}

const char *cgmath::get_direction_string(GuideDirection dir)
{
    switch (dir)
    {
        case RA_DEC_DIR:
            return "Decrease RA";
            break;

        case RA_INC_DIR:
            return "Increase RA";
            break;

        case DEC_DEC_DIR:
            return "Decrease DEC";
            break;

        case DEC_INC_DIR:
            return "Increase DEC";
            break;

        default:
            break;
    }

    return "NO DIR";
}

bool cgmath::isImageGuideEnabled() const
{
    return imageGuideEnabled;
}

void cgmath::setImageGuideEnabled(bool value)
{
    imageGuideEnabled = value;
}

static void psf_conv(float *dst, const float *src, int width, int height)
{
    //dst.Init(src.Size);

    //                       A      B1     B2    C1     C2    C3     D1     D2     D3
    const double PSF[] = { 0.906, 0.584, 0.365, .117, .049, -0.05, -.064, -.074, -.094 };

    //memset(dst.px, 0, src.NPixels * sizeof(float));

    /* PSF Grid is:
    D3 D3 D3 D3 D3 D3 D3 D3 D3
    D3 D3 D3 D2 D1 D2 D3 D3 D3
    D3 D3 C3 C2 C1 C2 C3 D3 D3
    D3 D2 C2 B2 B1 B2 C2 D2 D3
    D3 D1 C1 B1 A  B1 C1 D1 D3
    D3 D2 C2 B2 B1 B2 C2 D2 D3
    D3 D3 C3 C2 C1 C2 C3 D3 D3
    D3 D3 D3 D2 D1 D2 D3 D3 D3
    D3 D3 D3 D3 D3 D3 D3 D3 D3

    1@A
    4@B1, B2, C1, C3, D1
    8@C2, D2
    44 * D3
    */

    int psf_size = 4;

    for (int y = psf_size; y < height - psf_size; y++)
    {
        for (int x = psf_size; x < width - psf_size; x++)
        {
            float A, B1, B2, C1, C2, C3, D1, D2, D3;

#define PX(dx, dy) *(src + width * (y + (dy)) + x + (dx))
            A =  PX(+0, +0);
            B1 = PX(+0, -1) + PX(+0, +1) + PX(+1, +0) + PX(-1, +0);
            B2 = PX(-1, -1) + PX(+1, -1) + PX(-1, +1) + PX(+1, +1);
            C1 = PX(+0, -2) + PX(-2, +0) + PX(+2, +0) + PX(+0, +2);
            C2 = PX(-1, -2) + PX(+1, -2) + PX(-2, -1) + PX(+2, -1) + PX(-2, +1) + PX(+2, +1) + PX(-1, +2) + PX(+1, +2);
            C3 = PX(-2, -2) + PX(+2, -2) + PX(-2, +2) + PX(+2, +2);
            D1 = PX(+0, -3) + PX(-3, +0) + PX(+3, +0) + PX(+0, +3);
            D2 = PX(-1, -3) + PX(+1, -3) + PX(-3, -1) + PX(+3, -1) + PX(-3, +1) + PX(+3, +1) + PX(-1, +3) + PX(+1, +3);
            D3 = PX(-4, -2) + PX(-3, -2) + PX(+3, -2) + PX(+4, -2) + PX(-4, -1) + PX(+4, -1) + PX(-4, +0) + PX(+4, +0) + PX(-4, +1) + PX(+4, +1) + PX(-4, +2) + PX(-3, +2) + PX(+3, +2) + PX(+4, +2);
#undef PX
            int i;
            const float *uptr;

            uptr = src + width * (y - 4) + (x - 4);
            for (i = 0; i < 9; i++)
                D3 += *uptr++;

            uptr = src + width * (y - 3) + (x - 4);
            for (i = 0; i < 3; i++)
                D3 += *uptr++;
            uptr += 3;
            for (i = 0; i < 3; i++)
                D3 += *uptr++;

            uptr = src + width * (y + 3) + (x - 4);
            for (i = 0; i < 3; i++)
                D3 += *uptr++;
            uptr += 3;
            for (i = 0; i < 3; i++)
                D3 += *uptr++;

            uptr = src + width * (y + 4) + (x - 4);
            for (i = 0; i < 9; i++)
                D3 += *uptr++;

            double mean = (A + B1 + B2 + C1 + C2 + C3 + D1 + D2 + D3) / 81.0;
            double PSF_fit = PSF[0] * (A - mean) + PSF[1] * (B1 - 4.0 * mean) + PSF[2] * (B2 - 4.0 * mean) +
                PSF[3] * (C1 - 4.0 * mean) + PSF[4] * (C2 - 8.0 * mean) + PSF[5] * (C3 - 4.0 * mean) +
                PSF[6] * (D1 - 4.0 * mean) + PSF[7] * (D2 - 8.0 * mean) + PSF[8] * (D3 - 44.0 * mean);

            dst[width * y + x] = (float) PSF_fit;
        }
    }
}

static void GetStats(double *mean, double *stdev, int width, const float *img, const QRect& win)
{
    // Determine the mean and standard deviation
    double sum = 0.0;
    double a = 0.0;
    double q = 0.0;
    double k = 1.0;
    double km1 = 0.0;

    const float *p0 = img + win.top() * width + win.left();
    for (int y = 0; y < win.height(); y++)
    {
        const float *end = p0 + win.height();
        for (const float *p = p0; p < end; p++)
        {
            double const x = (double) *p;
            sum += x;
            double const a0 = a;
            a += (x - a) / k;
            q += (x - a0) * (x - a);
            km1 = k;
            k += 1.0;
        }
        p0 += width;
    }

    *mean = sum / km1;
    *stdev = sqrt(q / km1);
}

static void RemoveItems(std::set<Peak>& stars, const std::set<int>& to_erase)
{
    int n = 0;
    for (std::set<Peak>::iterator it = stars.begin(); it != stars.end(); n++)
    {
        if (to_erase.find(n) != to_erase.end())
        {
            std::set<Peak>::iterator next = it;
            ++next;
            stars.erase(it);
            it = next;
        }
        else
            ++it;
    }
}

// Based on PHD2 algorithm
QList<Edge*> cgmath::PSFAutoFind(int extraEdgeAllowance)
{
    //Debug.Write(wxString::Format("Star::AutoFind called with edgeAllowance = %d searchRegion = %d\n", extraEdgeAllowance, searchRegion));

    // run a 3x3 median first to eliminate hot pixels
    //usImage smoothed;
    //smoothed.CopyFrom(image);
    //Median3(smoothed);
    FITSData *smoothed = new FITSData(guideView->getImageData());
    smoothed->applyFilter(FITS_MEDIAN);

    int searchRegion = guideView->getTrackingBox().width();

    int subW = smoothed->getWidth();
    int subH = smoothed->getHeight();

    // convert to floating point
    float *conv = createFloatImage(smoothed);

    // run the PSF convolution
    {
        float *tmp = new float[smoothed->getSamplesPerChannel()];
        memset(tmp, 0, smoothed->getSamplesPerChannel()*sizeof(float));
        psf_conv(tmp, conv, subW, subH);
        delete [] conv;
        // Swap
        conv = tmp;
    }

    enum { CONV_RADIUS = 4 };
    int dw = subW;      // width of the downsampled image
    int dh = subH;     // height of the downsampled image
    QRect convRect(CONV_RADIUS, CONV_RADIUS, dw - 2 * CONV_RADIUS, dh - 2 * CONV_RADIUS);  // region containing valid data

    enum { TOP_N = 100 };  // keep track of the brightest stars
    std::set<Peak> stars;  // sorted by ascending intensity

    double global_mean, global_stdev;
    GetStats(&global_mean, &global_stdev, subW, conv, convRect);

    //Debug.Write(wxString::Format("AutoFind: global mean = %.1f, stdev %.1f\n", global_mean, global_stdev));

    const double threshold = 0.1;
    //Debug.Write(wxString::Format("AutoFind: using threshold = %.1f\n", threshold));

    // find each local maximum
    int srch = 4;
    for (int y = convRect.top() + srch; y <= convRect.bottom() - srch; y++)
    {
        for (int x = convRect.left() + srch; x <= convRect.right() - srch; x++)
        {
            float val = conv[dw * y + x];
            bool ismax = false;
            if (val > 0.0)
            {
                ismax = true;
                for (int j = -srch; j <= srch; j++)
                {
                    for (int i = -srch; i <= srch; i++)
                    {
                        if (i == 0 && j == 0)
                            continue;
                        if (conv[dw * (y + j) + (x + i)] > val)
                        {
                            ismax = false;
                            break;
                        }
                    }
                }
            }
            if (!ismax)
                continue;

            // compare local maximum to mean value of surrounding pixels
            const int local = 7;
            double local_mean, local_stdev;
            QRect localRect(x - local, y - local, 2 * local + 1, 2 * local + 1);
            localRect = localRect.intersected(convRect);
            GetStats(&local_mean, &local_stdev, subW, conv, localRect);

            // this is our measure of star intensity
            double h = (val - local_mean) / global_stdev;

            if (h < threshold)
            {
                //  Debug.Write(wxString::Format("AG: local max REJECT [%d, %d] PSF %.1f SNR %.1f\n", imgx, imgy, val, SNR));
                continue;
            }

            // coordinates on the original image
            int downsample =1;
            int imgx = x * downsample + downsample / 2;
            int imgy = y * downsample + downsample / 2;

            stars.insert(Peak(imgx, imgy, h));
            if (stars.size() > TOP_N)
                stars.erase(stars.begin());
        }
    }

    //for (std::set<Peak>::const_reverse_iterator it = stars.rbegin(); it != stars.rend(); ++it)
        //qCDebug(KSTARS_EKOS_GUIDE) << "AutoFind: local max [" << it->x << "," << it->y << "]" << it->val;

    // merge stars that are very close into a single star
    {
        const int minlimitsq = 5 * 5;
    repeat:
        for (std::set<Peak>::const_iterator a = stars.begin(); a != stars.end(); ++a)
        {
            std::set<Peak>::const_iterator b = a;
            ++b;
            for (; b != stars.end(); ++b)
            {
                int dx = a->x - b->x;
                int dy = a->y - b->y;
                int d2 = dx * dx + dy * dy;
                if (d2 < minlimitsq)
                {
                    // very close, treat as single star
                    //Debug.Write(wxString::Format("AutoFind: merge [%d, %d] %.1f - [%d, %d] %.1f\n", a->x, a->y, a->val, b->x, b->y, b->val));
                    // erase the dimmer one
                    stars.erase(a);
                    goto repeat;
                }
            }
        }
    }

    // exclude stars that would fit within a single searchRegion box
    {
        // build a list of stars to be excluded
        std::set<int> to_erase;
        const int extra = 5; // extra safety margin
        const int fullw = searchRegion + extra;
        for (std::set<Peak>::const_iterator a = stars.begin(); a != stars.end(); ++a)
        {
            std::set<Peak>::const_iterator b = a;
            ++b;
            for (; b != stars.end(); ++b)
            {
                int dx = abs(a->x - b->x);
                int dy = abs(a->y - b->y);
                if (dx <= fullw && dy <= fullw)
                {
                    // stars closer than search region, exclude them both
                    // but do not let a very dim star eliminate a very bright star
                    if (b->val / a->val >= 5.0)
                    {
                        //Debug.Write(wxString::Format("AutoFind: close dim-bright [%d, %d] %.1f - [%d, %d] %.1f\n", a->x, a->y, a->val, b->x, b->y, b->val));
                    }
                    else
                    {
                        //Debug.Write(wxString::Format("AutoFind: too close [%d, %d] %.1f - [%d, %d] %.1f\n", a->x, a->y, a->val, b->x, b->y, b->val));
                        to_erase.insert(std::distance(stars.begin(), a));
                        to_erase.insert(std::distance(stars.begin(), b));
                    }
                }
            }
        }
        RemoveItems(stars, to_erase);
    }

    // exclude stars too close to the edge
    {
        enum { MIN_EDGE_DIST = 40 };
        int edgeDist = MIN_EDGE_DIST;//pConfig->Profile.GetInt("/StarAutoFind/MinEdgeDist", MIN_EDGE_DIST);
        if (edgeDist < searchRegion)
            edgeDist = searchRegion;
        edgeDist += extraEdgeAllowance;

        std::set<Peak>::iterator it = stars.begin();
        while (it != stars.end())
        {
            std::set<Peak>::iterator next = it;
            ++next;
            if (it->x <= edgeDist || it->x >= subW - edgeDist ||
                it->y <= edgeDist || it->y >= subH - edgeDist)
            {
                //Debug.Write(wxString::Format("AutoFind: too close to edge [%d, %d] %.1f\n", it->x, it->y, it->val));
                stars.erase(it);
            }
            it = next;
        }
    }

    QList<Edge*> centers;
    for (std::set<Peak>::reverse_iterator it = stars.rbegin(); it != stars.rend(); ++it)
    {
        Edge *center = new Edge;
        center->x = it->x;
        center->y = it->y;
        center->val = it->val;
        centers.append(center);
    }

    delete [] conv;
    delete (smoothed);

    return centers;
}


//---------------------------------------------------------------------------------------
cproc_in_params::cproc_in_params()
{
    reset();
}

void cproc_in_params::reset(void)
{
    threshold_alg_idx = CENTROID_THRESHOLD;
    average           = true;

    for (int k = GUIDE_RA; k <= GUIDE_DEC; k++)
    {
        enabled[k]          = true;
        accum_frame_cnt[k]  = 1;
        integral_gain[k]    = 0;
        derivative_gain[k]  = 0;
        max_pulse_length[k] = 5000;
        min_pulse_length[k] = 100;
    }
}

cproc_out_params::cproc_out_params()
{
    reset();
}

void cproc_out_params::reset(void)
{
    for (int k = GUIDE_RA; k <= GUIDE_DEC; k++)
    {
        delta[k]        = 0;
        pulse_dir[k]    = NO_DIR;
        pulse_length[k] = 0;
        sigma[k]        = 0;
    }
}

