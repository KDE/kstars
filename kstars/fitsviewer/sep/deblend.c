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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sep.h"
#include "sepcore.h"
#include "extract.h"

#ifndef	RAND_MAX
#define	RAND_MAX 2147483647
#endif
#define	NSONMAX	1024  /* max. number per level */
#define NSONMAX_STR "1024" /* just for error message */
#define	NBRANCH	16    /* starting number per branch */



int belong(int, objliststruct *, int, objliststruct *);
int *createsubmap(objliststruct *, int, int *, int *, int *, int *);
int gatherup(objliststruct *, objliststruct *);

static objliststruct *objlist=NULL;
static short	     *son=NULL, *ok=NULL;

/******************************** deblend ************************************/
/*
Divide a list of isophotal detections in several parts (deblending).
NOTE: Even if the object is not deblended, the output objlist threshold is
      recomputed if a variable threshold is used.

This can return two error codes: DEBLEND_OVERFLOW or MEMORY_ALLOC_ERROR
*/
int deblend(objliststruct *objlistin, int l, objliststruct *objlistout,
	    int deblend_nthresh, double deblend_mincont, int minarea)
{
  objstruct		*obj;
  static objliststruct	debobjlist, debobjlist2;
  double		thresh, thresh0, value0;
  int			h,i,j,k,m,subx,suby,subh,subw,
                        xn,
			nbm = NBRANCH,
			status;
  int                   *submap;

  submap = NULL;
  status = RETURN_OK;
  xn = deblend_nthresh;

  /* reset global static objlist for deblending */
  memset(objlist, 0, (size_t)xn*sizeof(objliststruct));

  /* initialize local object lists */
  debobjlist.obj = debobjlist2.obj =  NULL;
  debobjlist.plist = debobjlist2.plist = NULL;
  debobjlist.nobj = debobjlist2.nobj = 0;
  debobjlist.npix = debobjlist2.npix = 0;

  /* Create the submap for the object. 
   * The submap is used in lutz(). We create it here because we may call
   * lutz multiple times below, and we only want to create it once.
   */
  submap = createsubmap(objlistin, l, &subx, &suby, &subw, &subh);
  if (!submap)
    {
      status = MEMORY_ALLOC_ERROR;
      goto exit;
    }

  /* set thresholds of object lists based on object threshold */
  thresh0 = objlistin->obj[l].thresh;
  objlistout->thresh = debobjlist2.thresh = thresh0;

  /* add input object to global deblending objlist and one local objlist */
  if ((status = addobjdeep(l, objlistin, &objlist[0])) != RETURN_OK)
    goto exit;
  if ((status = addobjdeep(l, objlistin, &debobjlist2)) != RETURN_OK)
    goto exit;

  value0 = objlist[0].obj[0].fdflux*deblend_mincont;
  ok[0] = (short)1;
  for (k=1; k<xn; k++)
    {
      /*------ Calculate threshold */
      thresh = objlistin->obj[l].fdpeak;
      debobjlist.thresh = thresh > 0.0? 
	thresh0*pow(thresh/thresh0,(double)k/xn) : thresh0;
      
      /*--------- Build tree (bottom->up) */
      if (objlist[k-1].nobj>=NSONMAX)
	{
	  status = DEBLEND_OVERFLOW;
	  goto exit;
	}
      
      for (i=0; i<objlist[k-1].nobj; i++)
	{
	  status = lutz(objlistin->plist, submap, subx, suby, subw,
			&objlist[k-1].obj[i], &debobjlist, minarea);
	  if (status != RETURN_OK)
	    goto exit;
	  
	  for (j=h=0; j<debobjlist.nobj; j++)
	    if (belong(j, &debobjlist, i, &objlist[k-1]))
	      {
		debobjlist.obj[j].thresh = debobjlist.thresh;
		if ((status = addobjdeep(j, &debobjlist, &objlist[k]))
		    != RETURN_OK)
		  goto exit;
		m = objlist[k].nobj - 1;
		if (m>=NSONMAX)
		  {
		    status = DEBLEND_OVERFLOW;
		    goto exit;
		  }
		if (h>=nbm-1)
		  if (!(son = (short *)
			realloc(son,xn*NSONMAX*(nbm+=16)*sizeof(short))))
		    {
		      status = MEMORY_ALLOC_ERROR;
		      goto exit;
		    }
		son[k-1+xn*(i+NSONMAX*(h++))] = (short)m;
		ok[k+xn*m] = (short)1;
	      }
	  son[k-1+xn*(i+NSONMAX*h)] = (short)-1;
	}
    }
  
  /*------- cut the right branches (top->down) */
  for (k = xn-2; k>=0; k--)
    {
      obj = objlist[k+1].obj;
      for (i=0; i<objlist[k].nobj; i++)
	{
	  for (m=h=0; (j=(int)son[k+xn*(i+NSONMAX*h)])!=-1; h++)
	    {
	      if (obj[j].fdflux - obj[j].thresh * obj[j].fdnpix > value0)
		m++;
	      ok[k+xn*i] &= ok[k+1+xn*j];
	    }
	  if (m>1)	
	    {
	      for (h=0; (j=(int)son[k+xn*(i+NSONMAX*h)])!=-1; h++)
		if (ok[k+1+xn*j] &&
		    obj[j].fdflux - obj[j].thresh * obj[j].fdnpix > value0)
		  {
		    objlist[k+1].obj[j].flag |= SEP_OBJ_MERGED;
		    status = addobjdeep(j, &objlist[k+1], &debobjlist2);
		    if (status != RETURN_OK)
		      goto exit;
		  }
	      ok[k+xn*i] = (short)0;
	    }
	}
    }
  
  if (ok[0])
    status = addobjdeep(0, &debobjlist2, objlistout);
  else
    status = gatherup(&debobjlist2, objlistout);
  
 exit:
  if (status == DEBLEND_OVERFLOW)
    put_errdetail("limit of " NSONMAX_STR " sub-objects reached while "
		  "deblending. Decrease number of deblending thresholds "
		  "or increase the detection threshold.");

  free(submap);
  submap = NULL;
  free(debobjlist2.obj);
  free(debobjlist2.plist);
  
  for (k=0; k<xn; k++)
    {
      free(objlist[k].obj);
      free(objlist[k].plist);
    }

  free(debobjlist.obj);
  free(debobjlist.plist);
  
  return status;
}


