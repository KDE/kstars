/* Driver for any Apogee USB Alta camera.
 * low level USB code from http://www.randomfactory.com

    Copyright (C) 2007 Elwood Downey (ecdowney@clearskyinstitute.com)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <dirent.h>
#include <math.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <fitsio.h>

#include "indidevapi.h"
#include "eventloop.h"
#include "base64.h"
#include "zlib.h"

#include "apogee/ApnGlue.h"

/* operational info */
#define MYDEV "Apogee CCD"			/* Device name we call ourselves */

/* MaxValues parameter */
typedef enum {	/* N.B. order must match array below */
    EXP_MV, ROIW_MV, ROIH_MV, OSW_MV, OSH_MV, BINW_MV, BINH_MV,
    SHUTTER_MV, MINTEMP_MV, N_MV
} MaxValuesIndex;
static INumber  maxvalues_elements[N_MV] = {
    {"ExpTime",  "Exposure time, secs",       "%8.2f"},
    {"ROIW",     "Imaging width, pixels",     "%4.0f"},
    {"ROIH",     "Imaging height, pixels",    "%4.0f"},
    {"OSW",      "Overscan width, pixels",    "%4.0f"},
    {"OSH",      "Overscan height, pixels",   "%4.0f"},
    {"BinW",     "Horizontal binning factor", "%4.0f"},
    {"BinH",     "Vertical binnng factor",    "%4.0f"},
    {"Shutter",  "1 if have shutter, else 0", "%2.0f"},
    {"MinTemp",  "Min cooler temp, C",        "%5.1f"},
};
static INumberVectorProperty maxvalues = {MYDEV, "MaxValues",
    "Maximum camera settings",
    "", IP_RO, 0, IPS_IDLE, maxvalues_elements, NARRAY(maxvalues_elements)};

/* ExpValues parameter */
typedef enum {	/* N.B. order must match array below */
    EXP_EV, ROIW_EV, ROIH_EV, OSW_EV, OSH_EV, BINW_EV, BINH_EV,
    ROIX_EV, ROIY_EV, SHUTTER_EV, N_EV
} ExpValuesIndex;
static INumber  expvalues_elements[N_EV] = {
    {"ExpTime",  "Exposure time, secs",       "%8.2f", 0, 0, 0,   1.0},
    {"ROIW",     "Imaging width, pixels",     "%4.0f", 0, 0, 0, 100.0},
    {"ROIH",     "Imaging height, pixels",    "%4.0f", 0, 0, 0, 100.0},
    {"OSW",      "Overscan width, pixels",    "%4.0f", 0, 0, 0,   0.0},
    {"OSH",      "Overscan height, pixels",   "%4.0f", 0, 0, 0,   0.0},
    {"BinW",     "Horizontal binning factor", "%4.0f", 0, 0, 0,   1.0},
    {"BinH",     "Vertical binnng factor",    "%4.0f", 0, 0, 0,   1.0},
    {"ROIX",     "Imaging start x, pixels",   "%4.0f", 0, 0, 0, 100.0},
    {"ROIY",     "Imaging start y, pixels",   "%4.0f", 0, 0, 0, 100.0},
    {"Shutter",  "1 to open shutter, else 0", "%2.0f", 0, 0, 0,   1.0},
};
static INumberVectorProperty expvalues = {MYDEV, "ExpValues",
    "Exposure settings",
    "", IP_WO, 0, IPS_IDLE, expvalues_elements, NARRAY(expvalues_elements)};



/* ExpGo parameter */
typedef enum {	/* N.B. order must match array below */
    ON_EG, N_EG
} ExpGoIndex;
static ISwitch expgo_elements[N_EG] = {
    {"Go",        "Start exposure",    ISS_OFF}
};
static ISwitchVectorProperty expgo = {MYDEV, "ExpGo",
    "Control exposure",
    "", IP_RW, ISR_ATMOST1, 0., IPS_IDLE,expgo_elements,NARRAY(expgo_elements)};


/* Pixels BLOB parameter */
typedef enum {	/* N.B. order must match array below */
    IMG_B, N_B
} PixelsIndex;
static IBLOB pixels_elements[N_B] = {
    {"Img", "Image", ".fits"}
};
static IBLOBVectorProperty pixels = {MYDEV, "Pixels",
    "Image data",
    "", IP_RO, 0, IPS_IDLE, pixels_elements, NARRAY(pixels_elements)};



