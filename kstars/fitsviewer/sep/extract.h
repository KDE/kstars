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

#define	UNKNOWN	        -1    /* flag for LUTZ */
#define	CLEAN_ZONE      10.0  /* zone (in sigma) to consider for processing */
#define CLEAN_STACKSIZE 3000  /* replaces prefs.clean_stacksize  */
                              /* (MEMORY_OBJSTACK in sextractor inputs) */
#define CLEAN_MARGIN    0  /* replaces prefs.cleanmargin which was set based */
                           /* on stuff like apertures and vignet size */
#define	MARGIN_SCALE   2.0 /* Margin / object height */ 
#define	MARGIN_OFFSET  4.0 /* Margin offset (pixels) */ 
#define	MAXDEBAREA     3   /* max. area for deblending (must be >= 1)*/
#define	MAXPICSIZE     1048576 /* max. image size in any dimension */

/* plist-related macros */
#define	PLIST(ptr, elem)	(((pbliststruct *)(ptr))->elem)
#define	PLISTEXIST(elem)	(plistexist_##elem)
#define	PLISTPIX(ptr, elem)	(*((PIXTYPE *)((ptr)+plistoff_##elem)))
#define	PLISTFLAG(ptr, elem)	(*((FLAGTYPE *)((ptr)+plistoff_##elem)))

/* Extraction status */
typedef	enum {COMPLETE, INCOMPLETE, NONOBJECT, OBJECT} pixstatus;

/* Temporary object parameters during extraction */
typedef struct structinfo
{
  LONG	pixnb;	    /* Number of pixels included */
  LONG	firstpix;   /* Pointer to first pixel of pixlist */
  LONG	lastpix;    /* Pointer to last pixel of pixlist */
  short	flag;	    /* Extraction flag */
} infostruct;

typedef	char pliststruct;      /* Dummy type for plist */

typedef struct
{
  int     nextpix;
  int     x, y;
  PIXTYPE value;
} pbliststruct;

/* array buffer struct */
typedef struct
{
  BYTE *dptr;         /* pointer to original data, can be any supported type */
  int dtype;          /* data type of original data */
  int dw, dh;         /* original data width, height */
  PIXTYPE *bptr;      /* buffer pointer (self-managed memory) */
  int bw, bh;         /* buffer width, height (bufw can be larger than w due */
                      /* to padding). */
  PIXTYPE *midline;   /* "middle" line in buffer (at index bh/2) */
  PIXTYPE *lastline;  /* last line in buffer */
  array_converter readline;  /* function to read a data line into buffer */
  int elsize;         /* size in bytes of one element in original data */ 
  int yoff;           /* line index in original data corresponding to bufptr */
} arraybuffer;


/* globals */
extern int plistexist_cdvalue, plistexist_thresh, plistexist_var;
extern int plistoff_value, plistoff_cdvalue, plistoff_thresh, plistoff_var;
extern int plistsize;

typedef struct
{
  /* thresholds */
  float	   thresh;		             /* detect threshold (ADU) */
  float	   mthresh;		             /* max. threshold (ADU) */

  /* # pixels */
  int	   fdnpix;		       	/* nb of extracted pix */
  int	   dnpix;	       		/* nb of pix above thresh  */
  int	   npix;       			/* "" in measured frame */
  int	   nzdwpix;			/* nb of zero-dweights around */
  int	   nzwpix;		       	/* nb of zero-weights inside */
  
  /* position */
  int	   xpeak, ypeak;                     /* pos of brightest pix */
  int	   xcpeak,ycpeak;                    /* pos of brightest pix */
  double   mx, my;        	             /* barycenter */
  int	   xmin,xmax,ymin,ymax,ycmin,ycmax;  /* x,y limits */

  /* shape */
  double   mx2,my2,mxy;			     /* variances and covariance */
  float	   a, b, theta, abcor;		     /* moments and angle */
  float	   cxx,cyy,cxy;			     /* ellipse parameters */
  double   errx2, erry2, errxy;      /* Uncertainties on the variances */

  /* flux */
  float	   fdflux;	       		/* integrated ext. flux */
  float	   dflux;      			/* integrated det. flux */
  float	   flux;       			/* integrated mes. flux */
  float	   fluxerr;			/* integrated variance */
  PIXTYPE  fdpeak;	       		/* peak intensity (ADU) */
  PIXTYPE  dpeak;      			/* peak intensity (ADU) */
  PIXTYPE  peak;       			/* peak intensity (ADU) */

  /* flags */
  short	   flag;			     /* extraction flags */

  /* accessing individual pixels in plist*/
  int	   firstpix;			     /* ptr to first pixel */
  int	   lastpix;			     /* ptr to last pixel */
} objstruct;

typedef struct
{
  int           nobj;	  /* number of objects in list */
  objstruct     *obj;	  /* pointer to the object array */
  int           npix;	  /* number of pixels in pixel-list */
  pliststruct   *plist;	  /* pointer to the pixel-list */
  PIXTYPE       thresh;   /* detection threshold */
} objliststruct;


int analysemthresh(int objnb, objliststruct *objlist, int minarea,
		   PIXTYPE thresh);
void preanalyse(int, objliststruct *);
void analyse(int, objliststruct *, int, double);

int  lutzalloc(int, int);
void lutzfree(void);
int  lutz(pliststruct *plistin,
	  int *objrootsubmap, int subx, int suby, int subw,
	  objstruct *objparent, objliststruct *objlist, int minarea);

void update(infostruct *, infostruct *, pliststruct *);

int  allocdeblend(int);
void freedeblend(void);
int  deblend(objliststruct *, int, objliststruct *, int, double, int);

/*int addobjshallow(objstruct *, objliststruct *);
int rmobjshallow(int, objliststruct *);
void mergeobjshallow(objstruct *, objstruct *);
*/
int addobjdeep(int, objliststruct *, objliststruct *);

int convolve(arraybuffer *buf, int y, float *conv, int convw, int convh,
             PIXTYPE *out);
int matched_filter(arraybuffer *imbuf, arraybuffer *nbuf, int y,
                   float *conv, int convw, int convh,
                   PIXTYPE *work, PIXTYPE *out, int noise_type);
