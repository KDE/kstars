/*
    This file is part of SEP

    SPDX-FileCopyrightText: 1993-2011 Emmanuel Bertin -- IAP /CNRS/UPMC
    SPDX-FileCopyrightText: 2014 SEP developers

    SPDX-License-Identifier: LGPL-3.0-or-later
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sep.h"
#include "sepcore.h"
#include "overlap.h"

#define FLUX_RADIUS_BUFSIZE 64

#define WINPOS_NITERMAX 16      /* Maximum number of steps */
#define WINPOS_NSIG     4       /* Measurement radius */
#define WINPOS_STEPMIN  0.0001  /* Minimum change in position for continuing */
#define WINPOS_FAC      2.0     /* Centroid offset factor (2 for a Gaussian) */

/*
  Adding (void *) pointers is a GNU C extension, not part of standard C. 
  When compiling on Windows with MS VIsual C compiler need to cast the
  (void *) to something the size of one byte.
*/
#if defined(_MSC_VER)
  #define MSVC_VOID_CAST (char *)
#else
  #define MSVC_VOID_CAST
#endif

/****************************************************************************/
/* conversions between ellipse representations */

/* return ellipse semi-major and semi-minor axes and position angle, given
   representation: cxx*x^2 + cyy*y^2 + cxy*x*y = 1
   derived from http://mathworld.wolfram.com/Ellipse.html

   Input requirements:
   cxx*cyy - cxy*cxy/4. > 0.
   cxx + cyy > 0.
*/
int sep_ellipse_axes(double cxx, double cyy, double cxy,
                     double *a, double *b, double *theta)
{
  double p, q, t;

  p = cxx + cyy;
  q = cxx - cyy;
  t = sqrt(q*q + cxy*cxy);

  /* Ensure that parameters actually describe an ellipse. */
  if ((cxx*cyy - cxy*cxy/4. <= 0.) || (p <= 0.))
    return NON_ELLIPSE_PARAMS;

  *a = sqrt(2. / (p - t));
  *b = sqrt(2. / (p + t));

  /* theta = 0 if cxy == 0, else (1/2) acot(q/cxy) */
  *theta = (cxy == 0.) ? 0. : (q == 0. ? 0. : atan(cxy/q))/2.;
  if (cxx>cyy)
    *theta += PI/2.;
  if (*theta > PI/2.)
    *theta -= PI;

  return RETURN_OK;
}

void sep_ellipse_coeffs(double a, double b, double theta,
                        double *cxx, double *cyy, double *cxy)
{
  double costheta, sintheta;

  costheta = cos(theta);
  sintheta = sin(theta);

  *cxx = costheta*costheta/(a*a) + sintheta*sintheta/(b*b);
  *cyy = sintheta*sintheta/(a*a) + costheta*costheta/(b*b);
  *cxy = 2.*costheta*sintheta * (1./(a*a) - 1./(b*b));
}

/*****************************************************************************/
/* Helper functions for aperture functions */

/* determine the extent of the box that just contains the circle with
 * parameters x, y, r. xmin is inclusive and xmax is exclusive.
 * Ensures that box is within image bound and sets a flag if it is not.
 */
static void boxextent(double x, double y, double rx, double ry, int w, int h,
                      int *xmin, int *xmax, int *ymin, int *ymax,
                      short *flag)
{
  *xmin = (int)(x - rx + 0.5);
  *xmax = (int)(x + rx + 1.4999999);
  *ymin = (int)(y - ry + 0.5);
  *ymax = (int)(y + ry + 1.4999999);
  if (*xmin < 0)
    {
      *xmin = 0;
      *flag |= SEP_APER_TRUNC;
    }
  if (*xmax > w)
    {
      *xmax = w;
      *flag |= SEP_APER_TRUNC;
    }
  if (*ymin < 0)
    {
      *ymin = 0;
      *flag |= SEP_APER_TRUNC;
    }
  if (*ymax > h)
    {
      *ymax = h;
      *flag |= SEP_APER_TRUNC;
    }
}