/* SetTemp parameter */
typedef enum {	/* N.B. order must match array below */
    T_STEMP, N_STEMP
} SetTempIndex;
static INumber  settemp_elements[N_STEMP] = {
    {"TEMPERATURE",  "Target temp, C (0 off)",    "%6.1f", 0, 0, 0,   -20.0},
};
static INumberVectorProperty settemp = {MYDEV, "CCD_TEMPERATURE_REQUEST",
    "Set target cooler temperature",
    "", IP_WO, 0, IPS_IDLE, settemp_elements, NARRAY(settemp_elements)};


/* TempNow parameter */
typedef enum {	/* N.B. order must match array below */
    T_TN, N_TN
} TempNowIndex;
static INumber  tempnow_elements[N_TN] = {
    {"TEMPERATURE",  "Cooler temp, C",         "%6.1f", 0, 0, 0,   0.0},
};
static INumberVectorProperty tempnow = {MYDEV, "CCD_TEMPERATURE",
    "Current cooler temperature",
    "", IP_RO, 0, IPS_IDLE, tempnow_elements, NARRAY(tempnow_elements)};




/* FanSpeed parameter */
typedef enum {	/* N.B. order must match array below */
    OFF_FS, SLOW_FS, MED_FS, FAST_FS, N_FS
} FanSpeedIndex;
static ISwitch fanspeed_elements[N_FS] = {
    /* N.B. exactly one must be ISS_ON here to serve as our default */
    {"Off",        "Fans off",     ISS_OFF},
    {"Slow",       "Fans slow",    ISS_ON},
    {"Med",        "Fans medium",  ISS_OFF},
    {"Fast",       "Fans fast",    ISS_OFF},
};
static ISwitchVectorProperty fanspeed = {MYDEV, "FanSpeed",
    "Set fans speed",
    "", IP_RW, ISR_1OFMANY, 0., IPS_IDLE, fanspeed_elements,
    						NARRAY(fanspeed_elements)};


#define	MAXEXPERR	10		/* max err in exp time we allow, secs */
#define	COOLTM		5000		/* ms between reading cooler */
#define	FHDRSZ		(2*FITS_HROWS*FITS_HCOLS)   /* bytes in FITS header */
#define	OPENDT		5		/* open retry delay, secs */

static int inited;			/* set after done with 1-time stuff */
static int impixw, impixh;		/* image size, final binned pixels */
static int expTID;			/* exposure callback timer id, if any */

/* info when exposure started */
static struct timeval exp0;		/* when exp started */
static double ra0, dec0, alt0, az0, am0;
static double winds0, windd0, hum0, extt0, mirrort0;

/* we premanently allocate an image buffer that is surely always large enough.
 * we do this for the following reasons:
 *   1. there is no sure means to limit how much GetImageData() will read
 *   2. this insures lack of memory at runtime will never be a cause for not
 *      being able to read
 */
static char imbuf[5000*5000*2];

static void getStartConditions(void);
static void expTO (void *vp);
static void coolerTO (void *vp);
static void setHeader (char *fits);
static void sendFITS (char *fits, int nfits);
static void fixFITSpix (char *fits, int nfits);
static void camconnect(void);

