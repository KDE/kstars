#ifndef QCAMV4L_H
#define QCAMV4L_H

#include <stdio.h>
#include <stdlib.h>
#include <linux/videodev.h>

int connectCam(const char * devpath="/dev/video0", int preferedPalette = 0 /* auto palette*/);
void disconnectCam();
char * getDeviceName();

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