static void boxextent_ellipse(double x, double y,
                              double cxx, double cyy, double cxy, double r,
                              int w, int h,
                              int *xmin, int *xmax, int *ymin, int *ymax,
                              short *flag)
{
  double dxlim, dylim;

  dxlim = cxx - cxy*cxy/(4.0*cyy);
  dxlim = dxlim>0.0 ? r/sqrt(dxlim) : 0.0;
  dylim = cyy - cxy*cxy/(4.0*cxx);
  dylim = dylim > 0.0 ? r/sqrt(dylim) : 0.0;
  boxextent(x, y, dxlim, dylim, w, h, xmin, xmax, ymin, ymax, flag);
}

/* determine oversampled annulus for a circle */
static void oversamp_ann_circle(double r, double *r_in2, double *r_out2)
{
   *r_in2 = r - 0.7072;
   *r_in2 = (*r_in2 > 0.0) ? (*r_in2)*(*r_in2) : 0.0;
   *r_out2 = r + 0.7072;
   *r_out2 = (*r_out2) * (*r_out2);
}

/* determine oversampled "annulus" for an ellipse */
static void oversamp_ann_ellipse(double r, double b, double *r_in2,
                                 double *r_out2)
{
   *r_in2 = r - 0.7072/b;
   *r_in2 = (*r_in2 > 0.0) ? (*r_in2)*(*r_in2) : 0.0;
   *r_out2 = r + 0.7072/b;
   *r_out2 = (*r_out2) * (*r_out2);
}

/*****************************************************************************/
/* circular aperture */

#define APER_NAME sep_sum_circle
#define APER_ARGS double r
#define APER_DECL double r2, r_in2, r_out2
#define APER_CHECKS                             \
  if (r < 0.0)                                  \
    return ILLEGAL_APER_PARAMS
#define APER_INIT                               \
  r2 = r*r;                                     \
  oversamp_ann_circle(r, &r_in2, &r_out2)
#define APER_BOXEXTENT boxextent(x, y, r, r, im->w, im->h,              \
                                 &xmin, &xmax, &ymin, &ymax, flag)
#define APER_EXACT circoverlap(dx-0.5, dy-0.5, dx+0.5, dy+0.5, r)
#define APER_RPIX2 dx*dx + dy*dy
#define APER_RPIX2_SUBPIX dx1*dx1 + dy2
#define APER_COMPARE1 rpix2 < r_out2
#define APER_COMPARE2 rpix2 > r_in2
#define APER_COMPARE3 rpix2 < r2
#include "aperture.i"
#undef APER_NAME
#undef APER_ARGS
#undef APER_DECL
#undef APER_CHECKS
#undef APER_INIT
#undef APER_BOXEXTENT
#undef APER_EXACT
#undef APER_RPIX2
#undef APER_RPIX2_SUBPIX
#undef APER_COMPARE1
#undef APER_COMPARE2
#undef APER_COMPARE3

/*****************************************************************************/
/* elliptical aperture */

#define APER_NAME sep_sum_ellipse
#define APER_ARGS double a, double b, double theta, double r
#define APER_DECL double cxx, cyy, cxy, r2, r_in2, r_out2
#define APER_CHECKS                                                     \
  if (!(r >= 0.0 && b >= 0.0 && a >= b &&                               \
        theta >= -PI/2. && theta <= PI/2.))                             \
    return ILLEGAL_APER_PARAMS
#define APER_INIT                                               \
  r2 = r*r;                                                     \
  oversamp_ann_ellipse(r, b, &r_in2, &r_out2);                  \
  sep_ellipse_coeffs(a, b, theta, &cxx, &cyy, &cxy);            \
  a *= r;                                                       \
  b *= r
#define APER_BOXEXTENT boxextent_ellipse(x, y, cxx, cyy, cxy, r, im->w, im->h, \
                                         &xmin, &xmax, &ymin, &ymax, flag)
