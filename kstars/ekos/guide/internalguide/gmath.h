/*  Ekos guide tool
    Copyright (C) 2012 Andrew Stepanenko

    Modified by Jasem Mutlaq <mutlaqja@ikarustech.com> for KStars.

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef GMATH_H_
#define GMATH_H_

#include <stdint.h>
#include <sys/types.h>

#include <QObject>
#include <QTime>
#include <QPointer>

#include "fitsviewer/fitsview.h"
#include "indi/indicommon.h"

#include "vect.h"
#include "matr.h"

typedef struct
{
    int size;
    double square;
}guide_square_t;

#define SMART_THRESHOLD 0
#define CENTROID_THRESHOLD 1
#define AUTO_THRESHOLD	2
#define NO_THRESHOLD  	3

typedef struct
{
    int idx;
    const char name[32];
}square_alg_t;

// smart threshold algorithm param
// width of outer frame for backgroung calculation
#define SMART_FRAME_WIDTH	4
// cut-factor above avarage threshold
#define SMART_CUT_FACTOR	0.1


#define GUIDE_RA	0
#define GUIDE_DEC	1
#define CHANNEL_CNT	2
#define DEFAULT_SQR	1

#define  MAX_ACCUM_CNT	50
extern const guide_square_t guide_squares[];
extern const square_alg_t guide_square_alg[];

// input params
class cproc_in_params
{
public:
    cproc_in_params();
    void reset( void );

    int       threshold_alg_idx;
    double    guiding_rate;
    bool      enabled[CHANNEL_CNT];
    bool      enabled_axis1[CHANNEL_CNT];
    bool      enabled_axis2[CHANNEL_CNT];
    bool      average;
    uint32_t  accum_frame_cnt[CHANNEL_CNT];
    double    proportional_gain[CHANNEL_CNT];
    double    integral_gain[CHANNEL_CNT];
    double    derivative_gain[CHANNEL_CNT];
    int       max_pulse_length[CHANNEL_CNT];
    int       min_pulse_length[CHANNEL_CNT];
};


//output params
class cproc_out_params
{
public:
    cproc_out_params();
    void reset( void );

    double  	delta[2];
    GuideDirection 	pulse_dir[2];
    int	    	pulse_length[2];
    double		sigma[2];
};


typedef struct
{
 double focal_ratio;
 double fov_wd, fov_ht;
 double focal, aperture;
}info_params_t;


class cgmath : public QObject
{
    Q_OBJECT

public:
    cgmath();
    virtual ~cgmath();

    // functions
    bool setVideoParameters( int vid_wd, int vid_ht, int binX, int binY );
    bool setGuiderParameters( double ccd_pix_wd, double ccd_pix_ht, double guider_aperture, double guider_focal );
    void getGuiderParameters( double *ccd_pix_wd, double *ccd_pix_ht, double *guider_aperture, double *guider_focal );
    bool setReticleParameters( double x, double y, double ang );
    bool getReticleParameters( double *x, double *y, double *ang ) const;
    int  getSquareIndex( void ) const;
    int  getSquareAlgorithmIndex( void ) const;
    int  getSquareSize() { return squareSize; }
    void setSquareAlgorithm( int alg_idx );

    Matrix getROTZ() { return ROT_Z; }        
    const cproc_out_params *getOutputParameters() const { return &out_params; }
    info_params_t getInfoParameters( void ) const;
    uint32_t getTicks( void ) const;

    void setGuideView(FITSView *image);
    bool declinationSwapEnabled() { return dec_swap;}
    void setDeclinationSwapEnabled(bool enable) { dec_swap = enable;}
    FITSView *getGuideView() { return guideView; }
    void setPreviewMode(bool enable) { preview_mode = enable;}

    /*void moveSquare( double newx, double newy );
    void resizeSquare( int size_idx );
    Vector getSquarePosition() { return square_pos; }*/

    // Rapid Guide
    void setRapidGuide(bool enable);
    void setRapidStarData(double dx, double dy);

    // Operations
    void start( void );
    void stop( void );
    bool reset( void );
    void suspend( bool mode );
    bool isSuspended( void ) const;

    // Star tracking
    void getStarDrift( double *dx, double *dy ) const;
    void getStarScreenPosition( double *dx, double *dy ) const;
    Vector findLocalStarPosition( void ) const;
    bool isStarLost(void) const;
    void setLostStar(bool is_lost);    

    // Main processing function
    void performProcessing( void );

    // Math
    bool calculateAndSetReticle1D( double start_x, double start_y, double end_x, double end_y, int totalPulse=-1);
    bool calculateAndSetReticle2D( double start_ra_x, double start_ra_y, double end_ra_x, double end_ra_y, double start_dec_x, double start_dec_y, double end_dec_x, double end_dec_y, bool *swap_dec, int totalPulse=-1);
    double calculatePhi( double start_x, double start_y, double end_x, double end_y ) const;

    // Dither
    double getDitherRate(int axis);

    // Logging
    void setLogFile(QFile *file);

signals:
    void newAxisDelta(double delta_ra, double delta_dec);
    void newStarPosition(QVector3D, bool);

private:

    // Templated functions
    template<typename T> Vector findLocalStarPosition( void ) const;
    // sys...
    uint32_t ticks;		// global channel ticker    
    QPointer<FITSView> guideView;   // pointer to image
    int video_width, video_height;	// video frame dimensions
    double ccd_pixel_width, ccd_pixel_height, aperture, focal;
    Matrix	ROT_Z;
    bool preview_mode, suspended, lost_star, dec_swap;

    // square variables
    int squareSize;	// size of analysing square
    int square_alg_idx;		// index of threshold algorithm
    int subBinX,subBinY;

    // sky coord. system vars.
    Vector star_pos;	// position of star in reticle coord. system
    Vector scr_star_pos; // sctreen star position
    Vector reticle_pos;
    Vector reticle_orts[2];
    double reticle_angle;

    // processing
    uint32_t  channel_ticks[2];
    uint32_t  accum_ticks[2];
    double *drift[2];
    double drift_integral[2];

    // overlays...
    cproc_in_params  in_params;
    cproc_out_params out_params;

    // stat math...
    bool do_statistics;
    double sum, sqr_sum;
    double delta_prev, sigma_prev, sigma;

    // proc
    void do_ticks( void );
    Vector point2arcsec( const Vector &p ) const;
    void process_axes( void );
    void calc_square_err( void );
    const char *get_direction_string(GuideDirection dir);

    // rapid guide
    bool useRapidGuide;
    double rapidDX, rapidDY;

    // dithering
    double ditherRate[2];

    QFile *logFile;
    QTime logTime;

};

#endif /*GMATH_H_*/