/******************************* allocdeblend ******************************/
/*
Allocate the memory allocated by global pointers in refine.c
*/
int allocdeblend(int deblend_nthresh)
{
  int status=RETURN_OK;
  QMALLOC(son, short,  deblend_nthresh*NSONMAX*NBRANCH, status);
  QMALLOC(ok, short,  deblend_nthresh*NSONMAX, status);
  QMALLOC(objlist, objliststruct, deblend_nthresh, status);

  return status;
 exit:
  freedeblend();
  return status;
}

/******************************* freedeblend *******************************/
/*
Free the memory allocated by global pointers in refine.c
*/
void freedeblend(void)
{
  free(son);
  son = NULL;
  free(ok);
  ok = NULL;
  free(objlist);
  objlist = NULL;
  return;
}

/********************************* gatherup **********************************/
/*
Collect faint remaining pixels and allocate them to their most probable
progenitor.
*/
int gatherup(objliststruct *objlistin, objliststruct *objlistout)
{
  char        *bmp;
  float       *amp, *p, dx,dy, drand, dist, distmin;
  objstruct   *objin = objlistin->obj, *objout, *objt;

  pliststruct *pixelin = objlistin->plist, *pixelout, *pixt,*pixt2;

  int         i,k,l, *n, iclst, npix, bmwidth,
              nobj = objlistin->nobj, xs,ys, x,y, status;

  bmp = NULL;
  amp = p = NULL;
  n = NULL;
  status = RETURN_OK;

  objlistout->thresh = objlistin->thresh;

  QMALLOC(amp, float, nobj, status);
  QMALLOC(p, float, nobj, status);
  QMALLOC(n, int, nobj, status);

  for (i=1; i<nobj; i++)
    analyse(i, objlistin, 0, 0.0);

  p[0] = 0.0;
  bmwidth = objin->xmax - (xs=objin->xmin) + 1;
  npix = bmwidth * (objin->ymax - (ys=objin->ymin) + 1);
  if (!(bmp = (char *)calloc(1, npix*sizeof(char))))
    {
      bmp = NULL;
      status = MEMORY_ALLOC_ERROR;
      goto exit;
    }
  
  for (objt = objin+(i=1); i<nobj; i++, objt++)
    {
      /*-- Now we have passed the deblending section, reset threshold */
      objt->thresh = objlistin->thresh;

      /* ------------	flag pixels which are already allocated */
      for (pixt=pixelin+objin[i].firstpix; pixt>=pixelin;
	   pixt=pixelin+PLIST(pixt,nextpix))
	bmp[(PLIST(pixt,x)-xs) + (PLIST(pixt,y)-ys)*bmwidth] = '\1';
      
      status = addobjdeep(i, objlistin, objlistout);
      if (status != RETURN_OK)
	goto exit;
      n[i] = objlistout->nobj - 1;

      dist = objt->fdnpix/(2*PI*objt->abcor*objt->a*objt->b);
      amp[i] = dist<70.0? objt->thresh * expf(dist) : 4.0*objt->fdpeak;

      /* ------------ limitate expansion ! */
      if (amp[i]>4.0*objt->fdpeak)
	amp[i] = 4.0*objt->fdpeak;
    }

  objout = objlistout->obj;		/* DO NOT MOVE !!! */

  if (!(pixelout=(pliststruct *)realloc(objlistout->plist,
					(objlistout->npix + npix)*plistsize)))
    {
      status = MEMORY_ALLOC_ERROR;
      goto exit;
    }
  
  objlistout->plist = pixelout;
  k = objlistout->npix;
  iclst = 0;				/* To avoid gcc -Wall warnings */
  for (pixt=pixelin+objin->firstpix; pixt>=pixelin;
       pixt=pixelin+PLIST(pixt,nextpix))
    {
      x = PLIST(pixt,x);
      y = PLIST(pixt,y);
      if (!bmp[(x-xs) + (y-ys)*bmwidth])
	{
	  pixt2 = pixelout + (l=(k++*plistsize));
	  memcpy(pixt2, pixt, (size_t)plistsize);
	  PLIST(pixt2, nextpix) = -1;
	  distmin = 1e+31;
	  for (objt = objin+(i=1); i<nobj; i++, objt++)
	    {
	      dx = x - objt->mx;
	      dy = y - objt->my;
	      dist=0.5*(objt->cxx*dx*dx+objt->cyy*dy*dy+objt->cxy*dx*dy)/objt->abcor;
	      p[i] = p[i-1] + (dist<70.0?amp[i]*expf(-dist) : 0.0);
	      if (dist<distmin)
		{
		  distmin = dist;
		  iclst = i;
		}
	    }			
	  if (p[nobj-1] > 1.0e-31)
	    {
	      drand = p[nobj-1]*rand()/RAND_MAX;
	      for (i=1; i<nobj && p[i]<drand; i++);
	      if (i==nobj)
		i=iclst;
	    }
	  else
	    i = iclst;
	  objout[n[i]].lastpix=PLIST(pixelout+objout[n[i]].lastpix,nextpix)=l;
	}
    }

  objlistout->npix = k;
  if (!(objlistout->plist = (pliststruct *)realloc(pixelout,
						   objlistout->npix*plistsize)))
    status = MEMORY_ALLOC_ERROR;

 exit:
  free(bmp);
  free(amp);
  free(p);
  free(n);

  return status;
}