#define APER_EXACT ellipoverlap(dx-0.5, dy-0.5, dx+0.5, dy+0.5, a, b, theta)
#define APER_RPIX2 cxx*dx*dx + cyy*dy*dy + cxy*dx*dy
#define APER_RPIX2_SUBPIX cxx*dx1*dx1 + cyy*dy2 + cxy*dx1*dy
#define APER_COMPARE1 rpix2 < r_out2
#define APER_COMPARE2 rpix2 > r_in2
#define APER_COMPARE3 rpix2 < r2
#include "aperture.i"
#undef APER_NAME
#undef APER_ARGS
#undef APER_DECL
#undef APER_CHECKS
#undef APER_INIT
#undef APER_BOXEXTENT
#undef APER_EXACT
#undef APER_RPIX2
#undef APER_RPIX2_SUBPIX
#undef APER_COMPARE1
#undef APER_COMPARE2
#undef APER_COMPARE3

/*****************************************************************************/
/* circular annulus aperture */

#define APER_NAME sep_sum_circann
#define APER_ARGS double rin, double rout
#define APER_DECL double rin2, rin_in2, rin_out2, rout2, rout_in2, rout_out2
#define APER_CHECKS                             \
  if (!(rin >= 0.0 && rout >= rin))             \
    return ILLEGAL_APER_PARAMS
#define APER_INIT                                       \
  rin2 = rin*rin;                                       \
  oversamp_ann_circle(rin, &rin_in2, &rin_out2);        \
  rout2 = rout*rout;                                    \
  oversamp_ann_circle(rout, &rout_in2, &rout_out2)
#define APER_BOXEXTENT boxextent(x, y, rout, rout, im->w, im->h, \
                                 &xmin, &xmax, &ymin, &ymax, flag)
#define APER_EXACT (circoverlap(dx-0.5, dy-0.5, dx+0.5, dy+0.5, rout) - \
                    circoverlap(dx-0.5, dy-0.5, dx+0.5, dy+0.5, rin))
#define APER_RPIX2 dx*dx + dy*dy
#define APER_RPIX2_SUBPIX dx1*dx1 + dy2
#define APER_COMPARE1 (rpix2 < rout_out2) && (rpix2 > rin_in2)
#define APER_COMPARE2 (rpix2 > rout_in2) || (rpix2 < rin_out2)
#define APER_COMPARE3 (rpix2 < rout2) && (rpix2 > rin2)
#include "aperture.i"
#undef APER_NAME
#undef APER_ARGS
#undef APER_DECL
#undef APER_CHECKS
#undef APER_INIT
#undef APER_BOXEXTENT
#undef APER_EXACT
#undef APER_RPIX2
#undef APER_RPIX2_SUBPIX
#undef APER_COMPARE1
#undef APER_COMPARE2
#undef APER_COMPARE3

/*****************************************************************************/
/* elliptical annulus aperture */

#define APER_NAME sep_sum_ellipann
#define APER_ARGS double a, double b, double theta, double rin, double rout
#define APER_DECL                                               \
  double cxx, cyy, cxy;                                         \
  double rin2, rin_in2, rin_out2, rout2, rout_in2, rout_out2
#define APER_CHECKS                                         \
  if (!(rin >= 0.0 && rout >= rin && b >= 0.0 && a >= b &&  \
        theta >= -PI/2. && theta <= PI/2.))                 \
    return ILLEGAL_APER_PARAMS
#define APER_INIT                                               \
  rin2 = rin*rin;                                               \
  oversamp_ann_ellipse(rin, b, &rin_in2, &rin_out2);            \
  rout2 = rout*rout;                                            \
  oversamp_ann_ellipse(rout, b, &rout_in2, &rout_out2);         \
  sep_ellipse_coeffs(a, b, theta, &cxx, &cyy, &cxy)
#define APER_BOXEXTENT boxextent_ellipse(x, y, cxx, cyy, cxy, rout, im->w, im->h, \
                                         &xmin, &xmax, &ymin, &ymax, flag)