/* send client definitions of all properties */
void
ISGetProperties (char const *dev)
{

        if (dev && strcmp (MYDEV, dev))
	    return;

	if (!inited) {
	    int roiw, roih, osw, osh, binw, binh, shutter;
	    double exptime, mintemp;
	    char whynot[1024];

	    /* connect to the camera, wait if not yet ready */
	    camconnect();

	    /* get hardware max values */
	    ApnGlueGetMaxValues (&exptime, &roiw, &roih, &osw, &osh, &binw,
						&binh, &shutter, &mintemp);
	    maxvalues.np[EXP_MV].value = exptime;
	    maxvalues.np[ROIW_MV].value = roiw;
	    maxvalues.np[ROIH_MV].value = roih;
	    maxvalues.np[OSW_MV].value = osw;
	    maxvalues.np[OSH_MV].value = osh;
	    maxvalues.np[BINW_MV].value = binw;
	    maxvalues.np[BINH_MV].value = binh;
	    maxvalues.np[SHUTTER_MV].value = shutter;
	    maxvalues.np[MINTEMP_MV].value = mintemp;

	    /* use max values to set up a default geometry */
	    expvalues.np[EXP_EV].value = 1.0;
	    expvalues.np[ROIW_EV].value = roiw;
	    expvalues.np[ROIH_EV].value = roih;
	    expvalues.np[OSW_EV].value = 0;
	    expvalues.np[OSH_EV].value = 0;
	    expvalues.np[BINW_EV].value = 1;
	    expvalues.np[BINH_EV].value = 1;
	    expvalues.np[ROIX_EV].value = 0;
	    expvalues.np[ROIY_EV].value = 0;
	    expvalues.np[SHUTTER_EV].value = 1;

	    if (ApnGlueSetExpGeom (roiw, roih, 0, 0, 1, 1, 0, 0, &impixw,
							&impixh, whynot) < 0) {
		fprintf (stderr, "Can't even set up %dx%d image geo: %s\n",
							    roiw, roih, whynot);
		exit(1);
	    }

	    /* start cooler to our settemp default */
	    ApnGlueSetTemp (settemp.np[T_STEMP].value);

	    /* init and start cooler reading timer */
	    coolerTO(NULL);

	    /* init fans to our fanspeed switch default */
	    ApnGlueSetFan (IUFindOnSwitch(&fanspeed) - fanspeed_elements);

	    inited = 1;
	}

	IDDefNumber (&maxvalues, NULL);
	IDDefNumber (&expvalues, NULL);
	IDDefSwitch (&expgo, NULL);
	IDDefBLOB (&pixels, NULL);
	IDDefNumber (&settemp, NULL);
	IDDefNumber (&tempnow, NULL);
	IDDefSwitch (&fanspeed, NULL);
}

void
ISNewNumber (const char *dev, const char *name, double *doubles, char *names[],
int n)
{
/* FIXME */
#if 0
        if (!IUCrackNumber (&expvalues, dev, name, doubles, names, n)) {
	    int roiw, roih, osw, osh, binw, binh, roix, roiy;
	    char whynot[1024];

	    roiw = expvalues.np[ROIW_EV].value;
	    roih = expvalues.np[ROIH_EV].value;
	    osw  = expvalues.np[OSW_EV].value;
	    osh  = expvalues.np[OSH_EV].value;
	    binw = expvalues.np[BINW_EV].value;
	    binh = expvalues.np[BINH_EV].value;
	    roix = expvalues.np[ROIX_EV].value;
	    roiy = expvalues.np[ROIY_EV].value;

	    if (ApnGlueSetExpGeom (roiw, roih, osw, osh, binw, binh, roix, roiy,
						&impixw, &impixh, whynot) < 0) {
		expvalues.s = IPS_ALERT;
		IDSetNumber (&expvalues, "Bad values: %s", whynot);
	    } else if (impixw*impixh*2 + FHDRSZ > sizeof(imbuf)) {
		expvalues.s = IPS_ALERT;
		IDSetNumber (&expvalues, "No memory for %d x %d",impixw,impixh);
	    } else {
		expvalues.s = IPS_OK;
		IDSetNumber (&expvalues, "New values accepted");
	    }
	} else if (!IUCrackNumber (&settemp, dev, name, doubles, names, n)) {

	    double newt = settemp.np[T_STEMP].value;

	    ApnGlueSetTemp (newt);
	    /* let coolerTO loop update tempnow */

	    settemp.s = IPS_OK;
	    IDSetNumber (&settemp, "Set cooler target to %.1f", newt);
	}

#endif
}

void
ISNewText (const char *dev, const char *name, char *texts[], char *names[],
int n)
{
}

