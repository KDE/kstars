#include <iostream>

#include "ccvt.h"
#include "../eventloop.h"

using namespace std;

int connectCam(const char * devpath,int preferedPalette, unsigned long options) { return -1; }

void callFrame(void *p) {}

int getWidth() { return -1;}

int getHeight() { return -1;}

void setFPS(int fps) {}

int  getFPS() { return -1; }

char * getDeviceName() { return NULL; }

void enableStream(bool enable) {}

void init(int preferedPalette) {}

void allocBuffers() {}

void checkSize(int & x, int & y) {}

void getMaxMinSize(int & xmax, int & ymax, int & xmin, int & ymin) {}

bool setSize(int x, int y) { return false; }

bool dropFrame() { return false; }

void updateFrame(int d, void * p) {}

void setContrast(int val) {}

int getContrast() { return -1; }
 
void setBrightness(int val) { return;}
   
int getBrightness() { return -1; } 

void setColor(int val) {}

int getColor() { return -1; }

void setHue(int val) {}

int getHue() { return -1; }

void setWhiteness(int val) {}
   
int getWhiteness() { return -1;}

void disconnectCam() {}

void updatePictureSettings() {}

void setGrey(bool val) {}

bool mmapInit() { return false; }

void mmapSync() {}
   
unsigned char * mmapLastFrame() { return NULL; }

void mmapCapture() {}
   
unsigned char * getY() { return NULL; }

unsigned char * getU() { return NULL; }

unsigned char * getV() { return NULL; }

unsigned char * getColorBuffer() { return NULL; }