#define APER_EXACT                                                      \
  (ellipoverlap(dx-0.5, dy-0.5, dx+0.5, dy+0.5, a*rout, b*rout, theta) - \
   ellipoverlap(dx-0.5, dy-0.5, dx+0.5, dy+0.5, a*rin, b*rin, theta))
#define APER_RPIX2 cxx*dx*dx + cyy*dy*dy + cxy*dx*dy
#define APER_RPIX2_SUBPIX cxx*dx1*dx1 + cyy*dy2 + cxy*dx1*dy
#define APER_COMPARE1 (rpix2 < rout_out2) && (rpix2 > rin_in2)
#define APER_COMPARE2 (rpix2 > rout_in2) || (rpix2 < rin_out2)
#define APER_COMPARE3 (rpix2 < rout2) && (rpix2 > rin2)
#include "aperture.i"
#undef APER_NAME
#undef APER_ARGS
#undef APER_DECL
#undef APER_CHECKS
#undef APER_INIT
#undef APER_BOXEXTENT
#undef APER_EXACT
#undef APER_RPIX2
#undef APER_RPIX2_SUBPIX
#undef APER_COMPARE1
#undef APER_COMPARE2
#undef APER_COMPARE3


/*****************************************************************************/
/*
 * This is just different enough from the other aperture functions
 * that it doesn't quite make sense to use aperture.i.
 */
