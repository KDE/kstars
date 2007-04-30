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
#include <ktemporaryfile.h>
#include <kio/netaccess.h>
#include <kfiledialog.h>
#include <kcombobox.h>
#include <kurl.h>
#include <klineedit.h>

#include <QImage>
#include <QPainter>
#include <QStringList>
#include <QDir>
#include <QLayout>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QCloseEvent>
#include <QImageWriter>


#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>


#define STREAMBUFSIZ		1024

#include "ccdpreviewwg.moc"

FILE *CCDwfp;

/*CCDPreviewWGUI::CCDPreviewWGUI( QWidget *parent ) : QFrame( parent )
{
   setupUi(parent);

  foreach (QByteArray format, QImageWriter::supportedImageFormats())
     imgFormatCombo->addItem(QString(format));
}*/

 CCDPreviewWG::CCDPreviewWG(INDIStdDevice *inStdDev, QWidget * parent) : QWidget(parent)
 {
 
   //ui = new CCDPreviewWGUI(this);

   setupUi(this);
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

   playPix    = KIcon( "media-playback-start" );
   pausePix   = KIcon( "media-playback-pause" );
   capturePix = KIcon( "frame_image" );
  
  playB->setIcon(pausePix);	
  captureB->setIcon(capturePix);
  
  connect(playB, SIGNAL(clicked()), this, SLOT(playPressed()));
  connect(captureB, SIGNAL(clicked()), this, SLOT(captureImage()));
  connect(brightnessBar, SIGNAL(vsalueChanged(int)), this, SLOT(brightnessChanged(int)));
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
    playB->setIcon(pausePix);
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
  playB->setIcon(playPix);	
  processStream = false;
 }
 else
 {
  playB->setIcon(pausePix);	
  processStream = true;
 }
 
}

void CCDPreviewWG::captureImage()
{
  QString fname;
  QString fmt;
  KUrl currentFileURL;
  QString currentDir = Options::fitsSaveDirectory();
  KTemporaryFile tmpfile;
  tmpfile.open();

  fmt = imgFormatCombo->currentText();

  currentFileURL = KFileDialog::getSaveUrl( currentDir, fmt );
  
  if (currentFileURL.isEmpty()) return;

  if ( currentFileURL.isValid() )
  {
	currentDir = currentFileURL.directory();

	if ( currentFileURL.isLocalFile() )
  	   fname = currentFileURL.path();
	else
	   fname = tmpfile.fileName();

	if (fname.right(fmt.length()).toLower() != fmt.toLower()) 
	{
	  fname += '.';
	  fname += fmt.toLower();
	}
	  
	streamFrame->kPix.save(fname, fmt.toAscii());
	
	//set rwx for owner, rx for group, rx for other
	chmod( fname.toAscii(), S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH );

	if ( tmpfile.fileName() == fname )
	{ //need to upload to remote location
	  if ( ! KIO::NetAccess::upload( tmpfile.fileName(), currentFileURL, (QWidget*) 0 ) )
	  {
		QString message = i18n( "Could not upload image to remote location: %1", currentFileURL.prettyUrl() );
		KMessageBox::sorry( 0, message, i18n( "Could not upload file" ) );
	  }
	}
  }
  else
  {
		QString message = i18n( "Invalid URL: %1", currentFileURL.url() );
		KMessageBox::sorry( 0, message, i18n( "Invalid URL" ) );
  }

}


CCDVideoWG::CCDVideoWG(QWidget * parent) : QFrame(parent)
{
  setObjectName(QLatin1String("CCDPreviewFrame"));
  setAttribute(Qt::WA_OpaquePaintEvent);
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
	val=round(val*(254.0/255.0));
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
	kPix = QPixmap::fromImage(streamImage->scaled(width(), height(), Qt::KeepAspectRatio));
	delete (streamImage);
	streamImage = NULL;
   }
   
   QPainter p(this);
   p.drawPixmap(0, 0, kPix);
   p.end();
   
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
