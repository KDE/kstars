/*
    This file is part of SEP

    SPDX-FileCopyrightText: 1993-2011 Emmanuel Bertin -- IAP /CNRS/UPMC
    SPDX-FileCopyrightText: 2014 SEP developers

    SPDX-License-Identifier: LGPL-3.0-or-later
*/

/* datatype codes */
#define SEP_TBYTE        11  /* 8-bit unsigned byte */
#define SEP_TINT         31  /* native int type */
#define SEP_TFLOAT       42
#define SEP_TDOUBLE      82

/* object & aperture flags */
#define SEP_OBJ_MERGED       0x0001  /* object is result of deblending */
#define SEP_OBJ_TRUNC        0x0002  /* object truncated at image boundary */
#define SEP_OBJ_DOVERFLOW    0x0004  /* not currently used, but could be */
#define SEP_OBJ_SINGU        0x0008  /* x,y fully correlated */
#define SEP_APER_TRUNC       0x0010
#define SEP_APER_HASMASKED   0x0020
#define SEP_APER_ALLMASKED   0x0040
#define SEP_APER_NONPOSITIVE 0x0080

/* noise_type values in sep_image */
#define SEP_NOISE_NONE   0
#define SEP_NOISE_STDDEV 1
#define SEP_NOISE_VAR    2

/* input flags for aperture photometry */
#define SEP_MASK_IGNORE      0x0004

/* threshold interpretation for sep_extract */
#define SEP_THRESH_REL 0  /* in units of standard deviations (sigma) */
#define SEP_THRESH_ABS 1  /* absolute data values */

/* filter types for sep_extract */
#define SEP_FILTER_CONV    0
#define SEP_FILTER_MATCHED 1

/* structs ------------------------------------------------------------------*/

/* sep_image
 *
 * Represents an image, including data, noise and mask arrays, and
 * gain.
 */
typedef struct {
  void *data;        /* data array                */
  void *noise;       /* noise array (can be NULL) */
  void *mask;        /* mask array (can be NULL)  */
  int dtype;         /* element type of image     */
  int ndtype;        /* element type of noise     */
  int mdtype;        /* element type of mask      */
  int w;             /* array width               */
  int h;             /* array height              */
  double noiseval;   /* scalar noise value; used only if noise == NULL */
  short noise_type;  /* interpretation of noise value                  */
  double gain;       /* (poisson counts / data unit)                   */
  double maskthresh; /* pixel considered masked if mask > maskthresh   */
} sep_image;

/* sep_bkg
 *
 * The result of sep_background() -- represents a smooth image background
 * and its noise with splines.
 */
typedef struct {
  int w, h;          /* original image width, height */
  int bw, bh;        /* single tile width, height */
  int nx, ny;        /* number of tiles in x, y */
  int n;             /* nx*ny */
  float global;      /* global mean */
  float globalrms;   /* global sigma */
  float *back;       /* node data for interpolation */
  float *dback;
  float *sigma;    
  float *dsigma;
} sep_bkg;

/* sep_catalog
 *
 * The result of sep_extract(). This is a struct of arrays. Each array has
 * one entry per detected object.
 */
typedef struct {
  int    nobj;                 /* number of objects (length of all arrays) */
  float	 *thresh;              /* threshold (ADU)                          */
  int	 *npix;                 /* # pixels extracted (size of pix array)   */
  int    *tnpix;                /* # pixels above thresh (unconvolved)      */
  int	 *xmin, *xmax;      
  int    *ymin, *ymax;
  double *x, *y;                 /* barycenter (first moments)               */
  double *x2, *y2, *xy;		 /* second moments                           */
  double *errx2, *erry2, *errxy;      /* second moment errors            */
  float	 *a, *b, *theta;    /* ellipse parameters                       */
  float	 *cxx, *cyy, *cxy;  /* ellipse parameters (alternative)         */
  float	 *cflux;                /* total flux of pixels (convolved im)      */
  float	 *flux;      		 /* total flux of pixels (unconvolved)       */
  float  *cpeak;                /* peak intensity (ADU) (convolved)         */
  float  *peak;                 /* peak intensity (ADU) (unconvolved)       */
  int    *xcpeak, *ycpeak;       /* x, y coords of peak (convolved) pixel    */
  int    *xpeak, *ypeak;         /* x, y coords of peak (unconvolved) pixel  */
  short	 *flag;                 /* extraction flags                         */
  int    **pix;             /* array giving indicies of object's pixels in   */
                            /* image (linearly indexed). Length is `npix`.  */
                            /* (pointer to within the `objectspix` buffer)  */
  int    *objectspix;      /* buffer holding pixel indicies for all objects */
} sep_catalog;

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------- global background estimation ------------------------*/

/* sep_background()
 * 
 * Create representation of spatially varying image background and variance.
 *
 * Note that the returned pointer must eventually be freed by calling 
 * `sep_bkg_free()`.
 *
 * In addition to the image mask (if present), pixels <= -1e30 and NaN
 * are ignored.
 * 
 * Source Extractor defaults:
 * 
 * - bw, bh = (64, 64)
 * - fw, fh = (3, 3)
 * - fthresh = 0.0
 */