void
ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[],
int n)
{
	ISwitch *sp;

	/* ours with at least one switch else ignore */
	if (strcmp (dev, MYDEV) || n < 1)
	    return;


	if (!strcmp (name, expgo.name)) {

	    sp = IUFindSwitch (&expgo, names[0]);
	    if (sp) {
		int wanton = (states[0] == ISS_ON);

		if (wanton) {
		    if (expgo.s == IPS_BUSY) {
			/* can not start new exp if one in progress */
			IDSetSwitch (&expgo, "Exposure already in progress");
		    } else {
			/* start new exposure with last ExpValues settings.
			 * ExpGo goes busy. set timer to read when done
			 */
			double expsec = expvalues.np[EXP_EV].value;
			int expms = (int)ceil(expsec*1000);
			int wantshutter = (int)expvalues.np[SHUTTER_EV].value;
			if (ApnGlueStartExp (&expsec, wantshutter) < 0) {
			    expgo.sp[ON_EG].s = ISS_OFF;
			    expgo.s = IPS_ALERT;
			    IDSetSwitch (&expgo, "Error starting exposure");
			    sleep (5);
			    exit(1);
			}
			getStartConditions();
			expvalues.np[EXP_EV].value = expsec;
			expTID = IEAddTimer (expms, expTO, NULL);
			expgo.sp[ON_EG].s = ISS_ON;
			expgo.s = IPS_BUSY;
			IDSetSwitch (&expgo,
				    "Starting %g sec exp, %d x %d, shutter %s",
				    expsec, impixw, impixh,
				    wantshutter ? "open" : "closed");
		    }
		} else {
		    if (expgo.s == IPS_BUSY) {
			/* abort current exposure */
			if (expTID) {
			    IERmTimer (expTID);
			    expTID = 0;
			} else
			    fprintf (stderr, "Hmm, BUSY but no expTID\n");
			ApnGlueExpAbort();
			expgo.sp[ON_EG].s = ISS_OFF;
			expgo.s = IPS_IDLE;
			IDSetSwitch (&expgo, "Exposure aborted");
		    } /* else nothing to do */
		}
	    }
	} else if (!strcmp (name, fanspeed.name)) {
	    int i;

	    for (i = 0; i < n; i++) {
		ISwitch *sp = IUFindSwitch(&fanspeed, names[i]);
		if (sp && states[i] == ISS_ON) {
		    char *smsg = NULL;
		    int fs = 0;

		    /* see which switch */
		    fs = sp - fanspeed_elements;
		    switch (fs) {
		    case 0: smsg = "Fans shut off"; break;
		    case 1: smsg = "Fans speed set to slow"; break;
		    case 2: smsg = "Fans speed set to medium"; break;
		    case 3: smsg = "Fans speed set to fast"; break;
		    }

		    /* install if reasonable */
		    if (smsg) {
			ApnGlueSetFan (fs);
			IUResetSwitches (&fanspeed);
			fanspeed.sp[fs].s = ISS_ON;
			fanspeed.s = IPS_OK;
			IDSetSwitch (&fanspeed, smsg);
		    }

		    break;
		}
	    }
	}

}

void
ISNewBLOB (const char *dev, const char *name, int sizes[],
    int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
}

/* indiserver is sending us a message from a snooped device */
void
ISSnoopDevice (XMLEle *root)
{
}

/* save conditions at start of exposure */
static void
getStartConditions()
{

	gettimeofday (&exp0, NULL);

}

/* called when exposure is expected to be complete
 * doesn't have to be timed perfectly.
 */
static void
expTO (void *vp)
{

/* FIXME */

#if 0
	int npix = impixw*impixh;
	int nfits = npix*2 + FHDRSZ;
	char whynot[1024];
	char *fits;
	int zero = 0;
	int i;

	/* record we went off */
	expTID = 0;

	/* assert we are doing an exposure */
	if (expgo.s != IPS_BUSY) {
	    fprintf (stderr, "Hmm, expTO but not exposing\n");
	    return;
	}

	/* wait for exp complete, to a point */
	for (i = 0; i < MAXEXPERR && !ApnGlueExpDone(); i++)
	    IEDeferLoop(200, &zero);
	if (i == MAXEXPERR) {
	    /* something's wrong */
	    ApnGlueExpAbort();
	    expgo.s = IPS_ALERT;
	    expgo.sp[ON_EG].s = ISS_OFF;
	    IDSetSwitch (&expgo, "Exposure never completed");
	    return;
	}

	/* read pixels as a FITS file */
	IDSetSwitch (&expgo, "Reading %d pixels", npix);
	fits = imbuf;
	if (ApnGlueReadPixels (fits+FHDRSZ, npix*2, whynot) < 0) {
	    /* can't get pixels */
	    ApnGlueExpAbort();
	    expgo.s = IPS_ALERT;
	    expgo.sp[ON_EG].s = ISS_OFF;
	    IDSetSwitch (&expgo, "Error reading pixels: %s", whynot);
	} else {
	    /* ok */
	    setHeader (fits);
	    fixFITSpix (fits, nfits);
	    sendFITS (fits, nfits);
	    expgo.s = IPS_OK;
	    expgo.sp[ON_EG].s = ISS_OFF;
	    IDSetSwitch (&expgo, "Exposure complete");
	}

#endif

}

