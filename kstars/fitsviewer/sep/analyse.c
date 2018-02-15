/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
*
* This file is part of SEP
*
* Copyright 1993-2011 Emmanuel Bertin -- IAP/CNRS/UPMC
* Copyright 2014 SEP developers
*
* SEP is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* SEP is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with SEP.  If not, see <http://www.gnu.org/licenses/>.
*
*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sep.h"
#include "sepcore.h"
#include "extract.h"

/********************************** cleanprep ********************************/
/*
 * Prepare object for cleaning, by calculating mthresh.
 * This used to be in analyse() / examineiso().
 */

int analysemthresh(int objnb, objliststruct *objlist, int minarea,
		   PIXTYPE thresh)
{
  objstruct *obj = objlist->obj+objnb;
  pliststruct *pixel = objlist->plist;
  pliststruct *pixt;
  PIXTYPE tpix;
  float     *heap,*heapt,*heapj,*heapk, swap;
  int       j, k, h, status;

  status = RETURN_OK;
  heap = heapt = heapj = heapk = NULL;
  h = minarea;

  if (obj->fdnpix < minarea)
    {
      obj->mthresh = 0.0;
      return status;
    }

  QMALLOC(heap, float, minarea, status);
  heapt = heap;

  /*-- Find the minareath pixel in decreasing intensity for CLEANing */
  for (pixt=pixel+obj->firstpix; pixt>=pixel; pixt=pixel+PLIST(pixt,nextpix))
    {
      /* amount pixel is above threshold */
      tpix = PLISTPIX(pixt, cdvalue) - (PLISTEXIST(thresh)?
					PLISTPIX(pixt, thresh):thresh);
      if (h>0)
        *(heapt++) = (float)tpix;
      else if (h)
        {
	  if ((float)tpix>*heap)
	    {
	      *heap = (float)tpix;
	      for (j=0; (k=(j+1)<<1)<=minarea; j=k)
		{
		  heapk = heap+k;
		  heapj = heap+j;
		  if (k != minarea && *(heapk-1) > *heapk)
		    {
		      heapk++;
		      k++;
		    }
		  if (*heapj <= *(--heapk))
		    break;
		  swap = *heapk;
		  *heapk = *heapj;
		  *heapj = swap;
		}
	    }
        }
      else
        fqmedian(heap, minarea);
      h--;
    }

  obj->mthresh = *heap;

 exit:
  free(heap);
  return status;
}

/************************* preanalyse **************************************/

void  preanalyse(int no, objliststruct *objlist)
{
  objstruct	*obj = &objlist->obj[no];
  pliststruct	*pixel = objlist->plist, *pixt;
  PIXTYPE	peak, cpeak, val, cval;
  double	rv;
  int		x, y, xmin,xmax, ymin,ymax, fdnpix;
  int           xpeak, ypeak, xcpeak, ycpeak;
  
  /*-----  initialize stacks and bounds */
  fdnpix = 0;
  rv = 0.0;
  peak = cpeak = -BIG;
  ymin = xmin = 2*MAXPICSIZE;    /* to be really sure!! */
  ymax = xmax = 0;
  xpeak = ypeak = xcpeak = ycpeak = 0; /* avoid -Wall warnings */

  /*-----  integrate results */
  for (pixt=pixel+obj->firstpix; pixt>=pixel; pixt=pixel+PLIST(pixt,nextpix))
    {
      x = PLIST(pixt, x);
      y = PLIST(pixt, y);
      val = PLISTPIX(pixt, value);
      cval = PLISTPIX(pixt, cdvalue);
      if (peak < val)
	{
	  peak = val;
	  xpeak = x;
	  ypeak = y;
	}
      if (cpeak < cval)
	{
	  cpeak = cval;
	  xcpeak = x;
	  ycpeak = y;
	}
      rv += cval;
      if (xmin > x)
	xmin = x;
      if (xmax < x)
	xmax = x;
      if (ymin > y)
	ymin = y;
      if (ymax < y)
	ymax = y;
      fdnpix++;
    }    
  
  obj->fdnpix = (LONG)fdnpix;
  obj->fdflux = (float)rv;
  obj->fdpeak = cpeak;
  obj->dpeak = peak;
  obj->xpeak = xpeak;
  obj->ypeak = ypeak;
  obj->xcpeak = xcpeak;
  obj->ycpeak = ycpeak;
  obj->xmin = xmin;
  obj->xmax = xmax;
  obj->ymin = ymin;
  obj->ymax = ymax;

  return;
}

/******************************** analyse *********************************/
/*
  If robust = 1, you must have run previously with robust=0
*/