int sep_background(sep_image *image,
                   int bw, int bh,   /* size of a single background tile */
                   int fw, int fh,   /* filter size in tiles             */
                   double fthresh,   /* filter threshold                 */
                   sep_bkg **bkg);   /* OUTPUT                           */


/* sep_bkg_global[rms]()
 *
 * Get the estimate of the global background "median" or standard deviation.
 */
float sep_bkg_global(sep_bkg *bkg);
float sep_bkg_globalrms(sep_bkg *bkg);


/* sep_bkg_pix()
 *
 * Return background at (x, y).
 * Unlike other routines, this uses simple linear interpolation.
 */
float sep_bkg_pix(sep_bkg *bkg, int x, int y);


/* sep_bkg_[sub,rms]line()
 * 
 * Evaluate the background or RMS at line `y`.
 * Uses bicubic spline interpolation between background map vertices.
 * The second function subtracts the background from the input array.
 * Line must be an array with same width as original image.
 */
int sep_bkg_line(sep_bkg *bkg, int y, void *line, int dtype);
int sep_bkg_subline(sep_bkg *bkg, int y, void *line, int dtype);
int sep_bkg_rmsline(sep_bkg *bkg, int y, void *line, int dtype);


/* sep_bkg_[sub,rms]array()
 * 
 * Evaluate the background or RMS for entire image.
 * Uses bicubic spline interpolation between background map vertices.
 * The second function subtracts the background from the input array.
 * `arr` must be an array of the same size as original image.
 */
int sep_bkg_array(sep_bkg *bkg, void *arr, int dtype);
int sep_bkg_subarray(sep_bkg *bkg, void *arr, int dtype);
int sep_bkg_rmsarray(sep_bkg *bkg, void *arr, int dtype);

/* sep_bkg_free()
 *
 * Free memory associated with bkg.
 */
void sep_bkg_free(sep_bkg *bkg);

/*-------------------------- source extraction ------------------------------*/

/* sep_extract()
 *
 * Extract sources from an image. Source Extractor defaults are shown
 * in [ ] above.
 *
 * Notes
 * -----
 * `dtype` and `ndtype` indicate the data type (float, int, double) of the 
 * image and noise arrays, respectively.
 *
 * If `noise` is NULL, thresh is interpreted as an absolute threshold.
 * If `noise` is not null, thresh is interpreted as a relative threshold
 * (the absolute threshold will be thresh*noise[i,j]).
 * 
 */
int sep_extract(sep_image *image,
		float thresh,         /* detection threshold           [1.5] */
                int thresh_type,      /* threshold units    [SEP_THRESH_REL] */
		int minarea,          /* minimum area in pixels          [5] */
		float *conv,          /* convolution array (can be NULL)     */
                                      /*               [{1 2 1 2 4 2 1 2 1}] */
		int convw, int convh, /* w, h of convolution array     [3,3] */
                int filter_type,      /* convolution (0) or matched (1)  [0] */
		int deblend_nthresh,  /* deblending thresholds          [32] */
		double deblend_cont,  /* min. deblending contrast    [0.005] */
		int clean_flag,       /* perform cleaning?               [1] */
		double clean_param,   /* clean parameter               [1.0] */
                sep_catalog **catalog); /* OUTPUT catalog                    */



/* set and get the size of the pixel stack used in extract() */
void sep_set_extract_pixstack(size_t val);
size_t sep_get_extract_pixstack(void);

/* free memory associated with a catalog */
void sep_catalog_free(sep_catalog *catalog);

/*-------------------------- aperture photometry ----------------------------*/


/* Sum array values within a circular aperture.
 * 
 * Notes
 * -----
 * error : Can be a scalar (default), an array, or NULL
 *         If an array, set the flag SEP_ERROR_IS_ARRAY in `inflags`.
 *         Can represent 1-sigma std. deviation (default) or variance.
 *         If variance, set the flag SEP_ERROR_IS_VARIANCE in `inflags`.
 *
 * gain : If 0.0, poisson noise on sum is ignored when calculating error.
 *        Otherwise, (sum / gain) is added to the variance on sum.
 *
 * area : Total pixel area included in sum. Includes masked pixels that were
 *        corrected. The area can differ from the exact area of a circle due
 *        to inexact subpixel sampling and intersection with array boundaries.
 */
int sep_sum_circle(sep_image *image,
		   double x,          /* center of aperture in x */
		   double y,          /* center of aperture in y */
		   double r,          /* radius of aperture */
		   int subpix,        /* subpixel sampling */
		   short inflags,     /* input flags (see below) */
		   double *sum,       /* OUTPUT: sum */
		   double *sumerr,    /* OUTPUT: error on sum */
		   double *area,      /* OUTPUT: area included in sum */
		   short *flag);      /* OUTPUT: flags */


int sep_sum_circann(sep_image *image,
                    double x, double y, double rin, double rout,
                    int subpix, short inflags,
		    double *sum, double *sumerr, double *area, short *flag);

int sep_sum_ellipse(sep_image *image,
		    double x, double y, double a, double b, double theta,
		    double r, int subpix, short inflags,
		    double *sum, double *sumerr, double *area, short *flag);

