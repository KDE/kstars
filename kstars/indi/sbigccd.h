#if 0
    INDI driver for SBIG CCD
    Copyright (C) 2005 Chris Curran (ccurran AT planetcurran DOT com)

    Based on Apogee PPI driver by Jasem Mutlaq (mutlaqja AT ikarustech DOT com)

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

#endif

#ifndef SBIGCCD_H
#define SBIGCCD_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

#include "fitsrw.h"
#include "indidevapi.h"
#include "eventloop.h"
#include "indicom.h"

#define mydev           "SBIG CCD"

#define COMM_GROUP	"Communication"
#define EXPOSE_GROUP	"Expose"
#define IMAGE_GROUP	"Image Settings"

#define POLLMS		1000		/* Polling time (ms) */
#define TEMP_THRESHOLD  .25		/* Differential temperature threshold (C)*/

#define MAX_PIXELS	4096
#define MAXHBIN 	8
#define MAXVBIN 	64
#define MIN_CCD_TEMP	-60
#define MAX_CCD_TEMP	40

#define getBigEndian(p) ( ((p & 0xff) << 8) | (p  >> 8))

class SBIGCam {
  
  public:
    
    SBIGCam();
    ~SBIGCam();
    
    /* INDI Functions that must be called from indidrivermain */
    void ISGetProperties (const char *dev);
    void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
    void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
    void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
    
  private:
    
    /* Structs */
    struct
    {
      short  width;
      short  height;
      int    frameType;
      int    expose;
      double temperature;
      int    binX, binY;
      unsigned short  *img;
    } SBIGFrame;
    
    enum { LIGHT_FRAME , BIAS_FRAME, DARK_FRAME, FLAT_FRAME };
    
    /* Switches */
    ISwitch PowerS[2];
    ISwitch FrameTypeS[4];
    
    /* Numbers */
    INumber FrameN[4];
    INumber BinningN[2];
    INumber ExposeTimeN[1];
    INumber TemperatureN[1];
    
    /* BLOBs */
    IBLOB imageB;
    
    /* Switch vectors */
    ISwitchVectorProperty PowerSP;				/* Connection switch */
    ISwitchVectorProperty FrameTypeSP;				/* Frame type */
    
    /* Number vectors */
    INumberVectorProperty FrameNP;				/* Frame specs */
    INumberVectorProperty BinningNP;				/* Binning */
    INumberVectorProperty ExposeTimeNP;				/* Exposure */
    INumberVectorProperty TemperatureNP;			/* Temperature control */
    
    
    /* BLOB vectors */
    IBLOBVectorProperty imageBP;				/* Data stream */
    
    /* Other */
    double targetTemp;						/* Target temperature */
    
    /* Functions */
    
    /* General */
    void initProperties();
    bool initCamera();
    
    /* CCD */
    void getBasicData(void);
    void handleExposure(void *);
    void connectCCD(void);
    void uploadFile(char * filename);
    int  writeFITS(char *filename, char errmsg[]);
    void grabImage(void);
    int  isCCDConnected(void);
    
    /* Power */
    int  checkPowerS(ISwitchVectorProperty *sp);
    int  checkPowerN(INumberVectorProperty *np);
    int  checkPowerT(ITextVectorProperty *tp);
    
    /* Helper functions */
    int  manageDefaults(char errmsg[]);
    int  getOnSwitch(ISwitchVectorProperty *sp);
    FITS_HDU_LIST * create_fits_header (FITS_FILE *ofp, uint width, uint height, uint bpp);
    static void ISStaticPoll(void *);
    void   ISPoll();
    
};
    
#endif

