/*  CCD Preview
    Copyright (C) 2005 Dirk Huenniger <hunniger@cip.physik.uni-bonn.de>

    Adapted from streamwg by Jasem Mutlaq

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
    
 */
 
#include "ccdpreviewwg.h"
#include "indistd.h"
#include "indidriver.h"
#include "indimenu.h"
#include "Options.h"

#include <kmessagebox.h>
#include <klocale.h>
#include <kdebug.h>
#include <kpushbutton.h>
#include <kiconloader.h>
#include <ktempfile.h>
#include <kio/netaccess.h>
#include <kfiledialog.h>
#include <kcombobox.h>
#include <kurl.h>
#include <klineedit.h>

#include <qsocketnotifier.h>
#include <qimage.h>
#include <qpainter.h>
#include <qstringlist.h>
#include <qdir.h>
#include <qlayout.h>
#include <qlabel.h>


#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>


#define STREAMBUFSIZ		1024

#include "ccdpreviewwg.moc"

FILE *CCDwfp;

 CCDPreviewWG::CCDPreviewWG(INDIStdDevice *inStdDev, QWidget * parent, const char * name) : CCDPreviewForm(parent, name)
 {
 
   stdDev         = inStdDev;

   fwhm 	  = -1;
   mu		  = -1;
   streamWidth    = streamHeight = -1;
   processStream  = colorFrame = false;
   streamFrame      = new CCDVideoWG(videoFrame);
   streamFrame->bytesPerPixel= 1;
   streamFrame->PixelOrder= 1;	 
   gammaChanged(gammaBar->value());
   brightnessChanged(brightnessBar->value());
   contrastChanged(contrastBar->value());    
	 
  KIconLoader *icons = KGlobal::iconLoader();
  
  playPix    = icons->loadIcon( "player_play", KIcon::Toolbar );
  pausePix   = icons->loadIcon( "player_pause", KIcon::Toolbar );
  capturePix = icons->loadIcon( "frame_image", KIcon::Toolbar );
  
  playB->setPixmap(pausePix);	
  captureB->setPixmap(capturePix);
  
  imgFormatCombo->insertStrList(QImage::outputFormats());
  
  connect(playB, SIGNAL(clicked()), this, SLOT(playPressed()));
  connect(captureB, SIGNAL(clicked()), this, SLOT(captureImage()));
  connect(brightnessBar, SIGNAL(valueChanged(int)), this, SLOT(brightnessChanged(int)));
  connect(contrastBar, SIGNAL(valueChanged(int)), this, SLOT(contrastChanged(int)));
  connect(gammaBar, SIGNAL(valueChanged(int)), this, SLOT(gammaChanged(int)));
  connect(focalEdit, SIGNAL(returnPressed()), this, SLOT(updateFWHM()));
 }
 
CCDPreviewWG::~CCDPreviewWG()
{

}

void CCDPreviewWG::closeEvent ( QCloseEvent * e )
{
  stdDev->streamDisabled();
  processStream = false;
  e->accept();
}

void CCDPreviewWG::setColorFrame(bool color)
{
  colorFrame = color;
}

/*void CCDPreviewWG::establishDataChannel(QString host, int port)
{
        QString errMsg;
	struct sockaddr_in pin;
	struct hostent *serverHostName = gethostbyname(host.ascii());
	errMsg = QString("Connection to INDI host at %1 on port %2 failed.").arg(host).arg(port);
	
	memset(&pin, 0, sizeof(pin));
	pin.sin_family 		= AF_INET;
	pin.sin_addr.s_addr 	= ((struct in_addr *) (serverHostName->h_addr))->s_addr;
	pin.sin_port 		= htons(port);

	if ( (streamFD = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
	 KMessageBox::error(0, i18n("Cannot create socket."));
	 return;
	}

	if ( ::connect(streamFD, (struct sockaddr*) &pin, sizeof(pin)) == -1)
	{
	  KMessageBox::error(0, errMsg);
	  streamFD = -1;
	  return;
	}

	// callback notified
	sNotifier = new QSocketNotifier( streamFD, QSocketNotifier::Read, this);
        QObject::connect( sNotifier, SIGNAL(activated(int)), this, SLOT(streamReceived()));
}*/

void CCDPreviewWG::enableStream(bool enable)
{
  if (enable)
  {
    processStream = true;
    show();
  }
  else
  {
    processStream = false;
    playB->setPixmap(pausePix);
    hide();
  }
  
}

