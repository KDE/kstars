/*
    Phlips webcam driver
    Copyright (C) 2004 by Jasem Mutlaq

    This library uses code from qastrocam.
    
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

#include <iostream>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/mman.h>
#include <string.h>

#include "ccvt.h"
#include "pwc-ioctl.h"
#include "QCamV4L.h"
#include "philipsV4L.h"
#include "../eventloop.h"

#define ERRMSG_SIZ	1024

extern int errno;

using namespace std;

int whiteBalanceMode_;
int whiteBalanceRed_;
int whiteBalanceBlue_;
int lastGain_;
int multiplicateur_;
int skippedFrame_;
int type_;

extern unsigned char * tmpBuffer_;
extern long mmap_last_sync_buff_;
extern long mmap_last_capture_buff_;  
extern struct video_mbuf mmap_mbuf_;
extern int device_;
extern unsigned long options_;
extern int frameRate_;
extern bool usingTimer;
extern bool streamActive;
extern bool frameUpdate;
extern struct video_capability capability_;
extern struct video_window window_;
extern struct video_picture picture_;
extern unsigned char * mmap_buffer_;
extern int  selectCallBackID;
extern int  timerCallBackID;

int connectPhilips(const char * devpath, char *errmsg)
{
   options_= (ioNoBlock|ioUseSelect|haveBrightness|haveContrast|haveColor);
   struct pwc_probe probe;
   bool IsPhilips = false;
   frameRate_=15;
   device_=-1;
   usingTimer = false;
   streamActive = true;
   
   if (-1 == (device_=open(devpath,
                           O_RDONLY | ((options_ & ioNoBlock) ? O_NONBLOCK : 0)))) {
      
      strncpy(errmsg, strerror(errno), 1024);
      cerr << strerror(errno);
      return -1;
   } 
  
   cerr << "Device opened" << endl;
   
   if (device_ != -1) {
      if (-1 == ioctl(device_,VIDIOCGCAP,&capability_)) {
         cerr << "Error: ioctl (VIDIOCGCAP)" << endl;
	 strncpy(errmsg, "ioctl (VIDIOCGCAP)", 1024);
	 return -1;
      }
      if (-1 == ioctl (device_, VIDIOCGWIN, &window_)) {
         cerr << "Error: ioctl (VIDIOCGWIN)" << endl;
	 strncpy(errmsg, "ioctl (VIDIOCGWIN)", 1024);
	 return -1;
      }
      if (-1 == ioctl (device_, VIDIOCGPICT, &picture_)) {
         cerr << "Error: ioctl (VIDIOCGPICT)" << endl;
	 strncpy(errmsg, "ioctl (VIDIOCGPICT)", 1024);
	 return -1;
      }
      init(0);
   }
   
  // Check to see if it's really a philips webcam       
  if (ioctl(device_, VIDIOCPWCPROBE, &probe) == 0) 
  {
  	    if (!strcmp(capability_.name,probe.name))
	    {
	       IsPhilips = true;
	       type_=probe.type;
            }
 }
	 
  if (IsPhilips)
      cerr << "Philips webcam type " << type_ << " detected" << endl;
  else
  {
   strncpy(errmsg, "No Philips webcam detected.", 1024);
   return -1;
  }
    
   cerr << "initial size w:" << window_.width << " -- h: " << window_.height << endl;
   
   if (options_&ioUseSelect)
   {
      selectCallBackID = addCallback(device_, updatePhilipsFrame, NULL);
       cerr << "Using select() to wait new frames." << endl;
   }
   else
   {
      usingTimer = true;
      timerCallBackID = addTimer(1000/frameRate_, callPhilipsFrame, NULL);
      cerr << "Using timer to wait new frames.\n";
   }
   mmap_buffer_=NULL;
   if (mmapInit()) {
      mmapCapture();
   }
   //label(capability_.name);
   
   setWhiteBalanceMode(PWC_WB_AUTO, errmsg);
   multiplicateur_=1;
   skippedFrame_=0;
   lastGain_=getGain();

   //setDeviceFD(device_);
   
   cerr << "All successful, returning\n";
   return 0;
}

void callPhilipsFrame(void *p)
{
  p=p;
  updatePhilipsFrame(0, NULL);
}

//QCamFrame yuvFrame() const { return yuvBuffer_; }

/*void resize(const QSize & s) {
#if 1
   cout << "QCamV4L::resize("
        << s.width()
        << "x"
        << s.height()
        << ")"
        << endl;
#endif
   setSize(s.width(),s.height());
}*/