int sep_sum_circann_multi(sep_image *im,
                          double x, double y, double rmax, int n, int subpix,
                          short inflag,
                          double *sum, double *sumvar, double *area,
                          double *maskarea, short *flag)
{
  PIXTYPE pix, varpix;
  double dx, dy, dx1, dy2, offset, scale, scale2, tmp, rpix2;
  int ix, iy, xmin, xmax, ymin, ymax, sx, sy, status, size, esize, msize;
  long pos;
  short errisarray, errisstd;
  BYTE *datat, *errort, *maskt;
  converter convert, econvert, mconvert;
  double rpix, r_out, r_out2, d, prevbinmargin, nextbinmargin, step, stepdens;
  int j, ismasked;

  /* input checks */
  if (rmax < 0.0 || n < 1)
    return ILLEGAL_APER_PARAMS;
  if (subpix < 1)
    return ILLEGAL_SUBPIX;

  /* clear results arrays */
  memset(sum, 0, (size_t)(n*sizeof(double)));
  memset(sumvar, 0, (size_t)(n*sizeof(double)));
  memset(area, 0, (size_t)(n*sizeof(double)));
  if (im->mask)
    memset(maskarea, 0, (size_t)(n*sizeof(double)));

  /* initializations */
  size = esize = msize = 0;
  datat = maskt = NULL;
  errort = im->noise;
  *flag = 0;
  varpix = 0.0;
  scale = 1.0/subpix;
  scale2 = scale*scale;
  offset = 0.5*(scale-1.0);

  r_out = rmax + 1.5; /* margin for interpolation */
  r_out2 = r_out * r_out;
  step = rmax/n;
  stepdens = 1.0/step;
  prevbinmargin = 0.7072;
  nextbinmargin = step - 0.7072;
  j = 0;
  d = 0.;
  ismasked = 0;
  errisarray = 0;
  errisstd = 0;

  /* get data converter(s) for input array(s) */
  if ((status = get_converter(im->dtype, &convert, &size)))
    return status;
  if (im->mask && (status = get_converter(im->mdtype, &mconvert, &msize)))
    return status;

  /* get image noise */
  if (im->noise_type != SEP_NOISE_NONE)
    {
      errisstd = (im->noise_type == SEP_NOISE_STDDEV);
      if (im->noise)
        {
          errisarray = 1;
          if ((status = get_converter(im->ndtype, &econvert, &esize)))
            return status;
        }
      else
        {
          varpix = (errisstd)?  im->noiseval * im->noiseval: im->noiseval;
        }
    }


  /* get extent of box */
  boxextent(x, y, r_out, r_out, im->w, im->h, &xmin, &xmax, &ymin, &ymax,
            flag);

  /* loop over rows in the box */
  for (iy=ymin; iy<ymax; iy++)
    {
      /* set pointers to the start of this row */
      pos = (iy % im->h) * im->w + xmin;
      datat = MSVC_VOID_CAST im->data + pos*size;
      if (errisarray)
        errort = MSVC_VOID_CAST im->noise + pos*esize;
      if (im->mask)
        maskt = MSVC_VOID_CAST im->mask + pos*msize;

      /* loop over pixels in this row */
      for (ix=xmin; ix<xmax; ix++)
        {
          dx = ix - x;
          dy = iy - y;
          rpix2 = dx*dx + dy*dy;
          if (rpix2 < r_out2)
            {
              /* get pixel values */
              pix = convert(datat);
              if (errisarray)
                {
                  varpix = econvert(errort);
                  if (errisstd)
                    varpix *= varpix;
                }
              if (im->mask)
                {
                  if (mconvert(maskt) > im->maskthresh)
                    {
                      *flag |= SEP_APER_HASMASKED;
                      ismasked = 1;
                    }
                  else
                    ismasked = 0;
                }

              /* check if oversampling is needed (close to bin boundary?) */
              rpix = sqrt(rpix2);
              d = fmod(rpix, step);
              if (d < prevbinmargin || d > nextbinmargin)
                {
                  dx += offset;
                  dy += offset;
                  for (sy=subpix; sy--; dy+=scale)
                    {
                      dx1 = dx;
                      dy2 = dy*dy;
                      for (sx=subpix; sx--; dx1+=scale)
                        {
                          j = (int)(sqrt(dx1*dx1+dy2)*stepdens);
                          if (j < n)
                            {
                              if (ismasked)
                                maskarea[j] += scale2;
                              else
                                {
                                  sum[j] += scale2*pix;
                                  sumvar[j] += scale2*varpix;
                                }
                              area[j] += scale2;
                            }
                        }
                    }
                }
              else
                /* pixel not close to bin boundary */
                {
                  j = (int)(rpix*stepdens);
                  if (j < n)
                    {
                      if (ismasked)
                        maskarea[j] += 1.0;
                      else
                        {
                          sum[j] += pix;
                          sumvar[j] += varpix;
                        }
                      area[j] += 1.0;
                    }
                }
            } /* closes "if pixel might be within aperture" */

          /* increment pointers by one element */
          datat += size;
          if (errisarray)
            errort += esize;
          maskt += msize;
        }
    }


  /* correct for masked values */
  if (im->mask)
    {
      if (inflag & SEP_MASK_IGNORE)
        for (j=n; j--;)
          area[j] -= maskarea[j];
      else
        {
          for (j=n; j--;)
            {
              tmp = area[j] == maskarea[j]? 0.0: area[j]/(area[j]-maskarea[j]);
              sum[j] *= tmp;
              sumvar[j] *= tmp;
            }
        }
    }

  /* add poisson noise, only if gain > 0 */
  if (im->gain > 0.0)
    for (j=n; j--;)
      if (sum[j] > 0.0)
        sumvar[j] += sum[j] / im->gain;

  return status;
}


/* for use in flux_radius */
static double inverse(double xmax, double *y, int n, double ytarg)
{
  double step;
  int i;

  step = xmax/n;
  i = 0;

  /* increment i until y[i] is >= to ytarg */
  while (i < n && y[i] < ytarg)
    i++;

  if (i == 0)
    {
      if (ytarg <= 0. || y[0] == 0.)
        return 0.;
      return step * ytarg/y[0];
    }
  if (i == n)
    return xmax;

  /* note that y[i-1] corresponds to x=step*i. */
  return step * (i + (ytarg - y[i-1])/(y[i] - y[i-1]));
}

