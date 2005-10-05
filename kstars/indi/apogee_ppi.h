#if 0
    Apogee PPI
    INDI Interface for Apogee PPI
    Copyright (C) 2005 Jasem Mutlaq (mutlaqja AT ikarustech DOT com)

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

#ifndef APOGEE_PPI_H
#define APOGEE_PPI_H

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
#include "apogee/CameraIO_Linux.h"

#define mydev           "Apogee PPI"

#define COMM_GROUP	"Communication"
#define EXPOSE_GROUP	"Expose"
#define IMAGE_GROUP	"Image Settings"
#define DATA_GROUP      "Data Channel"

#define POLLMS		1000		/* Polling time (ms) */
#define TEMP_THRESHOLD  .25		/* Differential temperature threshold (C)*/

#define MAX_PIXELS	4096
#define MAXHBIN 	8
#define MAXVBIN 	64
#define MIN_CCD_TEMP	-60
#define MAX_CCD_TEMP	40
#define MAXCOLUMNS 	16383
#define MAXROWS 	16383
#define MAXTOTALCOLUMNS 16383
#define MAXTOTALROWS 	16383

#define FILENAMESIZ	2048
#define LIBVERSIZ 	1024
#define PREFIXSIZ	64
#define PIPEBUFSIZ	8192
#define FRAME_ILEN	64

#define getBigEndian(p) ( ((p & 0xff) << 8) | (p  >> 8))

class ApogeeCam {
  
  public:
    
    ApogeeCam();
    ~ApogeeCam();
    
    /* INDI Functions that must be called from indidrivermain */
    void ISGetProperties (const char *dev);
    void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
    void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
    void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
    void ISPoll();
    
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
    } APGFrame;
    
    enum { LIGHT_FRAME , BIAS_FRAME, DARK_FRAME, FLAT_FRAME };
    
    /* Switches */
    ISwitch PowerS[2];
    ISwitch *ApogeeModelS;
    ISwitch FrameTypeS[4];
    
    /* Numbers */
    INumber FrameN[4];
    INumber BinningN[2];
    INumber ExposeTimeN[1];
    INumber TemperatureN[1];
    INumber DataChannelN[1];
    
    /* BLOBs */
    IBLOB imageB;
    
    /* Switch vectors */
    ISwitchVectorProperty PowerSP;				/* Connection switch */
    ISwitchVectorProperty ApogeeModelSP;			/* Apogee Model */
    ISwitchVectorProperty FrameTypeSP;				/* Frame type */
    
    /* Number vectors */
    INumberVectorProperty FrameNP;				/* Frame specs */
    INumberVectorProperty BinningNP;				/* Binning */
    INumberVectorProperty ExposeTimeNP;				/* Exposure */
    INumberVectorProperty TemperatureNP;			/* Temperature control */
    
    
    /* BLOB vectors */
    IBLOBVectorProperty imageBP;				/* Data stream */
    
    /* Other */
    static int streamTimerID;					/* Stream ID */
    double targetTemp;						/* Target temperature */
    CCameraIO *cam;						/* Apogee Camera object */
    
    /* Functions */
    
    /* General */
    void initProperties();
    bool loadXMLModel();
    bool initCamera();
    
    /* CCD */
    void getBasicData(void);
    void handleExposure(void *);
    void connectCCD(void);
    void uploadFile(char * filename);
    int  writeFITS(char *filename, char errmsg[]);
    int  setImageArea(char errmsg[]);
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
    unsigned short hextoi(char* instr);
    double min();
    double max();
    
};
    
#endif

