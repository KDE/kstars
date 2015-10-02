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

#include <QObject>
#include <QTime>

#include <stdint.h>
#include <sys/types.h>
#include "vect.h"
#include "matr.h"
#include "common.h"

class FITSView;

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



typedef struct
{
	enum type_t
	{
		OVR_SQUARE = 1,
		OVR_RETICLE = 2
	};
	int visible;
	int square_size;
	point_t square_pos;
	point_t reticle_axis_ra[2];
	point_t reticle_axis_dec[2];
	point_t reticle_pos;
}ovr_params_t;


// input params
class cproc_in_params
{
public:
	cproc_in_params();
	void reset( void );

	int       threshold_alg_idx;
	double    guiding_rate;
	bool      enabled[CHANNEL_CNT];
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
	bool set_video_params( int vid_wd, int vid_ht );
    float *get_data_buffer( int *width, int *height, int *length, int *size );
	bool set_guider_params( double ccd_pix_wd, double ccd_pix_ht, double guider_aperture, double guider_focal );
    void get_guider_params( double *ccd_pix_wd, double *ccd_pix_ht, double *guider_aperture, double *guider_focal );
	bool set_reticle_params( double x, double y, double ang );
	bool get_reticle_params( double *x, double *y, double *ang ) const;
	int  get_square_index( void ) const;
	int  get_square_algorithm_index( void ) const;
    int  get_square_size() { return square_size; }
	void set_square_algorithm( int alg_idx );
    Matrix get_ROTZ() { return ROT_Z; }
	cproc_in_params *get_in_params( void );
	void set_in_params( const cproc_in_params *v );
	const cproc_out_params *get_out_params( void ) const;
	info_params_t get_info_params( void ) const;
	uint32_t get_ticks( void ) const;
	void get_star_drift( double *dx, double *dy ) const;
	void get_star_screen_pos( double *dx, double *dy ) const;
	bool reset( void );
    void set_buffer(float *buffer);
    void set_image(FITSView *image);
    bool get_dec_swap() { return dec_swap;}
    void set_dec_swap(bool enable) { dec_swap = enable;}
    FITSView *get_image() { return pimage; }
    void set_preview_mode(bool enable) { preview_mode = enable;}
	
	ovr_params_t *prepare_overlays( void );
	void move_square( double newx, double newy );
	void resize_square( int size_idx );

    void setRapidGuide(bool enable);
    void setRapidStarData(double dx, double dy);
	
	// proc
	void start( void );
	void stop( void );
	void suspend( bool mode );
	bool is_suspended( void ) const;
    bool is_lost_star(void) const;
    void set_lost_star(bool is_lost);
	void do_processing( void );
	static double precalc_proportional_gain( double g_rate );
    bool calc_and_set_reticle( double start_x, double start_y, double end_x, double end_y, int totalPulse=-1);
    bool calc_and_set_reticle2( double start_ra_x, double start_ra_y, double end_ra_x, double end_ra_y, double start_dec_x, double start_dec_y, double end_dec_x, double end_dec_y, bool *swap_dec, int totalPulse=-1);
	double calc_phi( double start_x, double start_y, double end_x, double end_y ) const;
    Vector find_star_local_pos( void ) const;
    double get_dither_rate(int axis);
    void set_log_file(QFile *file);

signals:
    void newAxisDelta(double delta_ra, double delta_dec);

private:
	// sys...
	uint32_t ticks;		// global channel ticker
    float *pdata;		// pointer to data buffer
    FITSView *pimage;   // pointer to image
	int video_width, video_height;	// video frame dimensions
	double ccd_pixel_width, ccd_pixel_height, aperture, focal;
	Matrix	ROT_Z;
    bool preview_mode, suspended, lost_star, dec_swap;
	
	// square variables
	int square_size;	// size of analysing square
	double square_square; // square of guiding rect
	Vector square_pos;	// integer values in double vars.
	int square_idx;		// index in size list
	int square_alg_idx;		// index of threshold algorithm
	
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
	ovr_params_t overlays;
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
