/*
    Copyright (C) 2004 by Jasem Mutlaq

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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

#ifndef QCAMV4L_H
#define QCAMV4L_H

#include <stdio.h>
#include <stdlib.h>
#include "videodev.h"


int connectCam(const char * devpath, char *errmsg);
void disconnectCam();
char * getDeviceName();

#define VIDEO_COMPRESSION_LEVEL		4

/* Image settings */
int  getBrightness();
int  getContrast();
int  getColor();
int  getHue();
int  getWhiteness();
void setContrast(int val);
void setBrightness(int val);
void setColor(int val);
void setHue(int val);
void setWhiteness(int val);

/* Updates */
void updatePictureSettings();
void refreshPictureSettings();
bool dropFrame();
void updateFrame(int d, void * p);
void callFrame(void *p);
void enableStream(bool enable);

/* Image Size */
int getWidth();
int getHeight();
void checkSize(int & x, int & y);
bool setSize(int x, int y);
void getMaxMinSize(int & xmax, int & ymax, int & xmin, int & ymin);


/* Frame rate */
void setFPS(int fps);
int  getFPS();

/* mmap routines and buffer retrieval */
void init(int preferedPalette);
void allocBuffers();
bool mmapInit();
void mmapCapture();
void mmapSync();
unsigned char * mmapLastFrame();
unsigned char * getY();
unsigned char * getU();
unsigned char * getV();
unsigned char * getColorBuffer();


#endif