void CCDPreviewWG::setCtrl(int wd, int ht,int po, int bpp,unsigned long mgd)
{
  long i;
  streamWidth  = wd;
  streamHeight = ht;
  streamFrame->totalBaseCount = wd * ht * bpp;
 // fprintf(stderr,"%d %d %d",wd,ht,bpp)
  streamFrame->Width = wd;
  streamFrame->Height = ht;	
  streamFrame->bytesPerPixel=bpp;
  streamFrame->PixelOrder=po;
  streamFrame->maxGoodData=mgd;	
  if (streamFrame->streamBuffer!=NULL) {
    free(streamFrame->streamBuffer);	  
  }
  streamFrame->streamBufferPos=0;
  streamFrame->streamBuffer=(unsigned char*) 
	malloc(sizeof(unsigned char)*streamFrame->totalBaseCount);
  for (i=0;i<streamFrame->totalBaseCount;i++) {
    streamFrame->streamBuffer[i]=0;
  }
  resize(wd + layout()->margin() * 2 , ht + playB->height() + brightnessLabel->height()
      + contrastLabel->height() + gammaLabel->height() + focalEdit->height() + FWHMLabel->height() + layout()->margin() * 2 + layout()->spacing()*6);  
  streamFrame->resize(wd, ht);
}


void CCDPreviewWG::setCCDInfo(double in_fwhm, int in_mu)
{
  fwhm = in_fwhm;
  mu = in_mu;

  updateFWHM();

}

void CCDPreviewWG::updateFWHM()
{
  double focal_length(-1), fwhm_arcsec;

  focal_length = focalEdit->text().toDouble();

  if (focal_length <= 0 || fwhm <= 0 || mu <= 0)
  {
    FWHMLabel->setText("--");
    return;
  }

  fwhm_arcsec = (206.26 / focal_length) * fwhm * mu;

  FWHMLabel->setText(QString("%1").arg(fwhm_arcsec, 0, 'g', 3));
  
}


void CCDPreviewWG::resizeEvent(QResizeEvent *ev)
{
  streamFrame->resize(ev->size().width() - layout()->margin() * 2, ev->size().height() - playB->height() - layout()->margin() * 2 - layout()->spacing());
}
 
void CCDPreviewWG::playPressed()
{

 if (processStream)
 {
  playB->setPixmap(playPix);	
  processStream = false;
 }
 else
 {
  playB->setPixmap(pausePix);	
  processStream = true;
 }
 
}

void CCDPreviewWG::captureImage()
{
  QString fname;
  QString fmt;
  KURL currentFileURL;
  QString currentDir = Options::fitsSaveDirectory();
  KTempFile tmpfile;
  tmpfile.setAutoDelete(true);

  fmt = imgFormatCombo->currentText();

  currentFileURL = KFileDialog::getSaveURL( currentDir, fmt );
  
  if (currentFileURL.isEmpty()) return;

  if ( currentFileURL.isValid() )
  {
	currentDir = currentFileURL.directory();

	if ( currentFileURL.isLocalFile() )
  	   fname = currentFileURL.path();
	else
	   fname = tmpfile.name();

	if (fname.right(fmt.length()).lower() != fmt.lower()) 
	{
	  fname += ".";
	  fname += fmt.lower();
	}
	  
	streamFrame->qPix.save(fname, fmt.ascii());
	
	//set rwx for owner, rx for group, rx for other
	chmod( fname.ascii(), S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH );

	if ( tmpfile.name() == fname )
	{ //need to upload to remote location
	
	  if ( ! KIO::NetAccess::upload( tmpfile.name(), currentFileURL, (QWidget*) 0 ) )
	  {
		QString message = i18n( "Could not upload image to remote location: %1" ).arg( currentFileURL.prettyURL() );
		KMessageBox::sorry( 0, message, i18n( "Could not upload file" ) );
	  }
	}
  }
  else
  {
		QString message = i18n( "Invalid URL: %1" ).arg( currentFileURL.url() );
		KMessageBox::sorry( 0, message, i18n( "Invalid URL" ) );
  }

}


CCDVideoWG::CCDVideoWG(QWidget * parent, const char * name) : QFrame(parent, name, Qt::WNoAutoErase)
{
  streamImage    = NULL;
  streamBuffer	  = NULL;
  displayBuffer    = NULL;
  grayTable=new QRgb[256];
  for (int i=0;i<256;i++) {
        grayTable[i]=qRgb(i,i,i);
  }
  grayTable[255]=qRgb(255,0,0);
}
      