int sep_flux_radius(sep_image *im,
                    double x, double y, double rmax, int subpix, short inflag,
                    double *fluxtot, double *fluxfrac, int n, double *r,
                    short *flag)
{
  int status;
  int i;
  double f;
  double sumbuf[FLUX_RADIUS_BUFSIZE] = {0.};
  double sumvarbuf[FLUX_RADIUS_BUFSIZE];
  double areabuf[FLUX_RADIUS_BUFSIZE];
  double maskareabuf[FLUX_RADIUS_BUFSIZE];

  /* measure FLUX_RADIUS_BUFSIZE annuli out to rmax. */
  status = sep_sum_circann_multi(im, x, y, rmax,
                                 FLUX_RADIUS_BUFSIZE, subpix, inflag,
                                 sumbuf, sumvarbuf, areabuf, maskareabuf,
                                 flag);

  /* sum up sumbuf array */
  for (i=1; i<FLUX_RADIUS_BUFSIZE; i++)
    sumbuf[i] += sumbuf[i-1];

  /* if given, use "total flux", else, use sum within rmax. */
  f = fluxtot? *fluxtot : sumbuf[FLUX_RADIUS_BUFSIZE-1];

  /* Use inverse to get the radii corresponding to the requested flux fracs */
  for (i=0; i<n; i++)
    r[i] = inverse(rmax, sumbuf, FLUX_RADIUS_BUFSIZE, fluxfrac[i] * f);

  return status;
}

/*****************************************************************************/
/* calculate Kron radius from pixels within an ellipse. */
int sep_kron_radius(sep_image *im, double x, double y,
                    double cxx, double cyy, double cxy, double r,
                    double *kronrad, short *flag)
{
  float pix;
  double r1, v1, r2, area, rpix2, dx, dy;
  int ix, iy, xmin, xmax, ymin, ymax, status, size, msize;
  long pos;
  BYTE *datat, *maskt;
  converter convert, mconvert;

  r2 = r*r;
  r1 = v1 = 0.0;
  area = 0.0;
  *flag = 0;
  datat = maskt = NULL;
  size = msize = 0;

  /* get data converter(s) for input array(s) */
  if ((status = get_converter(im->dtype, &convert, &size)))
    return status;
  if (im->mask && (status = get_converter(im->mdtype, &mconvert, &msize)))
      return status;


  /* get extent of ellipse in x and y */
  boxextent_ellipse(x, y, cxx, cyy, cxy, r, im->w, im->h,
                    &xmin, &xmax, &ymin, &ymax, flag);

  /* loop over rows in the box */
  for (iy=ymin; iy<ymax; iy++)
    {
      /* set pointers to the start of this row */
      pos = (iy % im->h) * im->w + xmin;
      datat = MSVC_VOID_CAST im->data + pos*size;
      if (im->mask)
        maskt = MSVC_VOID_CAST im->mask + pos*msize;

      /* loop over pixels in this row */
      for (ix=xmin; ix<xmax; ix++)
        {
          dx = ix - x;
          dy = iy - y;
          rpix2 = cxx*dx*dx + cyy*dy*dy + cxy*dx*dy;
          if (rpix2 <= r2)
            {
              pix = convert(datat);
              if ((pix < -BIG) || (im->mask && mconvert(maskt) > im->maskthresh))
                {
                  *flag |= SEP_APER_HASMASKED;
                }
              else
                {
                  r1 += sqrt(rpix2)*pix;
                  v1 += pix;
                  area++;
                }
            }

          /* increment pointers by one element */
          datat += size;
          maskt += msize;
        }
    }

  if (area == 0)
    {
      *flag |= SEP_APER_ALLMASKED;
      *kronrad = 0.0;
    }
  else if (r1 <= 0.0 || v1 <= 0.0)
    {
      *flag |= SEP_APER_NONPOSITIVE;
      *kronrad = 0.0;
    }
  else
    {
      *kronrad = r1 / v1;
    }

  return RETURN_OK;
}


