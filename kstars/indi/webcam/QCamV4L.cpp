#include <iostream>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/mman.h>

#include "ccvt.h"
#include "QCamV4L.h"
#include "../eventloop.h"

using namespace std;

int device_;
unsigned long options_;
struct video_capability capability_;
struct video_window window_;
struct video_picture picture_;
/// mmap stuf
struct video_mbuf mmap_mbuf_;
unsigned char * mmap_buffer_;
unsigned char * tmpBuffer_;
long mmap_last_sync_buff_;
long mmap_last_capture_buff_;  
bool grey_;
int frameRate_;
bool usingTimer;
bool streamActive;
unsigned char * YBuf,*UBuf,*VBuf, *colorBuffer;

int connectCam(const char * devpath,int preferedPalette,
                 unsigned long options /* cf QCamV4L::options */) {
   options_=options;
   tmpBuffer_=NULL;
   frameRate_=10;
   device_=-1;
   usingTimer = false;
   streamActive = true;
   
   cerr << "In connect Cam with device " << devpath << endl;
   if (-1 == (device_=open(devpath,
                           O_RDONLY | ((options_ & ioNoBlock) ? O_NONBLOCK : 0)))) {
      
      cerr << "strlen " << strlen (devpath) << " -- " << devpath << endl;
      cerr << strerror(errno);
      return -1;
   }
   
   cerr << "Device opened" << endl;
   
   if (device_ != -1) {
      if (-1 == ioctl(device_,VIDIOCGCAP,&capability_)) {
         cerr << "ioctl (VIDIOCGCAP)" << endl;
	 return -1;
      }
      if (-1 == ioctl (device_, VIDIOCGWIN, &window_)) {
         cerr << "ioctl (VIDIOCGWIN)" << endl;
	 return -1;
      }
      if (-1 == ioctl (device_, VIDIOCGPICT, &picture_)) {
         cerr << "ioctl (VIDIOCGPICT)" << endl;
	 
	 return -1;
      }
      init(preferedPalette);
   }

#if 1
   cerr << "initial size w:" << window_.width << " -- h: " << window_.height << endl;
#endif
   //notifier_=NULL;
   //timer_=NULL;
   if (options_&ioUseSelect) {
      addCallback(device_, updateFrame, NULL);
      //notifier_ = new QSocketNotifier(device_, QSocketNotifier::Read, this);
      //connect(notifier_,SIGNAL(activated(int)),this,SLOT(updateFrame()));
     cerr << "Using select to wait new frames." << endl;
   } else {
      //timer_=new QTimer(this);
      usingTimer = true;
      addTimer(1000/frameRate_, callFrame, NULL);
      //connect(timer_,SIGNAL(timeout()),this,SLOT(updateFrame()));
      //timer_->start(1000/frameRate_) ; // value 0 => called every time event loop is empty
      cerr << "Using timer to wait new frames.\n";
   }
   mmap_buffer_=NULL;
   if (mmapInit()) {
      mmapCapture();
   }
   //label(capability_.name);
   
   cerr << "All successful, returning\n";
   return 0;
}

void callFrame(void *p)
{
  updateFrame(0, NULL);
}

int getWidth() { return window_.width; }

int getHeight() { return window_.height; }

void setFPS(int fps)
{
  frameRate_ = fps;
}

int  getFPS()
{
  return frameRate_;
}

char * getDeviceName()
{
  return capability_.name;
}