CCDVideoWG::~CCDVideoWG() 
{
  delete (streamImage);
  if (displayBuffer!=NULL) {
    free(displayBuffer);	  
  }
  if (streamBuffer!=NULL) {
    free(streamBuffer);	  
  }

  delete [] (grayTable);
}

void CCDVideoWG::newFrame(unsigned char *buffer, int buffSize, int w, int h)
{
  long i,offs,d;
  offs=0;	
  Width=w;
  Height=h;
  d=2*bytesPerPixel;
  if (streamBuffer==NULL) {
    return;
  }
  if (streamBufferPos>=totalBaseCount) {
    streamBufferPos=0;
  }
  for (i=streamBufferPos;((i<streamBufferPos+buffSize)&&(i<totalBaseCount));i++) {
    if (PixelOrder==PIXELORDER_NORMAL) {
      streamBuffer[i]=buffer[i-streamBufferPos];
    }
    if (PixelOrder==PIXELORDER_DUAL) {
      if (i%d==0) {
	      offs=i/2;
      }
      if ((i%d)<bytesPerPixel) {
	streamBuffer[i-offs]=buffer[i-streamBufferPos];
      }
      else {
        streamBuffer[Width*Height*bytesPerPixel-(i-offs)]=buffer[i-streamBufferPos];
      }
    }
  }
  streamBufferPos=i;
  /*if (buffSize > totalBaseCount)
     streamImage = new QImage(buffer, w, h, 32, 0, 0, QImage::BigEndian);
   else
    streamImage = new QImage(streamBuffer, w, h, 8, grayTable, 256, QImage::IgnoreEndian);
   update();
  */
  redrawVideoWG();
}

void CCDVideoWG::redrawVideoWG(void)
{
  int x,y,b;
  double val;
  unsigned long dat;
  if (displayBuffer!=NULL) {
    displayBuffer=(unsigned char*) 
      realloc(displayBuffer, sizeof(unsigned char)*Width*Height);
  }
  else {
  displayBuffer=(unsigned char*) 
      malloc(sizeof(unsigned char)*Width*Height);
  } 
  if (displayBuffer==NULL) {
     return;
  }
  if (streamBuffer==NULL) {
    return;
  }
  for (x=0;x<Width;x++) {
    for (y=0;y<Height;y++) {
      dat=0;
      for (b=0;b<bytesPerPixel;b++) {
        dat=(unsigned long) (dat+ streamBuffer[Width*y*bytesPerPixel+x*bytesPerPixel+b]*pow(256.0,b));
      }
      if (dat<=maxGoodData) {
        val=128+scale*(dat-offset)/(pow(256.0,bytesPerPixel)-1.0);
	if (val<0.0) {
		val=0.0;
	}
        val=pow(val/255.0,1.0/gamma)*255.0;
        if (val>255.0) {
          val=255.0; 
	}
	val=qRound(val*(254.0/255.0));
	displayBuffer[Width*y+x]=(int) val;
     }	
     else {
        displayBuffer[Width*y+x]=255;
      }	
    }
  }  
  streamImage = new QImage(displayBuffer, Width, Height, 8, grayTable, 256, QImage::IgnoreEndian);
  update();
}


void CCDVideoWG::paintEvent(QPaintEvent */*ev*/)
{
  	
   if (streamImage)
   {
	if (streamImage->isNull()) return;
  	//qPix = kPixIO.convertToPixmap(*streamImage);/*streamImage->smoothScale(width(), height()));*/
	qPix = kPixIO.convertToPixmap(streamImage->scale(width(), height()));
	delete (streamImage);
	streamImage = NULL;
   }
   
   bitBlt(this, 0, 0, &qPix);
   
}


void CCDPreviewWG::brightnessChanged(int value)
{
    streamFrame->offset=pow(pow(256.0,streamFrame->bytesPerPixel),1.0-(value/200.0))-1.0;
    //fprintf(stderr,"offs=%lf\n",streamFrame->offset);
    streamFrame->redrawVideoWG();	
}

void CCDPreviewWG::contrastChanged(int value)
{
    streamFrame->scale=pow(pow(256.0,streamFrame->bytesPerPixel+1),value/200.0)-1.0;
    //fprintf(stderr,"scale=%lf\n",streamFrame->scale);
    streamFrame->redrawVideoWG();
}

void CCDPreviewWG::gammaChanged(int value)
{
    streamFrame->gamma=3.0*(value/200.0);
    //fprintf(stderr,"gamma=%lf\n",streamFrame->gamma);
    streamFrame->redrawVideoWG();	
}