void checkPhilipsSize(int & x, int & y) 
{
 if (x>=capability_.maxwidth && y >= capability_.maxheight) {
      x=capability_.maxwidth;
      y=capability_.maxheight;
   } else if (x>=352 && y >=288 && type_<700) {
      x=352;y=288;
   } else if (x>=320 && y >= 240) {
      x=320;y=240;
   } else if (x>=176 && y >=144 && type_<700 ) {
      x=176;y=144;
   } else if (x>=160 && y >=120 ) {
      x=160;y=120;
   } else {
      x=capability_.minwidth;
      y=capability_.minheight;
   }
}

bool setPhilipsSize(int x, int y) 
{

   int oldX, oldY;
   char msg[ERRMSG_SIZ];
   checkPhilipsSize(x,y);
   
   oldX = window_.width;
   oldY = window_.height;
   
   window_.width=x;
   window_.height=y;
   
   if (ioctl (device_, VIDIOCSWIN, &window_))
   {
       snprintf(msg, ERRMSG_SIZ, "ioctl(VIDIOCSWIN) %s", strerror(errno));
       cerr << msg << endl;
       window_.width=oldX;
       window_.height=oldY;
       return false;
   }
   ioctl (device_, VIDIOCGWIN, &window_);

   cerr << "New size is x=" << window_.width << " " << "y=" << window_.height <<endl;
   
   allocBuffers();
   
   return true;
}

void updatePhilipsFrame(int /*d*/, void * /*p*/)
{
   static int tmp;
   if (!streamActive) return;
   
   if (skippedFrame_ < multiplicateur_- 1)
   {
      /* TODO consider SC modded cams
      if (skippedFrame_ >= multiplicateur_ - 3)
             stopAccumulation();*/
      
      if (dropFrame())
      {
         skippedFrame_++;
         //exposureTimeLeft_->setProgress(skippedFrame_);
	 cerr << "Progress of time left is " << skippedFrame_ << endl;
         tmp=0;
      }
      return;
      
   } else
   {
      //if (updateFrame())
      updateFrame(0, NULL);
      if (frameUpdate)
      {
         skippedFrame_=0;
         /* TODO consider SC modded cams 
	 if (multiplicateur_ > 1)
	 {
            startAccumulation();
            //if (guiBuild()) exposureTimeLeft_->reset();
	    cerr << "Reseting time left exposure... " << endl;
         }*/
	 

	 // TODO get gain..etc
         //setProperty("Gain",tmpVal=getGain(),false);
         //emit gainChange(tmpVal);
	 //setProperty("FrameRateSecond",(tmpVal=getFrameRate())/(double)multiplicateur_);
         //emit frameRateChange(tmpVal);
	 
         /*if (liveWhiteBalance_ || refreshGui_) {
            getWhiteBalance();
         }
         if (SCmodCtrl_) {
            setProperty("ExposureTime",multiplicateur_/(double)getFrameRate());
            emit frameRateMultiplicateurChange(multiplicateur_);
            emit exposureTime(multiplicateur_/(double)getFrameRate());
         } else {
            setProperty("ExposureTime",1/(double)getFrameRate());
         }
         refreshGui_=false;
         return true;
      } else {
       //  refreshGui_=false;
         return false; */
      }
   }
}

int saveSettings(char *errmsg)
{
   if (ioctl(device_, VIDIOCPWCSUSER)==-1)
    {
      snprintf(errmsg, ERRMSG_SIZ, "VIDIOCPWCSUSER %s", strerror(errno));
      return -1;
   }
   
   return 0;
}

void restoreSettings()
 {
   ioctl(device_, VIDIOCPWCRUSER);
   refreshPictureSettings();
}

void restoreFactorySettings() {
   ioctl(device_, VIDIOCPWCFACTORY);
   refreshPictureSettings();
}

int setGain(int val, char *errmsg)
 {
   if(-1==ioctl(device_, VIDIOCPWCSAGC, &val))
   {
      snprintf(errmsg, ERRMSG_SIZ, "VIDIOCPWCSAGC %s", strerror(errno));
      return -1;
   }
   else lastGain_=val;
   
   cerr << "setGain "<<val<<endl;
   
   return lastGain_;
}

int getGain()
{
   int gain;
   char msg[ERRMSG_SIZ];
   static int cpt=0;
   if ((cpt%4)==0)
   {
      if (-1==ioctl(device_, VIDIOCPWCGAGC, &gain))
      {
         //perror("VIDIOCPWCGAGC");
	 snprintf(msg, ERRMSG_SIZ, "VIDIOCPWCGAGC %s", strerror(errno));
	 cerr << msg << endl;
         gain=lastGain_;
      } else
      {
         ++cpt;
         lastGain_=gain;
      }
   } else
   {
      ++cpt;
      gain=lastGain_; 
   }
   //cerr << "get gain "<<gain<<endl;
   if (gain < 0) gain*=-1;
   return gain;
}