void  analyse(int no, objliststruct *objlist, int robust, double gain)
{
  objstruct	*obj = &objlist->obj[no];
  pliststruct	*pixel = objlist->plist, *pixt;
  PIXTYPE	peak, val, cval;
  double	thresh,thresh2, t1t2,darea,
                mx,my, mx2,my2,mxy, rv, rv2, tv,
		xm,ym, xm2,ym2,xym,
                temp,temp2, theta,pmx2,pmy2,
                errx2, erry2, errxy, cvar, cvarsum;
  int		x, y, xmin, ymin, area2, dnpix;

  preanalyse(no, objlist);
  
  dnpix = 0;
  mx = my = tv = 0.0;
  mx2 = my2 = mxy = 0.0;
  cvarsum = errx2 = erry2 = errxy = 0.0;
  thresh = obj->thresh;
  peak = obj->dpeak;
  rv = obj->fdflux;
  rv2 = rv * rv;
  thresh2 = (thresh + peak)/2.0;
  area2 = 0;
  
  xmin = obj->xmin;
  ymin = obj->ymin;

  for (pixt=pixel+obj->firstpix; pixt>=pixel; pixt=pixel+PLIST(pixt,nextpix))
    {
      x = PLIST(pixt,x)-xmin;  /* avoid roundoff errors on big images */
      y = PLIST(pixt,y)-ymin;  /* avoid roundoff errors on big images */
      cval = PLISTPIX(pixt, cdvalue);
      tv += (val = PLISTPIX(pixt, value));
      if (val>thresh)
	dnpix++;
      if (val > thresh2)
	area2++;
      mx += cval * x;
      my += cval * y;
      mx2 += cval * x*x;
      my2 += cval * y*y;
      mxy += cval * x*y;

    }

  /* compute object's properties */
  xm = mx / rv;    /* mean x */
  ym = my / rv;    /* mean y */


  /* In case of blending, use previous barycenters */
  if ((robust) && (obj->flag & SEP_OBJ_MERGED))
    {
      double xn, yn;
	  
      xn = obj->mx-xmin;
      yn = obj->my-ymin;
      xm2 = mx2 / rv + xn*xn - 2*xm*xn;
      ym2 = my2 / rv + yn*yn - 2*ym*yn;
      xym = mxy / rv + xn*yn - xm*yn - xn*ym;
      xm = xn;
      ym = yn;
    }
  else
    {
      xm2 = mx2 / rv - xm * xm;	 /* variance of x */
      ym2 = my2 / rv - ym * ym;	 /* variance of y */
      xym = mxy / rv - xm * ym;	 /* covariance */

    }

  /* Calculate the errors on the variances */
  for (pixt=pixel+obj->firstpix; pixt>=pixel; pixt=pixel+PLIST(pixt,nextpix))
    {
      x = PLIST(pixt,x)-xmin;  /* avoid roundoff errors on big images */
      y = PLIST(pixt,y)-ymin;  /* avoid roundoff errors on big images */

      cvar = PLISTEXIST(var)? PLISTPIX(pixt, var): 0.0;
      if (gain > 0.0) {  /* add poisson noise if given */
        cval = PLISTPIX(pixt, cdvalue);
        if (cval > 0.0) cvar += cval / gain;
      }

      /* Note that this works for both blended and non-blended cases
       * because xm is set to xn above for the blended case. */
      cvarsum += cvar;
      errx2 += cvar * (x - xm) * (x - xm);
      erry2 += cvar * (y - ym) * (y - ym);
      errxy += cvar * (x - xm) * (y - ym);
    }
  errx2 /= rv2;
  erry2 /= rv2;
  errxy /= rv2;

  /* Handle fully correlated x/y (which cause a singularity...) */
  if ((temp2=xm2*ym2-xym*xym)<0.00694)
    {
      xm2 += 0.0833333;
      ym2 += 0.0833333;
      temp2 = xm2*ym2-xym*xym;
      obj->flag |= SEP_OBJ_SINGU;

      /* handle it for the error parameters */
      cvarsum *= 0.08333/rv2;
      if (errx2*erry2 - errxy * errxy < cvarsum * cvarsum) {
        errx2 += cvarsum;
        erry2 += cvarsum;
      }
    }

  if ((fabs(temp=xm2-ym2)) > 0.0)
    theta = atan2(2.0 * xym, temp) / 2.0;
  else
    theta = PI/4.0;

  temp = sqrt(0.25*temp*temp+xym*xym);
  pmy2 = pmx2 = 0.5*(xm2+ym2);
  pmx2+=temp;
  pmy2-=temp;
  
  obj->dnpix = (LONG)dnpix;
  obj->dflux = tv;
  obj->mx = xm+xmin;	/* add back xmin */
  obj->my = ym+ymin;	/* add back ymin */
  obj->mx2 = xm2;
  obj->errx2 = errx2;
  obj->my2 = ym2;
  obj->erry2 = erry2;
  obj->mxy = xym;
  obj->errxy = errxy;
  obj->a = (float)sqrt(pmx2);
  obj->b = (float)sqrt(pmy2);
  obj->theta = theta;
  
  obj->cxx = (float)(ym2/temp2);
  obj->cyy = (float)(xm2/temp2);
  obj->cxy = (float)(-2*xym/temp2);
  
  darea = (double)area2 - dnpix;
  t1t2 = thresh/thresh2;

  /* debugging */
  /*if (t1t2>0.0 && !PLISTEXIST(thresh)) */  /* was: prefs.dweight_flag */
  if (t1t2 > 0.0)
    {
      obj->abcor = (darea<0.0?darea:-1.0)/(2*PI*log(t1t2<1.0?t1t2:0.99)
					   *obj->a*obj->b);
      if (obj->abcor>1.0)
	obj->abcor = 1.0;
    }
  else
    {
      obj->abcor = 1.0;
    }

  return;

}