int sep_sum_ellipann(sep_image *image,
		     double x, double y, double a, double b, double theta,
		     double rin, double rout, int subpix, short inflags,
		     double *sum, double *sumerr, double *area, short *flag);

/* sep_sum_circann_multi()
 *
 * Sum an array of circular annuli more efficiently (but with no exact mode).
 *
 * Notable parameters:
 * 
 * rmax:     Input radii are  [rmax/n, 2*rmax/n, 3*rmax/n, ..., rmax].
 * n:        Length of input and output arrays.
 * sum:      Preallocated array of length n holding sums in annuli. sum[0]
 *           corresponds to r=[0, rmax/n], sum[n-1] to outermost annulus.
 * sumvar:   Preallocated array of length n holding variance on sums.
 * area:     Preallocated array of length n holding area summed in each annulus.
 * maskarea: Preallocated array of length n holding masked area in each
             annulus (if mask not NULL).
 * flag:     Output flag (non-array).
 */
int sep_sum_circann_multi(sep_image *im,
			  double x, double y, double rmax, int n, int subpix,
                          short inflag,
			  double *sum, double *sumvar, double *area,
			  double *maskarea, short *flag);

/* sep_flux_radius()
 *
 * Calculate the radii enclosing the requested fraction of flux relative
 * to radius rmax.
 *
 * (see previous functions for most arguments)
 * rmax : maximum radius to analyze
 * fluxtot : scale requested flux fractions to this. (If NULL, flux within
             `rmax` is used.)
 * fluxfrac : array of requested fractions.
 * n : length of fluxfrac
 * r : (output) result array of length n.
 * flag : (output) scalar flag
 */
int sep_flux_radius(sep_image *im,
		    double x, double y, double rmax, int subpix, short inflag,
		    double *fluxtot, double *fluxfrac, int n,
		    double *r, short *flag);

/* sep_kron_radius()
 *
 * Calculate Kron radius within an ellipse given by 
 *
 *     cxx*(x'-x)^2 + cyy*(y'-y)^2 + cxy*(x'-x)*(y'-y) < r^2
 *
 * The Kron radius is sum(r_i * v_i) / sum(v_i) where v_i is the value of pixel
 * i and r_i is the "radius" of pixel i, as given by the left hand side of
 * the above equation.
 *
 * Flags that might be set:
 * SEP_APER_HASMASKED - at least one of the pixels in the ellipse is masked.
 * SEP_APER_ALLMASKED - All pixels in the ellipse are masked. kronrad = 0.
 * SEP_APER_NONPOSITIVE - There was a non-positive numerator or denominator.
 *                        kronrad = 0.
 */
int sep_kron_radius(sep_image *im, double x, double y,
		    double cxx, double cyy, double cxy, double r,
		    double *kronrad, short *flag);


/* sep_windowed()
 *
 * Calculate "windowed" position parameters via an iterative procedure.
 *
 * x, y       : initial center
 * sig        : sigma of Gaussian to use for weighting. The integration
 *              radius is 4 * sig.
 * subpix     : Subpixels to use in aperture-pixel overlap.
 *              SExtractor uses 11. 0 is supported for exact overlap.
 * xout, yout : output center.
 * niter      : number of iterations used.
 */
int sep_windowed(sep_image *im,
                 double x, double y, double sig, int subpix, short inflag,
                 double *xout, double *yout, int *niter, short *flag);


/* sep_set_ellipse()
 *
 * Set array elements within an elliptical aperture to a given value.
 *
 * Ellipse: cxx*(x'-x)^2 + cyy*(y'-y)^2 + cxy*(x'-x)*(y'-y) = r^2  
 */
void sep_set_ellipse(unsigned char *arr, int w, int h,
		     double x, double y, double cxx, double cyy, double cxy,
		     double r, unsigned char val);


/* sep_ellipse_axes()
 * sep_ellipse_coeffs()
 *
 * Convert between coefficient representation of ellipse,
 * cxx*(x'-x)^2 + cyy*(y'-y)^2 + cxy*(x'-x)*(y'-y) = r^2,
 * and axis representation of an ellipse. The axis representation is
 * defined by:
 *
 * a = semimajor axis
 * b = semiminor axis
 * theta = angle in radians counter-clockwise from positive x axis
 */
int sep_ellipse_axes(double cxx, double cyy, double cxy,
		     double *a, double *b, double *theta);
void sep_ellipse_coeffs(double a, double b, double theta,
			double *cxx, double *cyy, double *cxy);

/*----------------------- info & error messaging ----------------------------*/

/* sep_version_string : library version (e.g., "0.2.0") */
extern char *sep_version_string;

/* sep_get_errmsg()
 *
 * Return a short descriptive error message that corresponds to the input
 * error status value.  The message may be up to 60 characters long, plus
 * the terminating null character.
 */
void sep_get_errmsg(int status, char *errtext);


/* sep_get_errdetail()
 *
 * Return a longer error message with more specifics about the problem.
 * The message may be up to 512 characters.
 */
void sep_get_errdetail(char *errtext);

#ifdef __cplusplus
}
#endif