int setExposure(int val, char *errmsg)
 {
   //cout << "set exposure "<<val<<"\n";
   
   if (-1==ioctl(device_, VIDIOCPWCSSHUTTER, &val))
   {
     snprintf(errmsg, ERRMSG_SIZ, "VIDIOCPWCSSHUTTER %s", strerror(errno));
     return -1;
  }
  
  return 0;
}

void setCompression(int val)
{
   ioctl(device_, VIDIOCPWCSCQUAL, &val); 
}

int getCompression()
{
   int gain;
   ioctl(device_, VIDIOCPWCGCQUAL , &gain);
   if (gain < 0) gain*=-1;
   return gain;
}

int setNoiseRemoval(int val, char *errmsg)
{
   if (-1 == ioctl(device_, VIDIOCPWCSDYNNOISE, &val))
   {
       snprintf(errmsg, ERRMSG_SIZ, "VIDIOCPWCGDYNNOISE %s", strerror(errno));
       return -1;
   }
   
   return 0;
}

int getNoiseRemoval()
{
   int gain;
   char msg[ERRMSG_SIZ];
   
   if (-1 == ioctl(device_, VIDIOCPWCGDYNNOISE , &gain)) 
   {
   	 snprintf(msg, ERRMSG_SIZ, "VIDIOCPWCGDYNNOISE %s", strerror(errno));
	 cerr << msg << endl;
   }
      
   cout <<"get noise = "<<gain<<endl;
   return gain;
}

int setSharpness(int val, char *errmsg)
 {
   if (-1 == ioctl(device_, VIDIOCPWCSCONTOUR, &val))
   {
       snprintf(errmsg, ERRMSG_SIZ, "VIDIOCPWCSCONTOUR %s", strerror(errno));
       return -1;
  }
  
  return 0;
}

int getSharpness()
{
   int gain;
   char msg[ERRMSG_SIZ];
   
   if (-1 == ioctl(device_, VIDIOCPWCGCONTOUR, &gain)) 
   {
      snprintf(msg, ERRMSG_SIZ, "VIDIOCPWCGCONTOUR %s", strerror(errno));
      cerr << msg << endl;
  }
   
   cout <<"get sharpness = "<<gain<<endl;
   return gain;
}

int setBackLight(bool val, char *errmsg)
{
   static int on=1;
   static int off=0;
   if (-1 == ioctl(device_,VIDIOCPWCSBACKLIGHT, &  val?&on:&off))
   {
        snprintf(errmsg, ERRMSG_SIZ, "VIDIOCPWCSBACKLIGHT %s", strerror(errno));
	return -1;
   }
   
   return 0;
}

bool getBackLight()
{
   int val;
   char msg[ERRMSG_SIZ];
   
   if (-1 == ioctl(device_,VIDIOCPWCGBACKLIGHT, & val)) 
   {
      snprintf(msg, ERRMSG_SIZ, "VIDIOCPWCSBACKLIGHT %s", strerror(errno));
      cerr << msg << endl;
   }

   return val !=0;
}

int setFlicker(bool val, char *errmsg)
{
   static int on=1;
   static int off=0;
   if (-1 == ioctl(device_,VIDIOCPWCSFLICKER, val?&on:&off))
   {
      snprintf(errmsg, ERRMSG_SIZ, "VIDIOCPWCSFLICKER %s", strerror(errno));
      return -1;
   }
   
   return 0;
   
}

bool getFlicker() 
{
   int val;
   char msg[ERRMSG_SIZ];
   
   if (-1 == ioctl(device_,VIDIOCPWCGFLICKER, & val))
   {
         snprintf(msg, ERRMSG_SIZ, "VIDIOCPWCGFLICKER %s", strerror(errno));
	 cerr << msg << endl;
   }
   
   return val !=0;
}

void setGama(int val)
{
   picture_.whiteness=val;
   updatePictureSettings();
}

int getGama() 
{
   return picture_.whiteness;
}

int setFrameRate(int value, char *errmsg)
 {
   window_.flags = (window_.flags & ~PWC_FPS_MASK) | ((value << PWC_FPS_SHIFT) & PWC_FPS_MASK);
   if (ioctl(device_, VIDIOCSWIN, &window_))
   {
             snprintf(errmsg, ERRMSG_SIZ, "setFrameRate %s", strerror(errno));
	     return -1;
   }
   
   ioctl(device_, VIDIOCGWIN, &window_);
   frameRate_ = value;
   
   return 0;
   //emit exposureTime(multiplicateur_/(double)getFrameRate());
}

int getFrameRate() 
{
   return ((window_.flags&PWC_FPS_FRMASK)>>PWC_FPS_SHIFT); 
}