/* set array values within an ellipse (uc = unsigned char array) */
void sep_set_ellipse(unsigned char *arr, int w, int h,
                     double x, double y, double cxx, double cyy, double cxy,
                     double r, unsigned char val)
{
  unsigned char *arrt;
  int xmin, xmax, ymin, ymax, xi, yi;
  double r2, dx, dy, dy2;
  short flag; /* not actually used, but input to boxextent */

  flag = 0;
  r2 = r*r;

  boxextent_ellipse(x, y, cxx, cyy, cxy, r, w, h,
                    &xmin, &xmax, &ymin, &ymax, &flag);

  for (yi=ymin; yi<ymax; yi++)
    {
      arrt = arr + (yi*w + xmin);
      dy = yi - y;
      dy2 = dy*dy;
      for (xi=xmin; xi<xmax; xi++, arrt++)
        {
          dx = xi - x;
          if ((cxx*dx*dx + cyy*dy2 + cxy*dx*dy) <= r2)
            *arrt = val;
        }
    }
}

/*****************************************************************************/
/*
 * As with `sep_sum_circann_multi`, this is different enough from the other
 * aperture functions that it doesn't quite make sense to use aperture.i.
 *
 * To reproduce SExtractor:
 * - use sig = obj.hl_radius * 2/2.35.
 * - use obj.posx/posy for initial position
 *
 */