/**************** belong (originally in manobjlist.c) ************************/
/*
 * say if an object is "included" in another. Returns 1 if the pixels of the
 * first object are included in the pixels of the second object.
 */

int belong(int corenb, objliststruct *coreobjlist,
	   int shellnb, objliststruct *shellobjlist)
{
  objstruct   *cobj = &(coreobjlist->obj[corenb]),
              *sobj = &(shellobjlist->obj[shellnb]);
  pliststruct *cpl = coreobjlist->plist, *spl = shellobjlist->plist, *pixt;

  int         xc=PLIST(cpl+cobj->firstpix,x), yc=PLIST(cpl+cobj->firstpix,y);

  for (pixt = spl+sobj->firstpix; pixt>=spl; pixt = spl+PLIST(pixt,nextpix))
    if ((PLIST(pixt,x) == xc) && (PLIST(pixt,y) == yc))
      return 1;

  return 0;
}


/******************************** createsubmap *******************************/
/*
Create pixel-index submap for deblending.
*/
int *createsubmap(objliststruct *objlistin, int no,
		  int *subx, int *suby, int *subw, int *subh)
{
  objstruct	*obj;
  pliststruct	*pixel, *pixt;
  int		i, n, xmin,ymin, w, *pix, *pt, *submap;

  obj = objlistin->obj+no;
  pixel = objlistin->plist;
  
  *subx = xmin = obj->xmin;
  *suby = ymin = obj->ymin;
  *subw = w = obj->xmax - xmin + 1;
  *subh = obj->ymax - ymin + 1;

  n = w**subh;
  if (!(submap = pix = (int *)malloc(n*sizeof(int))))
    return NULL;
  pt = pix;
  for (i=n; i--;)
    *(pt++) = -1;
  
  for (i=obj->firstpix; i!=-1; i=PLIST(pixt,nextpix))
    {
      pixt = pixel+i;
      *(pix+(PLIST(pixt,x)-xmin) + (PLIST(pixt,y)-ymin)*w) = i;
    }
  
  return submap;
}
