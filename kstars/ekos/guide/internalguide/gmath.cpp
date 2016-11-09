/*  Ekos guide tool
    Copyright (C) 2012 Andrew Stepanenko

    Modified by Jasem Mutlaq <mutlaqja@ikarustech.com> for KStars.

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "Options.h"

#include "gmath.h"

#include <math.h>
#include <string.h>

#include "vect.h"
#include "matr.h"

#include "fitsviewer/fitsview.h"

#define DEF_SQR_0	(8-0)
#define DEF_SQR_1	(16-0)
#define DEF_SQR_2	(32-0)
#define DEF_SQR_3	(64-0)
#define DEF_SQR_4	(128-0)

const guide_square_t guide_squares[] = { 	{DEF_SQR_0, DEF_SQR_0*DEF_SQR_0*1.0},
                                            {DEF_SQR_1, DEF_SQR_1*DEF_SQR_1*1.0},
                                            {DEF_SQR_2, DEF_SQR_2*DEF_SQR_2*1.0},
                                            {DEF_SQR_3, DEF_SQR_3*DEF_SQR_3*1.0},
                                            {DEF_SQR_4, DEF_SQR_4*DEF_SQR_4*1.0},
                                            {-1, -1}
                                       };


const square_alg_t guide_square_alg[] = {
    { SMART_THRESHOLD, "Smart" },
    { CENTROID_THRESHOLD, "Fast"},
    { AUTO_THRESHOLD, "Auto" },
    { NO_THRESHOLD, "No thresh." },
    { -1, {0} }
};

// JM: Why not use QPoint?
typedef struct
{
    int x, y;
}point_t;

cgmath::cgmath() : QObject()
{
    // sys...
    ticks = 0;    
    video_width  = -1;
    video_height = -1;
    ccd_pixel_width  = 0;
    ccd_pixel_height = 0;
    focal = 0;
    aperture = 0;
    ROT_Z = Matrix(0);
    preview_mode = true;
    suspended	 = false;
    lost_star    = false;
    useRapidGuide = false;
    dec_swap = false;

    subBinX = subBinY = 1;

    // square variables
    square_alg_idx	= SMART_THRESHOLD;

    // sky coord. system vars.
    star_pos 	 	= Vector(0);
    scr_star_pos	= Vector(0);
    reticle_pos 	= Vector(0);
    reticle_orts[0] = Vector(0);
    reticle_orts[1] = Vector(0);
    reticle_angle	= 0;

    ditherRate[0] = ditherRate[1] = -1;

    // processing
    in_params.reset();
    out_params.reset();
    channel_ticks[GUIDE_RA] = channel_ticks[GUIDE_DEC] = 0;
    accum_ticks[GUIDE_RA] = accum_ticks[GUIDE_DEC] = 0;
    drift[GUIDE_RA]  = new double[MAX_ACCUM_CNT];
    drift[GUIDE_DEC] = new double[MAX_ACCUM_CNT];
    memset( drift[GUIDE_RA], 0, sizeof(double)*MAX_ACCUM_CNT );
    memset( drift[GUIDE_DEC], 0, sizeof(double)*MAX_ACCUM_CNT );
    drift_integral[GUIDE_RA] = drift_integral[GUIDE_DEC] = 0;


    // statistics
    do_statistics = true;
    sum = sqr_sum = 0;
    delta_prev = sigma_prev = sigma = 0;
}

cgmath::~cgmath()
{
    delete [] drift[GUIDE_RA];
    delete [] drift[GUIDE_DEC];


}


bool cgmath::setVideoParameters(int vid_wd, int vid_ht , int binX, int binY)
{
    if( vid_wd <= 0 || vid_ht <= 0 )
        return false;

    video_width  = vid_wd/binX;
    video_height = vid_ht/binY;

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

bool cgmath::setGuiderParameters( double ccd_pix_wd, double ccd_pix_ht, double guider_aperture, double guider_focal )
{
    if( ccd_pix_wd < 0 )
        ccd_pix_wd = 0;
    if( ccd_pix_ht < 0 )
        ccd_pix_ht = 0;
    if( guider_focal <= 0 )
        guider_focal = 1;

    ccd_pixel_width		= ccd_pix_wd / 1000.0; // from mkm to mm
    ccd_pixel_height	= ccd_pix_ht / 1000.0; // from mkm to mm
    aperture			= guider_aperture;
    focal 				= guider_focal;

    return true;
}

void cgmath::getGuiderParameters( double *ccd_pix_wd, double *ccd_pix_ht, double *guider_aperture, double *guider_focal )
{
    *ccd_pix_wd = ccd_pixel_width * 1000.0;
    *ccd_pix_ht = ccd_pixel_height * 1000.0;
    *guider_aperture = aperture;
    *guider_focal = focal;
}


bool cgmath::setReticleParameters( double x, double y, double ang )
{
    // check frame ranges
    if( x < 0 )
        x = 0;
    if( y < 0 )
        y = 0;
    if( x >= (double)video_width-1 )
        x = (double)video_width-1;
    if( y >= (double)video_height-1 )
        y = (double)video_height-1;

    reticle_pos = Vector( x, y, 0 );

    if( ang >= 0)
        reticle_angle = ang;

    ROT_Z = RotateZ( -M_PI*reticle_angle/180.0 ); // NOTE!!! sing '-' derotates star coordinate system

    reticle_orts[0] = Vector(1, 0, 0) * 100;
    reticle_orts[1] = Vector(0, 1, 0) * 100;

    reticle_orts[0] = reticle_orts[0] * ROT_Z;
    reticle_orts[1] = reticle_orts[1] * ROT_Z;

    return true;
}


bool cgmath::getReticleParameters( double *x, double *y, double *ang ) const
{
    *x = reticle_pos.x;
    *y = reticle_pos.y;

    if (ang)
        *ang = reticle_angle;

    return true;
}


int  cgmath::getSquareAlgorithmIndex( void ) const
{
    return square_alg_idx;
}

info_params_t cgmath::getInfoParameters( void ) const
{
    info_params_t ret;
    Vector p;

    ret.aperture	= aperture;
    ret.focal		= focal;
    ret.focal_ratio	= focal / aperture;
    p = Vector(video_width, video_height, 0);
    p = point2arcsec( p );
    p /= 60;	// convert to minutes
    ret.fov_wd	= p.x;
    ret.fov_ht	= p.y;

    return ret;
}

uint32_t cgmath::getTicks( void ) const
{
    return ticks;
}

void cgmath::getStarDrift( double *dx, double *dy ) const
{
    *dx = star_pos.x;
    *dy = star_pos.y;
}

void cgmath::getStarScreenPosition( double *dx, double *dy ) const
{
    *dx = scr_star_pos.x;
    *dy = scr_star_pos.y;
}


bool cgmath::reset( void )
{
    square_alg_idx	= AUTO_THRESHOLD;

    // sky coord. system vars.
    star_pos 	 	= Vector(0);
    scr_star_pos	= Vector(0);

    setReticleParameters( video_width/2, video_height/2, 0.0 );

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


void cgmath::setSquareAlgorithm( int alg_idx )
{
    if( alg_idx < 0 || alg_idx >= (int)(sizeof(guide_square_alg)/sizeof(square_alg_t))-1 )
        return;

    square_alg_idx = alg_idx;

    in_params.threshold_alg_idx = square_alg_idx;
}


Vector cgmath::point2arcsec( const Vector &p ) const
{
    Vector arcs;

    // arcs = 3600*180/pi * (pix*ccd_pix_sz) / focal_len
    arcs.x = 206264.8062470963552 * p.x * ccd_pixel_width / focal;
    arcs.y = 206264.8062470963552 * p.y * ccd_pixel_height / focal;

    return arcs;
}

bool cgmath::calculateAndSetReticle1D( double start_x, double start_y, double end_x, double end_y, int totalPulse )
{
    double phi;


    phi = calculatePhi( start_x, start_y, end_x, end_y );

    if( phi < 0 )
        return false;

    setReticleParameters( start_x, start_y, phi );

    if (totalPulse > 0)
    {
        double x = end_x-start_x;
        double y = end_y - start_y;
        double len = sqrt(x*x + y*y);

        ditherRate[GUIDE_RA] = totalPulse / len;

        if (Options::guideLogging())
            qDebug() << "Guide: Dither RA Rate " << ditherRate[GUIDE_RA] << " ms/Pixel";

    }

    return true;
}


bool cgmath::calculateAndSetReticle2D( double start_ra_x, double start_ra_y, double end_ra_x, double end_ra_y, double start_dec_x, double start_dec_y, double end_dec_x, double end_dec_y, bool *swap_dec, int totalPulse)
{
    double phi_ra = 0;	 // angle calculated by GUIDE_RA drift
    double phi_dec = 0; // angle calculated by GUIDE_DEC drift
    double phi = 0;

    Vector ra_vect  = Normalize( Vector(end_ra_x - start_ra_x, -(end_ra_y - start_ra_y), 0) );
    Vector dec_vect = Normalize( Vector(end_dec_x - start_dec_x, -(end_dec_y - start_dec_y), 0) );

    Vector try_increase = dec_vect * RotateZ( M_PI/2 );
    Vector try_decrease = dec_vect * RotateZ( -M_PI/2 );

    double cos_increase = try_increase & ra_vect;
    double cos_decrease = try_decrease & ra_vect;

    bool do_increase = cos_increase > cos_decrease ? true : false;

    phi_ra = calculatePhi( start_ra_x, start_ra_y, end_ra_x, end_ra_y );
    if( phi_ra < 0 )
        return false;

    phi_dec = calculatePhi( start_dec_x, start_dec_y, end_dec_x, end_dec_y );
    if( phi_dec < 0 )
        return false;

    if( do_increase )
        phi_dec += 90;
    else
        phi_dec -= 90;

    if( phi_dec > 360 )phi_dec -= 360.0;
    if( phi_dec < 0 )phi_dec += 360.0;

    if( fabs(phi_dec - phi_ra) > 180 )
    {
        if( phi_ra > phi_dec )
            phi_ra -= 360;
        else
            phi_dec -= 360;
    }

    // average angles
    phi = (phi_ra + phi_dec) / 2;
    if( phi < 0 )phi += 360.0;

    // check DEC
    if( swap_dec )
        *swap_dec = dec_swap = do_increase ? false : true;

    setReticleParameters( start_ra_x, start_ra_y, phi );

    if (totalPulse > 0)
    {
        double x = end_ra_x-start_ra_x;
        double y = end_ra_y - start_ra_y;
        double len = sqrt(x*x + y*y);

        ditherRate[GUIDE_RA] = totalPulse / len;

        if (Options::guideLogging())
            qDebug() << "Guide: Dither RA Rate " << ditherRate[GUIDE_RA] << " ms/Pixel";

        x = end_dec_x-start_dec_x;
        y = end_dec_y - start_dec_y;
        len = sqrt(x*x + y*y);

        ditherRate[GUIDE_DEC] = totalPulse / len;

        if (Options::guideLogging())
            qDebug() << "Guide: Dither DEC Rate " << ditherRate[GUIDE_DEC] << " ms/Pixel";

    }

    return true;
}


double cgmath::calculatePhi( double start_x, double start_y, double end_x, double end_y ) const
{
    double delta_x, delta_y;
    double phi;

    delta_x = end_x - start_x;
    delta_y = -(end_y - start_y);

    //if( (!Vector(delta_x, delta_y, 0)) < 2.5 )
    // JM 2015-12-10: Lower threshold to 1 pixel
    if( (!Vector(delta_x, delta_y, 0)) < 1 )
        return -1;

    // 90 or 270 degrees
    if( fabs(delta_x) < fabs(delta_y) / 1000000.0 )
    {
        phi = delta_y > 0 ? 90.0 : 270;
    }
    else
    {
        phi = 180.0/M_PI*atan2( delta_y, delta_x );
        if( phi < 0 )phi += 360.0;
    }

    return phi;
}


void cgmath::do_ticks( void )
{
    ticks++;

    channel_ticks[GUIDE_RA]++;
    channel_ticks[GUIDE_DEC]++;
    if( channel_ticks[GUIDE_RA] >= MAX_ACCUM_CNT )
        channel_ticks[GUIDE_RA] = 0;
    if( channel_ticks[GUIDE_DEC] >= MAX_ACCUM_CNT )
        channel_ticks[GUIDE_DEC] = 0;

    accum_ticks[GUIDE_RA]++;
    accum_ticks[GUIDE_DEC]++;
    if( accum_ticks[GUIDE_RA] >= in_params.accum_frame_cnt[GUIDE_RA] )
        accum_ticks[GUIDE_RA] = 0;
    if( accum_ticks[GUIDE_DEC] >= in_params.accum_frame_cnt[GUIDE_DEC] )
        accum_ticks[GUIDE_DEC] = 0;
}


//-------------------- Processing ---------------------------
void cgmath::start( void )
{
    ticks = 0;
    channel_ticks[GUIDE_RA] = channel_ticks[GUIDE_DEC] = 0;
    accum_ticks[GUIDE_RA] = accum_ticks[GUIDE_DEC] = 0;
    drift_integral[GUIDE_RA] = drift_integral[GUIDE_DEC] = 0;
    out_params.reset();

    memset( drift[GUIDE_RA], 0, sizeof(double)*MAX_ACCUM_CNT );
    memset( drift[GUIDE_DEC], 0, sizeof(double)*MAX_ACCUM_CNT );

    // cleanup stat vars.
    sum = sqr_sum = 0;
    delta_prev = sigma_prev = sigma = 0;

    preview_mode = false;
}


void cgmath::stop( void )
{
    preview_mode = true;
}


void cgmath::suspend( bool mode )
{
    suspended = mode;
}


bool cgmath::isSuspended( void ) const
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

Vector cgmath::findLocalStarPosition( void ) const
{
    if (useRapidGuide)
    {
        return Vector(rapidDX , rapidDY, 0);
    }

    FITSData *imageData = guideView->getImageData();

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

    return Vector(-1,-1,-1);
}

template<typename T> Vector cgmath::findLocalStarPosition( void ) const
{
    static double P0 = 0.906, P1 = 0.584, P2 = 0.365, P3 = 0.117, P4 = 0.049, P5 = -0.05, P6 = -0.064, P7 = -0.074, P8 = -0.094;

    Vector ret;
    int i, j;
    double resx, resy, mass, threshold, pval;
    T *psrc = NULL, *porigin = NULL;
    T *pptr;

    QRect trackingBox = guideView->getTrackingBox();

    if (trackingBox.isValid() == false)
        return Vector(-1,-1,-1);

    FITSData *imageData = guideView->getImageData();

    if (imageData == NULL)
    {
        if (Options::guideLogging())
            qDebug() << "Guide: Cannot process a NULL image.";
        return Vector(-1,-1,-1);
    }

    T *pdata = reinterpret_cast<T*>(imageData->getImageBuffer());

    if (Options::guideLogging())
        qDebug() << "Guide: Tracking Square " << trackingBox;

    double square_square = trackingBox.width()*trackingBox.width();

    psrc = porigin = pdata + trackingBox.y()* video_width + trackingBox.x();

    resx = resy = 0;
    threshold = mass = 0;

    // several threshold adaptive smart agorithms
    switch( square_alg_idx )
    {
    case CENTROID_THRESHOLD:
    {
        int width = trackingBox.width();
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
                xM4 = x - 4;
                p = psrc + (y - 4) * video_width + xM4; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++;
                p = psrc + (y - 3) * video_width + xM4; i8 += *p++; i8 += *p++; i8 += *p++; i7 += *p++; i6 += *p++; i7 += *p++; i8 += *p++; i8 += *p++; i8 += *p++;
                p = psrc + (y - 2) * video_width + xM4; i8 += *p++; i8 += *p++; i5 += *p++; i4 += *p++; i3 += *p++; i4 += *p++; i5 += *p++; i8 += *p++; i8 += *p++;
                p = psrc + (y - 1) * video_width + xM4; i8 += *p++; i7 += *p++; i4 += *p++; i2 += *p++; i1 += *p++; i2 += *p++; i4 += *p++; i8 += *p++; i8 += *p++;
                p = psrc + (y + 0) * video_width + xM4; i8 += *p++; i6 += *p++; i3 += *p++; i1 += *p++; i0 += *p++; i1 += *p++; i3 += *p++; i6 += *p++; i8 += *p++;
                p = psrc + (y + 1) * video_width + xM4; i8 += *p++; i7 += *p++; i4 += *p++; i2 += *p++; i1 += *p++; i2 += *p++; i4 += *p++; i8 += *p++; i8 += *p++;
                p = psrc + (y + 2) * video_width + xM4; i8 += *p++; i8 += *p++; i5 += *p++; i4 += *p++; i3 += *p++; i4 += *p++; i5 += *p++; i8 += *p++; i8 += *p++;
                p = psrc + (y + 3) * video_width + xM4; i8 += *p++; i8 += *p++; i8 += *p++; i7 += *p++; i6 += *p++; i7 += *p++; i8 += *p++; i8 += *p++; i8 += *p++;
                p = psrc + (y + 4) * video_width + xM4; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++;
                average = (i0 + i1 + i2 + i3 + i4 + i5 + i6 + i7 + i8) / 85.0;
                fit = P0 * (i0 - average) + P1 * (i1 - 4 * average) + P2 * (i2 - 4 * average) + P3 * (i3 - 4 * average) + P4 * (i4 - 8 * average) + P5 * (i5 - 4 * average) + P6 * (i6 - 4 * average) + P7 * (i7 - 8 * average) + P8 * (i8 - 48 * average);
                if (bestFit < fit)
                {
                    bestFit = fit;
                    ix = x;
                    iy = y;
                }
            }

        if (bestFit > 50)
        {
            double sumX = 0;
            double sumY = 0;
            double total = 0;
            for (int y = iy - 4; y <= iy + 4; y++) {
                p = psrc + y * width + ix - 4;
                for (int x = ix - 4; x <= ix + 4; x++) {
                    double w = *p++;
                    sumX += x * w;
                    sumY += y * w;
                    total += w;
                }
            }
            if (total > 0)
            {
                ret = (Vector(trackingBox.x(), trackingBox.y(), 0) + Vector(sumX/total , sumY/total, 0));
                return ret;
            }
        }

       return Vector(-1,-1,-1);
    }
        break;
        // Alexander's Stepanenko smart threshold algorithm
    case SMART_THRESHOLD:
    {
        point_t bbox_lt = { trackingBox.x()-SMART_FRAME_WIDTH, trackingBox.y()-SMART_FRAME_WIDTH };
        point_t bbox_rb = { trackingBox.x()+trackingBox.width()+SMART_FRAME_WIDTH, trackingBox.y()+trackingBox.width()+SMART_FRAME_WIDTH };
        int offset = 0;

        // clip frame
        if( bbox_lt.x < 0 )
            bbox_lt.x = 0;
        if( bbox_lt.y < 0 )
            bbox_lt.y = 0;
        if( bbox_rb.x > video_width )
            bbox_rb.x = video_width;
        if( bbox_rb.y > video_height )
            bbox_rb.y = video_height;

        // calc top bar
        int box_wd = bbox_rb.x - bbox_lt.x;
        int box_ht = trackingBox.y() - bbox_lt.y;
        int pix_cnt = 0;
        if( box_wd > 0 && box_ht > 0 )
        {
            pix_cnt += box_wd * box_ht;
            for( j = bbox_lt.y;j < trackingBox.y();++j )
            {
                offset = j*video_width;
                for( i = bbox_lt.x;i < bbox_rb.x;++i )
                {
                    pptr = pdata + offset + i;
                    threshold += *pptr;
                }
            }
        }
        // calc left bar
        box_wd = trackingBox.x() - bbox_lt.x;
        box_ht = trackingBox.width();
        if( box_wd > 0 && box_ht > 0 )
        {
            pix_cnt += box_wd * box_ht;
            for( j = trackingBox.y();j < trackingBox.y()+box_ht;++j )
            {
                offset = j*video_width;
                for( i = bbox_lt.x;i < trackingBox.x();++i )
                {
                    pptr = pdata + offset + i;
                    threshold += *pptr;
                }
            }
        }
        // calc right bar
        box_wd = bbox_rb.x - trackingBox.x() - trackingBox.width();
        box_ht = trackingBox.width();
        if( box_wd > 0 && box_ht > 0 )
        {
            pix_cnt += box_wd * box_ht;
            for( j = trackingBox.y();j < trackingBox.y()+box_ht;++j )
            {
                offset = j*video_width;
                for( i = trackingBox.x()+trackingBox.width();i < bbox_rb.x;++i )
                {
                    pptr = pdata + offset + i;
                    threshold += *pptr;
                }
            }
        }
        // calc bottom bar
        box_wd = bbox_rb.x - bbox_lt.x;
        box_ht = bbox_rb.y - trackingBox.y() - trackingBox.width();
        if( box_wd > 0 && box_ht > 0 )
        {
            pix_cnt += box_wd * box_ht;
            for( j = trackingBox.y()+trackingBox.width();j < bbox_rb.y;++j )
            {
                offset = j*video_width;
                for( i = bbox_lt.x;i < bbox_rb.x;++i )
                {
                    pptr = pdata + offset + i;
                    threshold += *pptr;
                }
            }
        }
        // find maximum
        double max_val = 0;
        for( j = 0;j < trackingBox.width();++j )
        {
            for( i = 0;i < trackingBox.width();++i )
            {
                pptr = psrc+i;
                if( *pptr > max_val )
                    max_val = *pptr;
            }
            psrc += video_width;
        }
        threshold /= (double)pix_cnt;
        // cut by 10% higher then average threshold
        if( max_val > threshold )
            threshold += (max_val - threshold) * SMART_CUT_FACTOR;

        //log_i("smart thr. = %f cnt = %d", threshold, pix_cnt);
        break;
    }
        // simple adaptive threshold
    case AUTO_THRESHOLD:
    {
        for( j = 0;j < trackingBox.width();++j )
        {
            for( i = 0;i < trackingBox.width();++i )
            {
                pptr = psrc+i;
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
    for( j = 0;j < trackingBox.width();++j )
    {
        for( i = 0;i < trackingBox.width();++i )
        {
            pptr = psrc+i;
            pval = *pptr - threshold;
            pval = pval < 0 ? 0 : pval;

            resx += (double)i * pval;
            resy += (double)j * pval;

            mass += pval;
        }
        psrc += video_width;
    }

    if( mass == 0 )mass = 1;

    resx /= mass;
    resy /= mass;

    ret = Vector(trackingBox.x(), trackingBox.y(), 0) + Vector( resx, resy, 0 );

    return ret;
}


void cgmath::process_axes( void  )
{
    int cnt = 0;
    double t_delta = 0;

    if (Options::guideLogging())
        qDebug() << "Guide: Processing Axes";

    in_params.proportional_gain[0] = Options::rAPropotionalGain();
    in_params.proportional_gain[1] = Options::dECPropotionalGain();

    in_params.integral_gain[0]     = Options::rAIntegralGain();
    in_params.integral_gain[1]     = Options::rAIntegralGain();

    in_params.derivative_gain[0]   = Options::rADerivativeGain();
    in_params.derivative_gain[1]   = Options::dECDerivativeGain();

    in_params.enabled[0]           = Options::rAGuideEnabled();
    in_params.enabled[1]           = Options::dECGuideEnabled();

    in_params.min_pulse_length[0]  = Options::rAMinimumPulse();
    in_params.min_pulse_length[1]  = Options::dECMinimumPulse();

    in_params.max_pulse_length[0]  = Options::rAMaximumPulse();
    in_params.max_pulse_length[1]  = Options::dECMaximumPulse();

    // RA W/E enable
    // East RA+ enabled?
    in_params.enabled_axis1[0]     = Options::eastRAGuideEnabled();
    // West RA- enabled?
    in_params.enabled_axis2[0]     = Options::westRAGuideEnabled();

    // DEC N/S enable
    // North DEC+ enabled?
    in_params.enabled_axis1[1]     = Options::northDECGuideEnabled();
    // South DEC- enabled?
    in_params.enabled_axis2[1]     = Options::southDECGuideEnabled();

    // process axes...
    for( int k = GUIDE_RA;k <= GUIDE_DEC;k++ )
    {
        // zero all out commands
        out_params.pulse_dir[k] = NO_DIR;

        if( accum_ticks[k] < in_params.accum_frame_cnt[k]-1 )
            continue;

        t_delta = 0;
        drift_integral[k] = 0;

        cnt = in_params.accum_frame_cnt[ k ];

        for( int i = 0, idx = channel_ticks[k];i < cnt;++i )
        {
            t_delta += drift[k][idx];

            if (Options::guideLogging())
                qDebug() << "Guide: At #" << idx << "drift[" << k << "][" << idx << "] = " << drift[k][idx] << " , t_delta: " << t_delta ;

            if( idx > 0 )
                --idx;
            else
                idx = MAX_ACCUM_CNT-1;
        }

        for( int i = 0;i < MAX_ACCUM_CNT;++i )
            drift_integral[k] += drift[k][i];

        out_params.delta[k] = t_delta / (double)cnt;
        drift_integral[k] /= (double)MAX_ACCUM_CNT;

        if (Options::guideLogging())
        {
            //qDebug() << "cnt: " << cnt;
            qDebug() << "Guide: delta         [" << k << "]= "  << out_params.delta[k];
            qDebug() << "Guide: drift_integral[" << k << "]= "  << drift_integral[k];
        }

        out_params.pulse_length[k] = fabs(out_params.delta[k]*in_params.proportional_gain[k] + drift_integral[k]*in_params.integral_gain[k]);
        out_params.pulse_length[k] = out_params.pulse_length[k] <= in_params.max_pulse_length[k] ? out_params.pulse_length[k] : in_params.max_pulse_length[k];

        if (Options::guideLogging())
            qDebug() << "Guide: pulse_length  [" << k << "]= "  << out_params.pulse_length[k];

        // calc direction
        // We do not send pulse if direction is disabled completely, or if direction in a specific axis (e.g. N or S) is disabled
        if ( !in_params.enabled[k] || (out_params.delta[k] > 0 && !in_params.enabled_axis1[k]) || (out_params.delta[k] < 0 && !in_params.enabled_axis2[k]))
        {
            out_params.pulse_dir[k] = NO_DIR;
            out_params.pulse_length[k] = 0;
            continue;
        }

        if( out_params.pulse_length[k] >= in_params.min_pulse_length[k] )
        {
            if( k == GUIDE_RA )
                out_params.pulse_dir[k] = out_params.delta[k] > 0 ? RA_DEC_DIR : RA_INC_DIR;   // GUIDE_RA. right dir - decreases GUIDE_RA
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

        if (Options::guideLogging())
            qDebug() << "Guide: Direction     : " << get_direction_string(out_params.pulse_dir[k]);

    }

    //emit newAxisDelta(out_params.delta[0], out_params.delta[1]);

    QTextStream out(logFile);
    out << ticks << "," << logTime.elapsed() << "," << out_params.delta[0] << "," << out_params.pulse_length[0] << "," << get_direction_string(out_params.pulse_dir[0])
            << "," << out_params.delta[1] << "," << out_params.pulse_length[1] << "," << get_direction_string(out_params.pulse_dir[1]) << endl;

}


void cgmath::performProcessing( void )
{
    Vector arc_star_pos, arc_reticle_pos;

    // do nothing if suspended
    if( suspended )
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

    QVector3D starCenter(star_pos.x,star_pos.y, 0);
    emit newStarPosition(starCenter, true);

    if( preview_mode )
        return;

    if (Options::guideLogging())
        qDebug() << "Guide: ################## BEGIN PROCESSING ##################";

    // translate star coords into sky coord. system

    // convert from pixels into arcsecs
    arc_star_pos 	= point2arcsec( star_pos );
    arc_reticle_pos = point2arcsec( reticle_pos );


    if (Options::guideLogging())
    {
        qDebug() << "Guide: Star    X : " << star_pos.x << " Y  : " << star_pos.y;
        qDebug() << "Guide: Reticle X : " << reticle_pos.x << " Y  :" << reticle_pos.y;
        qDebug() << "Guide: Star    RA: " << arc_star_pos.x << " DEC: " << arc_star_pos.y;
        qDebug() << "Guide: Reticle RA: " << arc_reticle_pos.x << " DEC: " << arc_reticle_pos.y;
    }

    // translate into sky coords.
    star_pos = arc_star_pos - arc_reticle_pos;
    star_pos.y = -star_pos.y; // invert y-axis as y picture axis is inverted

    if (Options::guideLogging())
        qDebug() << "Guide: -------> BEFORE ROTATION Diff RA: " << star_pos.x << " DEC: " << star_pos.y;

    star_pos = star_pos * ROT_Z;

    // both coords are ready for math processing
    //put coord to drift list
    drift[GUIDE_RA][channel_ticks[GUIDE_RA]]   = star_pos.x;
    drift[GUIDE_DEC][channel_ticks[GUIDE_DEC]] = star_pos.y;

    if (Options::guideLogging())
    {
        qDebug() << "Guide: -------> AFTER ROTATION  Diff RA: " << star_pos.x << " DEC: " << star_pos.y;
        qDebug() << "Guide: RA channel ticks: " << channel_ticks[GUIDE_RA] << " DEC channel ticks: " << channel_ticks[GUIDE_DEC];
    }

    // make decision by axes
    process_axes();

    // process statistics
    calc_square_err();

    // finally process tickers
    do_ticks();

    if (Options::guideLogging())
        qDebug() << "Guide: ################## FINISH PROCESSING ##################";
}



void cgmath::calc_square_err( void )
{
    if( !do_statistics )
        return;
    // through MAX_ACCUM_CNT values
    if( ticks == 0 )
        return;

    for( int k = GUIDE_RA;k <= GUIDE_DEC;k++ )
    {
        double sqr_avg = 0;
        for( int i = 0;i < MAX_ACCUM_CNT;++i )
            sqr_avg += drift[k][i] * drift[k][i];

        out_params.sigma[k] = sqrt( sqr_avg / (double)MAX_ACCUM_CNT );
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


void cgmath::setLogFile(QFile *file)
{
    logFile = file;
    logTime.restart();
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

//---------------------------------------------------------------------------------------
cproc_in_params::cproc_in_params()
{
    reset();
}


void cproc_in_params::reset( void )
{
    threshold_alg_idx = CENTROID_THRESHOLD;
    guiding_rate = 0.5;
    average = true;

    for( int k = GUIDE_RA;k <= GUIDE_DEC;k++ )
    {
        enabled[k] 			 = true;
        accum_frame_cnt[k] 	 = 1;        
        integral_gain[k] 	 = 0;
        derivative_gain[k] 	 = 0;
        max_pulse_length[k]  = 5000;
        min_pulse_length[k]  = 100;
    }
}


cproc_out_params::cproc_out_params()
{
    reset();
}


void cproc_out_params::reset( void )
{
    for( int k = GUIDE_RA;k <= GUIDE_DEC;k++ )
    {
        delta[k] 		= 0;
        pulse_dir[k] 	= NO_DIR;
        pulse_length[k] = 0;
        sigma[k] 		= 0;
    }
}