int sep_windowed(sep_image *im,
                 double x, double y, double sig, int subpix, short inflag,
                 double *xout, double *yout, int *niter, short *flag)
{
  PIXTYPE pix, varpix;
  double dx, dy, dx1, dy2, offset, scale, scale2, tmp, dxpos, dypos, weight;
  double maskarea, maskweight, maskdxpos, maskdypos;
  double r, tv, twv, sigtv, totarea, overlap, rpix2, invtwosig2;
  double wpix;
  int i, ix, iy, xmin, xmax, ymin, ymax, sx, sy, status, size, esize, msize;
  long pos;
  short errisarray, errisstd;
  BYTE *datat, *errort, *maskt;
  converter convert, econvert, mconvert;
  double r2, r_in2, r_out2;

  /* input checks */
  if (sig < 0.0)
    return ILLEGAL_APER_PARAMS;
  if (subpix < 0)
    return ILLEGAL_SUBPIX;

  /* initializations */
  size = esize = msize = 0;
  tv = sigtv = 0.0;
  overlap = totarea = maskweight = 0.0;
  datat = maskt = NULL;
  errort = im->noise;
  *flag = 0;
  varpix = 0.0;
  scale = 1.0/subpix;
  scale2 = scale*scale;
  offset = 0.5*(scale-1.0);
  invtwosig2 = 1.0/(2.0*sig*sig);
  errisarray = 0;
  errisstd = 0;

  /* Integration radius */
  r = WINPOS_NSIG*sig;

  /* calculate oversampled annulus */
  r2 = r*r;
  oversamp_ann_circle(r, &r_in2, &r_out2);

  /* get data converter(s) for input array(s) */
  if ((status = get_converter(im->dtype, &convert, &size)))
    return status;
  if (im->mask && (status = get_converter(im->mdtype, &mconvert, &msize)))
    return status;

  /* get image noise */
  if (im->noise_type != SEP_NOISE_NONE)
    {
      errisstd = (im->noise_type == SEP_NOISE_STDDEV);
      if (im->noise)
        {
          errisarray = 1;
          if ((status = get_converter(im->ndtype, &econvert, &esize)))
            return status;
        }
      else
        {
          varpix = (errisstd)?  im->noiseval * im->noiseval: im->noiseval;
        }
    }

  /* iteration loop */
  for (i=0; i<WINPOS_NITERMAX; i++)
    {

      /* get extent of box */
      boxextent(x, y, r, r, im->w, im->h,
                &xmin, &xmax, &ymin, &ymax, flag);

      /* TODO: initialize values */
      /* mx2ph
         my2ph
        esum, emxy, emx2, emy2, mx2, my2, mxy */
      tv = twv = sigtv = 0.0;
      overlap = totarea = maskarea = maskweight = 0.0;
      dxpos = dypos = 0.0;
      maskdxpos = maskdypos = 0.0;

      /* loop over rows in the box */
      for (iy=ymin; iy<ymax; iy++)
        {
          /* set pointers to the start of this row */
          pos = (iy % im->h) * im->w + xmin;
          datat = MSVC_VOID_CAST im->data + pos*size;
          if (errisarray)
            errort = MSVC_VOID_CAST im->noise + pos*esize;
          if (im->mask)
            maskt = MSVC_VOID_CAST im->mask + pos*msize;

          /* loop over pixels in this row */
          for (ix=xmin; ix<xmax; ix++)
            {
              dx = ix - x;
              dy = iy - y;
              rpix2 = dx*dx + dy*dy;
              if (rpix2 < r_out2)
                {
                  if (rpix2 > r_in2)  /* might be partially in aperture */
                    {
                      if (subpix == 0)
                        overlap = circoverlap(dx-0.5, dy-0.5, dx+0.5, dy+0.5,
                                              r);
                      else
                        {
                          dx += offset;
                          dy += offset;
                          overlap = 0.0;
                          for (sy=subpix; sy--; dy+=scale)
                            {
                              dx1 = dx;
                              dy2 = dy*dy;
                              for (sx=subpix; sx--; dx1+=scale)
                                if (dx1*dx1 + dy2 < r2)
                                  overlap += scale2;
                            }
                        }
                    }
                  else
                    /* definitely fully in aperture */
                    overlap = 1.0;

                  /* get pixel value and variance value */
                  pix = convert(datat);
                  if (errisarray)
                    {
                      varpix = econvert(errort);
                      if (errisstd)
                        varpix *= varpix;
                    }

                  /* offset of this pixel from center */
                  dx = ix - x; 
                  dy = iy - y;

                  /* weight by gaussian */
                  weight = exp(-rpix2*invtwosig2);

                  if (im->mask && (mconvert(maskt) > im->maskthresh))
                    {
                      *flag |= SEP_APER_HASMASKED;
                      maskarea += overlap;
                      maskweight += overlap * weight;
                      maskdxpos += overlap * weight * dx;
                      maskdypos += overlap * weight * dy;
                    }
                  else
                    {
                      tv += pix * overlap;
                      wpix = pix * overlap * weight;
                      twv += wpix;
                      dxpos += wpix * dx;
                      dypos += wpix * dy;
                    }

                  totarea += overlap;

                } /* closes "if pixel might be within aperture" */

              /* increment pointers by one element */
              datat += size;
              if (errisarray)
                errort += esize;
              maskt += msize;
            } /* closes loop over x */
        } /* closes loop over y */

      /* we're done looping over pixels for this iteration.
       * Our summary statistics are:
       *
       * tv : total value
       * twv : total weighted value
       * dxpos : weighted dx
       * dypos : weighted dy
       */

      /* Correct for masked values: This effectively makes it as if
       * the masked pixels had the value of the average unmasked value
       * in the aperture.
       */
      if (im->mask)
        {
          /* this option will probably not yield accurate values */
          if (inflag & SEP_MASK_IGNORE)
            totarea -= maskarea;
          else
            {
              tmp = tv/(totarea-maskarea); /* avg unmasked pixel value */
              twv += tmp * maskweight;
              dxpos += tmp * maskdxpos;
              dypos += tmp * maskdypos;
            }
        }

      /* update center */
      if (twv>0.0)
        {
          x += (dxpos /= twv) * WINPOS_FAC;
          y += (dypos /= twv) * WINPOS_FAC;
        }
      else
        break;

      /* Stop here if position does not change */
      if (dxpos*dxpos+dypos*dypos < WINPOS_STEPMIN*WINPOS_STEPMIN)
        break;

    } /* closes loop over interations */

  /* assign output results */
  *xout = x;
  *yout = y;
  *niter = i+1;

  return status;
}