void enableStream(bool enable)
{
  streamActive = enable;
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

void init(int preferedPalette) {
   // setting palette
   if (preferedPalette) {
      picture_.palette=preferedPalette;
      if (0 == ioctl(device_, VIDIOCSPICT, &picture_)) {
         cerr <<  "found preferedPalette " << preferedPalette << endl;
      } else {
         preferedPalette=0;
         cerr << "preferedPalette " << preferedPalette << " invalid, trying to find one." << endl;
      }
   }
   if (preferedPalette == 0) {
      do {
      /* trying VIDEO_PALETTE_YUV420P (Planar) */
      picture_.palette=VIDEO_PALETTE_YUV420P;
      if (0 == ioctl(device_, VIDIOCSPICT, &picture_)) {
         cerr << "found palette VIDEO_PALETTE_YUV420P" << endl;
	 break;
      }
      cerr << "VIDEO_PALETTE_YUV420P not supported." << endl;
      /* trying VIDEO_PALETTE_YUV420 (interlaced) */
      picture_.palette=VIDEO_PALETTE_YUV420;
      if ( 0== ioctl(device_, VIDIOCSPICT, &picture_)) {
         cerr << "found palette VIDEO_PALETTE_YUV420" << endl;
	 break;
      }
      cerr << "VIDEO_PALETTE_YUV420 not supported." << endl;
      /* trying VIDEO_PALETTE_RGB24 */
      picture_.palette=VIDEO_PALETTE_RGB24;
      if ( 0== ioctl(device_, VIDIOCSPICT, &picture_)) {
         cerr << "found palette VIDEO_PALETTE_RGB24" << endl;
      break;
      }
      cerr << "VIDEO_PALETTE_RGB24 not supported." << endl;
      /* trying VIDEO_PALETTE_GREY */
      picture_.palette=VIDEO_PALETTE_GREY;
      if ( 0== ioctl(device_, VIDIOCSPICT, &picture_)) {
         cerr << "found palette VIDEO_PALETTE_GREY" << endl;
      break;
      }
      cerr << "VIDEO_PALETTE_GREY not supported." << endl;
      cerr << "could not find a supported palette." << endl;
      exit(1);
   }
   while (false);
   }

   grey_ = (picture_.palette==VIDEO_PALETTE_GREY);
   if (ioctl(device_, VIDIOCGPICT, &picture_)) {
      cerr << "ioctl(VIDIOCGPICT)" << endl;
   }

   allocBuffers();

   //setProperty("CameraName",capability_.name);
}

void allocBuffers() {
   delete tmpBuffer_;
   delete YBuf;
   delete UBuf;
   delete VBuf;
   delete colorBuffer;
   
   //yuvBuffer_.setSize(QSize(window_.width,window_.height));
   switch (picture_.palette) {
   case VIDEO_PALETTE_GREY:
      tmpBuffer_=new unsigned char[(int)window_.width * window_.height];
      break;
   case VIDEO_PALETTE_RGB24:
      tmpBuffer_=new unsigned char[(int)window_.width * window_.height * 3];
      break;

   case VIDEO_PALETTE_YUV420:
      tmpBuffer_=new unsigned char[(int)window_.width * window_.height * 3/2 ];
      break;
#if 0
   case VIDEO_PALETTE_YUV422:
   case VIDEO_PALETTE_YUYV:
      tmpBuffer_=new uchar[(int)window_.width * window_.height * 2];
      break;
#endif
   default:
      tmpBuffer_=NULL;
   }
   
   YBuf= new unsigned char[window_.width * window_.height];
   UBuf= new unsigned char[window_.width * window_.height];
   VBuf= new unsigned char[window_.width * window_.height];
   colorBuffer = new unsigned char[window_.width * window_.height * 4];
}

void checkSize(int & x, int & y) {
   if (x>=capability_.maxwidth && y >= capability_.maxheight) {
      x=capability_.maxwidth;
      y=capability_.maxheight;
   } else if (x<=capability_.minwidth || y <=capability_.minheight ) {
      x=capability_.minwidth;
      y=capability_.minheight;
   }
}

void getMaxMinSize(int & xmax, int & ymax, int & xmin, int & ymin)
{
  xmax = capability_.maxwidth;
  ymax = capability_.maxheight;
  xmin = capability_.minwidth;
  ymin = capability_.minheight;
}

bool setSize(int x, int y) {

   checkSize(x,y);
   
   window_.width=x;
   window_.height=y;

   cerr << "New size is x=" << window_.width << " " << "y=" << window_.height <<endl;
   
   if (ioctl (device_, VIDIOCSWIN, &window_)) {
       cerr << "ioctl(VIDIOCSWIN)" << endl;
       return false;
   }
   ioctl (device_, VIDIOCGWIN, &window_);

   allocBuffers();
   
   return true;
}

bool dropFrame() {
   static char nullBuff[640*480*4];
   int bufSize;
   if (mmap_buffer_) {
      mmapCapture();
      mmapSync();
      return true;
   } else {
      switch (picture_.palette) {
      case VIDEO_PALETTE_GREY:
         bufSize=window_.width * window_.height;
         break;
      case VIDEO_PALETTE_YUV420P:
      case VIDEO_PALETTE_YUV420:
         bufSize=window_.width * window_.height *3/2;
         break;
      case VIDEO_PALETTE_RGB24:
         bufSize=window_.width * window_.height *3;
         break;
      default:
         cerr << "invalid palette "<<picture_.palette<<endl;
         exit(1);
      }
      return 0 < read(device_,(void*)nullBuff,bufSize);
   }
}

void updateFrame(int d, void * p) {
   p=p;
   static unsigned char nullBuf[640*480];
   bool res;
   
   if (!streamActive)
    return;
    
   if (grey_) { UBuf=VBuf=nullBuf; } 

   if (mmap_buffer_) {
      //cout <<"c"<<flush;
      mmapCapture();

      //cout <<"s"<<flush;
      mmapSync();
      //setTime();
      res=true;
      
      switch (picture_.palette) {
      case VIDEO_PALETTE_GREY:
         memcpy(YBuf,mmapLastFrame(),window_.width * window_.height);
         break;
      case VIDEO_PALETTE_YUV420P:
         memcpy(YBuf,mmapLastFrame(), window_.width * window_.height);
         memcpy(UBuf,
                mmapLastFrame()+ window_.width * window_.height,
                (window_.width/2) * (window_.height/2));
         memcpy(VBuf,
                mmapLastFrame()+ window_.width * window_.height+(window_.width/2) * (window_.height/2),
                (window_.width/2) * (window_.height/2));
         break;
#if 1
      case VIDEO_PALETTE_YUV420:
         ccvt_420i_420p(window_.width,window_.height,
                           mmapLastFrame(),
                           YBuf,
                           UBuf,
                           VBuf);
         break;
      case VIDEO_PALETTE_RGB24:
         ccvt_bgr24_420p(window_.width,window_.height,
                            mmapLastFrame(),
                         YBuf,
                         UBuf,
                         VBuf);
         break;
#endif

      default:
         cerr << "invalid palette "<<picture_.palette<<endl;
         exit(1);
      }
   } else {
   switch (picture_.palette) {
   case VIDEO_PALETTE_GREY:
      res = 0 < read(device_,YBuf,window_.width * window_.height);
      //if (res) setTime();
      break;
   case VIDEO_PALETTE_YUV420P:
      res = 0 < read(device_,YBuf,window_.width * window_.height);
      //if (res) setTime();
      res = res && (0 < read(device_,UBuf,(window_.width/2) * (window_.height/2)));
      res = res && (0 < read(device_,VBuf,(window_.width/2) * (window_.height/2)));
      break;
#if 0
   case VIDEO_PALETTE_YUV420:
      res = 0 < read(device_,(void*)tmpBuffer_,window_.width * window_.height *3/2);
      if (res) {
         setTime();
         ccvt_420i_420p(window_.width,window_.height,
                        tmpBuffer_,
                        YBuf,
                        UBuf,
                        VBuf);
      }
      break;
   case VIDEO_PALETTE_RGB24:
      res = 0 < read(device_,(void*)tmpBuffer_,window_.width * window_.height * 3);
      if (res) {
         setTime(); 
         ccvt_bgr24_420p(window_.width,window_.height,
                         tmpBuffer_,
                         YBuf,
                         UBuf,
                         VBuf);
      }
      break;
#endif
   default:
      cerr << "invalid palette "<<picture_.palette<<endl;
      exit(1);
   }
   }
   if (res) {
      //newFrameAvaible();
      //cout <<"b"<<flush;
      /*
        if (options_ & haveBrightness) emit brightnessChange(getBrightness());
        if (options_ & haveContrast) emit contrastChange(getContrast());
        if (options_ & haveHue) emit hueChange(getHue());
        if (options_ & haveColor) emit colorChange(getColor());
        if (options_ & haveWhiteness) emit whitenessChange(getWhiteness());
      */
      //cerr << "+";
   } else {
      //newFrameAvaible();
      //cerr("updateFrame");
      //cerr << ".";
   }
   //cout <<"k"<<flush;
   /*int newFrameRate=getFrameRate();
   if (frameRate_ != newFrameRate) {
      frameRate_=newFrameRate;
      
   }*/
   //return res;
   if (usingTimer)
       addTimer(1000/frameRate_, callFrame, NULL);
}

void setContrast(int val) {
   picture_.contrast=val;
   updatePictureSettings();
}

int getContrast() {
   return picture_.contrast;
}

void setBrightness(int val) {
   picture_.brightness=val;
   updatePictureSettings();
}

int getBrightness() {
   return picture_.brightness;
}

void setColor(int val) {
   picture_.colour=val;
   updatePictureSettings();
}

int getColor() {
   return picture_.colour;
}

void setHue(int val) {
   picture_.hue=val;
   updatePictureSettings();
}

int getHue() {
   return picture_.hue;
}

void setWhiteness(int val) {
   picture_.whiteness=val;
   updatePictureSettings();
}

int getWhiteness() {
   return picture_.whiteness;
}

void disconnectCam()
{
   delete tmpBuffer_;
   delete YBuf, UBuf, VBuf;
   tmpBuffer_ = YBuf = UBuf = VBuf = NULL;
   streamActive = false;
   munmap (mmap_buffer_, mmap_mbuf_.size);
   close(device_);
   fprintf(stderr, "Disconnect cam\n");
}

void updatePictureSettings() {
   if (ioctl(device_, VIDIOCSPICT, &picture_) ) {
      cerr << "updatePictureSettings" << endl;
   }
   ioctl(device_, VIDIOCGPICT, &picture_);
}

void refreshPictureSettings() {
   if (ioctl(device_, VIDIOCGPICT, &picture_) ) {
      cerr << "refreshPictureSettings" << endl;
   }
   /*if (options_ & haveBrightness) emit brightnessChange(getBrightness());
   if (options_ & haveContrast) emit contrastChange(getContrast());
   if (options_ & haveHue) emit hueChange(getHue());
   if (options_ & haveColor) emit colorChange(getColor());
   if (options_ & haveWhiteness) emit whitenessChange(getWhiteness());*/
}


void setGrey(bool val) {
   switch (val) {
   case false:
      grey_=val;
      break;
   case true:
      // color mode possible only if palette is not B&W
      if (picture_.palette != VIDEO_PALETTE_GREY) {
         grey_=val;
      }
      break;
   }
   //yuvBuffer_.setGrey(grey_);
}

bool mmapInit() {
   mmap_mbuf_.size = 0;
   mmap_mbuf_.frames = 0;
   mmap_last_sync_buff_=-1;
   mmap_last_capture_buff_=-1;
   mmap_buffer_=NULL;

   if (ioctl(device_, VIDIOCGMBUF, &mmap_mbuf_)) {
      // mmap not supported
      return false;
   }
   mmap_buffer_=(unsigned char *)mmap(NULL, mmap_mbuf_.size, PROT_READ, MAP_SHARED, device_, 0);
   if (mmap_buffer_ == MAP_FAILED) {
      cerr << "mmap" << endl;
      mmap_mbuf_.size = 0;
      mmap_mbuf_.frames = 0;
      mmap_buffer_=NULL;
      return false;
   }
   cerr << "mmap() in use: "
        << "frames="<<mmap_mbuf_.frames
      //<<" size="<<mmap_mbuf_.size
        <<"\n";
   /*
   for(int i=0;i<mmap_mbuf_.frames;++i) {
      cout << i<<"="<<mmap_mbuf_.offsets[i]<<"  ";
   }
   */
   return true;
}

void mmapSync() {
   mmap_last_sync_buff_=(mmap_last_sync_buff_+1)%mmap_mbuf_.frames;
   if (ioctl(device_, VIDIOCSYNC, &mmap_last_sync_buff_) < 0) {
      cerr << "QCamV4L::mmapSync()" << endl;
   }
}

unsigned char * mmapLastFrame() {
   return mmap_buffer_ + mmap_mbuf_.offsets[mmap_last_sync_buff_];
#if 0
   if (mmap_curr_buff_ == 1 ) {
      return mmap_buffer_;
   } else {
      return mmap_buffer_ + mmap_mbuf_.offsets[1];
   }
   return mmap_buffer_ + mmap_mbuf_.offsets[(mmap_curr_buff_-1)% mmap_mbuf_.frames];
   //return mmap_buffer_ + mmap_mbuf_.size*((mmap_curr_buff_-1)%mmap_mbuf_.frames);
#endif
}

void mmapCapture() {
   struct video_mmap vm;
   mmap_last_capture_buff_=(mmap_last_capture_buff_+1)%mmap_mbuf_.frames;
   vm.frame = mmap_last_capture_buff_;
   vm.format = picture_.palette;
   vm.width = window_.width;
   vm.height = window_.height;
   if (ioctl(device_, VIDIOCMCAPTURE, &vm) < 0) {
      cerr << "QCamV4L::mmapCapture" << endl;
   }
}


unsigned char * getY()
{
  return YBuf;
}

unsigned char * getU()
{
 return UBuf;
}

unsigned char * getV()
{
 return VBuf;
}

unsigned char * getColorBuffer()
{
  ccvt_420p_bgr32(window_.width, window_.height,
                      YBuf, UBuf , VBuf, (void*)colorBuffer);

  return colorBuffer;

}