int getWhiteBalance()
{
   char msg[ERRMSG_SIZ];
   struct pwc_whitebalance tmp_whitebalance;
   tmp_whitebalance.mode = tmp_whitebalance.manual_red = tmp_whitebalance.manual_blue = tmp_whitebalance.read_red = tmp_whitebalance.read_blue = PWC_WB_AUTO;
   
   if (ioctl(device_, VIDIOCPWCGAWB, &tmp_whitebalance)) 
   {
           snprintf(msg, ERRMSG_SIZ, "getWhiteBalance %s", strerror(errno));
	   cerr << msg << endl;
   }
   else
   {
#if 0
      cout << "mode="<<tmp_whitebalance.mode
           <<" mr="<<tmp_whitebalance.manual_red
           <<" mb="<<tmp_whitebalance.manual_blue
           <<" ar="<<tmp_whitebalance.read_red
           <<" ab="<<tmp_whitebalance.read_blue
           <<endl;
#endif
      /* manual_red and manual_blue are garbage :-( */
      whiteBalanceMode_=tmp_whitebalance.mode;
   }
            
      return whiteBalanceMode_;
     
      /*switch(whiteBalanceMode_) {
      case PWC_WB_INDOOR:
         setProperty("WhiteBalanceMode","Indor");
         break;
      case PWC_WB_OUTDOOR:
         setProperty("WhiteBalanceMode","Outdoor");
         break;
      case PWC_WB_FL:
         setProperty("WhiteBalanceMode","Neon");
         break;
      case PWC_WB_MANUAL:
          setProperty("WhiteBalanceMode","Manual");
          whiteBalanceRed_=tmp_whitebalance.manual_red;
          whiteBalanceBlue_=tmp_whitebalance.manual_blue;

         break;
      case PWC_WB_AUTO:
         setProperty("WhiteBalanceMode","Auto");
         whiteBalanceRed_=tmp_whitebalance.read_red;
         whiteBalanceBlue_=tmp_whitebalance.read_blue;
         break;
      default:
         setProperty("WhiteBalanceMode","???");
      }
         
      emit whiteBalanceModeChange(whiteBalanceMode_);

      if (((whiteBalanceMode_ == PWC_WB_AUTO) && liveWhiteBalance_)
          || whiteBalanceMode_ != PWC_WB_AUTO) {
         setProperty("WhiteBalanceRed",whiteBalanceRed_);
         emit whiteBalanceRedChange(whiteBalanceRed_);
         setProperty("WhiteBalanceBlue",whiteBalanceBlue_);   
         emit whiteBalanceBlueChange(whiteBalanceBlue_);
         if (guiBuild()) {         
            remoteCTRLWBred_->show();
            remoteCTRLWBblue_->show();
         }
      } else {
         if (guiBuild()) {         
            remoteCTRLWBred_->hide();
            remoteCTRLWBblue_->hide();
         }
      }
   }*/
}

int setWhiteBalance(char *errmsg) 
{
   struct pwc_whitebalance wb;
   wb.mode=whiteBalanceMode_;
   if (wb.mode == PWC_WB_MANUAL)
   {
      wb.manual_red=whiteBalanceRed_;
      wb.manual_blue=whiteBalanceBlue_;
   }
   
   if (ioctl(device_, VIDIOCPWCSAWB, &wb))
   {
       snprintf(errmsg, ERRMSG_SIZ, "setWhiteBalance %s", strerror(errno)); 
       return -1;
   }
   
   return 0;
}

int setWhiteBalanceMode(int val, char *errmsg)
 {
   if (val == whiteBalanceMode_) 
      return whiteBalanceMode_;
      
   if (val != PWC_WB_AUTO)
   {
      if ( val != PWC_WB_MANUAL)
      {
         whiteBalanceMode_=val;
         if (setWhiteBalance(errmsg) < 0)
	   return -1;
      }
      
      //whiteBalanceMode_=PWC_WB_AUTO;
      whiteBalanceMode_= val;
      if (setWhiteBalance(errmsg) < 0)
       return -1;
      getWhiteBalance();
   }

   /*if (guiBuild()) {
      if (val != PWC_WB_AUTO
          || ( liveWhiteBalance_ && (val ==PWC_WB_AUTO))) {
         remoteCTRLWBred_->show();
         remoteCTRLWBblue_->show();
      } else {
         remoteCTRLWBred_->hide();
         remoteCTRLWBblue_->hide();
      }
   }*/
   
   whiteBalanceMode_=val;
   if (setWhiteBalance(errmsg) < 0)
    return -1;
   getWhiteBalance();
   
   return 0;
}

int setWhiteBalanceRed(int val, char *errmsg) 
{
   whiteBalanceMode_ = PWC_WB_MANUAL;
   whiteBalanceRed_=val;
   if (setWhiteBalance(errmsg) < 0)
    return -1;
    
   return 0;
}

int setWhiteBalanceBlue(int val, char *errmsg)
{
   whiteBalanceMode_ = PWC_WB_MANUAL;
   whiteBalanceBlue_=val;
   if (setWhiteBalance(errmsg) < 0)
     return -1;
     
   return 0;
}