/* hack together a FITS header for the current image
 * we use up exactly FHDRSZ bytes.
 */
static void
setHeader (char *fits)
{

/* FIXME */

#if 0
	double expt = expvalues.np[EXP_EV].value;
	double tempt = tempnow.np[T_TN].value;
	int binw = expvalues.np[BINW_EV].value;
	int binh = expvalues.np[BINH_EV].value;
	char *shtr = expvalues.np[SHUTTER_EV].value ? "'OPEN    '":"'CLOSED  '";
	double jd = 2440587.5 + exp0.tv_sec/3600./24.;
	char *sensor, *camera;
	char *fits0 = fits;
	char buf[1024];
	struct tm *tmp;
	ImRegion imr;
	ImStats ims;

	/* compute some stats over the whole image */
	imr.im = (CamPix *) (fits0 + FHDRSZ);
	imr.iw = imr.rw = impixw;
	imr.ih = imr.rh = impixh;
	imr.rx = imr.ry = 0;
	regionStats (&imr, &ims);

	ApnGlueGetName (&sensor, &camera);

	fits += sprintf (fits, "SIMPLE  = %20s%-50s", "T", "");
	fits += sprintf (fits, "BITPIX  = %20d%-50s", 16, " / bit/pix");
	fits += sprintf (fits, "NAXIS   = %20d%-50s", 2, " / n image axes");
	fits += sprintf (fits, "NAXIS1  = %20d%-50s", impixw, " / columns");
	fits += sprintf (fits, "NAXIS2  = %20d%-50s", impixh, " / rows");
	fits += sprintf (fits, "BSCALE  = %20d%-50s", 1, " / v=p*BSCALE+BZERO");
	fits += sprintf (fits, "BZERO   = %20d%-50s", 32768, " / v=p*BSCALE+BZERO");
	fits += sprintf (fits, "EXPTIME = %20.6f%-50s", expt, " / seconds");
	fits += sprintf (fits, "INSTRUME= '%-18s'%-50s",camera," / instrument");
	fits += sprintf (fits, "DETECTOR= '%-18s'%-50s",sensor," / detector");
	fits += sprintf (fits, "CCDTEMP = %20.2f%-50s", tempt, " / deg C");
	fits += sprintf (fits, "CCDXBIN = %20d%-50s", binw," / column binning");
	fits += sprintf (fits, "CCDYBIN = %20d%-50s", binh, " / row binning");
	fits += sprintf (fits, "SHUTTER = %-20s%-50s", shtr," / shutter state");
	fits += sprintf (fits, "MEAN    = %20.3f%-50s", ims.mean, " / mean");
	fits += sprintf (fits, "MEDIAN  = %20d%-50s", ims.median," / median");
	fits += sprintf (fits, "STDEV   = %20.3f%-50s", ims.std," / standard deviation");
	fits += sprintf (fits, "MIN     = %20d%-50s", ims.min," / min pixel");
	fits += sprintf (fits, "MAX     = %20d%-50s", ims.max," / max pixel");
	fits += sprintf (fits, "MAXATX  = %20d%-50s", ims.maxatx," / col of max pixel");
	fits += sprintf (fits, "MAXATY  = %20d%-50s", ims.maxaty," / row of max pixel");


	tmp = gmtime (&exp0.tv_sec);
	fits += sprintf (fits, "TIMESYS = %-20s%-50s", "'UTC     '", " / time zone");
	fits += sprintf (fits, "JD      = %20.5f%-50s", jd, " / JD at start");
	sprintf (buf, "'%4d:%02d:%02d'", tmp->tm_year+1900, tmp->tm_mon+1,
							    tmp->tm_mday);
	fits += sprintf (fits, "DATE-OBS= %-20s%-50s", buf, " / Date at start");
	sprintf (buf, "'%02d:%02d:%06.3f'", tmp->tm_hour, tmp->tm_min,
					tmp->tm_sec + exp0.tv_usec/1e6);
	fits += sprintf (fits, "TIME-OBS= %-20s%-50s", buf, " / Time at start");

	/* some Telescope info, if sensible */
	if (ra0 || dec0) {
	    fs_sexa (buf, ra0, 4, 36000);
	    fits += sprintf(fits,"RA2K    = %-20s%-50s",buf," / RA J2K H:M:S");

	    fs_sexa (buf, dec0, 4, 36000);
	    fits += sprintf(fits,"DEC2K   = %-20s%-50s",buf," / Dec J2K D:M:S");

	    fs_sexa (buf, alt0, 4, 3600);
	    fits += sprintf(fits,"ALT     = %-20s%-50s",buf," / Alt D:M:S");

	    fs_sexa (buf, az0, 4, 3600);
	    fits += sprintf(fits,"AZ      = %-20s%-50s",buf," / Azimuth D:M:S");

	    fits += sprintf(fits,"AIRMASS = %20.3f%-50s", am0, " / Airmass");
	}
	ra0 = dec0 = 0;			/* mark stale for next time */

	/* some env info, if sensible */
	if (hum0 || windd0) {
	    fits += sprintf (fits, "HUMIDITY= %20.3f%-50s", hum0,
					    " / exterior humidity, percent");
	    fits += sprintf (fits, "AIRTEMP = %20.3f%-50s", extt0,
					    " / exterior temp, deg C");
	    fits += sprintf (fits, "MIRRTEMP= %20.3f%-50s", mirrort0,
					    " / mirror temp, deg C");
	    fits += sprintf (fits, "WINDSPD = %20.3f%-50s", winds0,
					    " / wind speed, kph");
	    fits += sprintf (fits, "WINDDIR = %20.3f%-50s", windd0,
					    " / wind dir, degs E of N");
	}
	hum0 = windd0 = 0;		/* mark stale for next time */

	/* pad with blank lines to next-to-last then carefully add END */
	while (fits-fits0 < FHDRSZ-80)
	    fits += sprintf (fits, "%80s", "");
	fits += sprintf (fits, "%-79s", "END");

	*fits = ' ';

#endif
}

/* send the given fits file out the Pixels BLOB
 */
static void
sendFITS (char *fits, int nfits)
{
	pixels.bp[IMG_B].blob = fits;
	pixels.bp[IMG_B].bloblen = nfits;
	pixels.bp[IMG_B].size = nfits;
	pixels.s = IPS_OK;
	IDSetBLOB (&pixels, NULL);
}

/* convert the camera array into proper FITS format: big-endian signed 2's comp.
 * TODO: need to byte swap if host is big-endian?
 */
static void
fixFITSpix (char *fits, int nfits)
{

/* FIXME */

#if 0
	typedef unsigned char *bp;
	bp p, pend;	/* avoid sign ext */

	/* camera is l-e unsigned 0..64k, convert to b-e signed -32k..+32k*/
	for (p = (bp)fits+FHDRSZ, pend = (bp)fits+nfits; p < pend; p += 2) {
	    /* int f = 65535.0*(p-(bp)fits)/nfits + 32768; fake */
	    int f = ((p[1]<<8) | p[0]) - 32768;
	    p[0] = (char)(f>>8);
	    p[1] = (char)(f);
	}

#endif
}

/* timer to read the cooler, repeats forever */
static void
coolerTO (void *vp)
{
	static int lasts = 9999;
	double cnow;
	int status;
	char *msg = NULL;

	status = ApnGlueGetTemp(&cnow);

	switch (status) {
	case 0:
	    tempnow.s = IPS_IDLE;
	    if (status != lasts)
		msg = "Cooler is now off";
	    break;

	case 1:
	    tempnow.s = IPS_BUSY;
	    if (status != lasts)
		msg = "Cooler is ramping to target";
	    break;

	case 2:
	    tempnow.s = IPS_OK;
	    if (status != lasts)
		msg = "Cooler is on target";
	    break;
	}

	tempnow.np[T_TN].value = cnow;
	IDSetNumber (&tempnow, msg);

	lasts = status;

	IEAddTimer (COOLTM, coolerTO, NULL);
}

/* wait forever trying to open camera
 */
static void
camconnect()
{
	while (ApnGlueOpen() < 0) {
	    fprintf (stderr, "Can not open camera: power ok? suid root?\n");
	    sleep (OPENDT);
	}
}
