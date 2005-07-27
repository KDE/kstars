/*  Stream Widget
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
    
    2004-03-16: A class to handle video streaming.
 */
 
#include "streamwg.h"
#include "indistd.h"
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

#include <qsocketnotifier.h>
#include <qimage.h>
#include <qpainter.h>
#include <qstringlist.h>
#include <qdir.h>
#include <qlayout.h>


#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#define STREAMBUFSIZ		1024

FILE *wfp;

 StreamWG::StreamWG(INDIStdDevice *inStdDev, QWidget * parent, const char * name) : streamForm(parent, name)
 {
 
   stdDev         = inStdDev;
//   streamBuffer   = NULL;
   streamWidth    = streamHeight = -1;
//   streamFD       = -1;
   processStream  = colorFrame = false;
   
   //videoFrameLayout = new QVBoxLayout(videoFrame, 0, 0); 
   streamFrame      = new VideoWG(videoFrame);
      
  KIconLoader *icons = KGlobal::iconLoader();
  
  playPix    = icons->loadIcon( "player_play", KIcon::Toolbar );
  pausePix   = icons->loadIcon( "player_pause", KIcon::Toolbar );
  capturePix = icons->loadIcon( "frame_image", KIcon::Toolbar );
  
  playB->setPixmap(pausePix);	
  captureB->setPixmap(capturePix);
  
  imgFormatCombo->insertStrList(QImage::outputFormats());
  
  connect(playB, SIGNAL(clicked()), this, SLOT(playPressed()));
  connect(captureB, SIGNAL(clicked()), this, SLOT(captureImage()));
   
 }
 
StreamWG::~StreamWG()
{
//  delete streamBuffer;
}

void StreamWG::closeEvent ( QCloseEvent * e )
{
  stdDev->streamDisabled();
  processStream = false;
  e->accept();
}

void StreamWG::setColorFrame(bool color)
{
  colorFrame = color;
}

/*void StreamWG::establishDataChannel(QString host, int port)
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

void StreamWG::enableStream(bool enable)
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

void StreamWG::setSize(int wd, int ht)
{
  streamWidth  = wd;
  streamHeight = ht;
  
  streamFrame->totalBaseCount = wd * ht;
  
  resize(wd + layout()->margin() * 2 , ht + playB->height() + layout()->margin() * 2 + layout()->spacing());  
  streamFrame->resize(wd, ht);
}

void StreamWG::resizeEvent(QResizeEvent *ev)
{

  streamFrame->resize(ev->size().width() - layout()->margin() * 2, ev->size().height() - playB->height() - layout()->margin() * 2 - layout()->spacing());

}
 /*
void StreamWG::allocateStreamBuffer()
{
  if (streamWidth < 0 || streamHeight < 0) return;
  
  fprintf(stderr, "In allocate stream buffer \n");
  
  delete (streamBuffer);
  
  if (colorFrame)
    frameTotalBytes = streamWidth * streamHeight * 4;
  else
    frameTotalBytes = streamWidth * streamHeight;
  
   streamBuffer = new unsigned char[frameTotalBytes];
   
   fprintf(stderr, "We have a %s frame. width: %d -- height: %d -- totalBytes: %d\n", colorFrame ? "color" : "grey", streamWidth, streamHeight, frameTotalBytes);
  
}

void StreamWG::streamReceived()
{

	char msg[1024];
	int nr=0, n=0;
	
	for (nr = 0; nr < frameTotalBytes; nr+=n)
	{
           n = read (streamFD, streamBuffer + nr, frameTotalBytes - nr);
	   if (n <= 0)
	   {
	    	if (n < 0)
			sprintf (msg, "INDI: input error.");
	    	else
			sprintf (msg, "INDI: agent closed connection.");

	    stdDev->sNotifier->disconnect();
	    close(stdDev->streamFD);
	    KMessageBox::error(0, QString(msg));
            return;
	   }
	}
	
	if (!processStream)
	  return;
	
	streamFrame->newFrame(streamBuffer, streamWidth, streamHeight, colorFrame);

}*/

void StreamWG::playPressed()
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

void StreamWG::captureImage()
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


VideoWG::VideoWG(QWidget * parent, const char * name) : QFrame(parent, name, Qt::WNoAutoErase)
{
  streamImage    = NULL;
  grayTable=new QRgb[256];
  for (int i=0;i<256;i++)
        grayTable[i]=qRgb(i,i,i);
}
      
VideoWG::~VideoWG() 
{
 delete (streamImage);
 delete [] (grayTable);
}

void VideoWG::newFrame(unsigned char *buffer, int buffSiz, int w, int h)
{
   //delete (streamImage);
   //streamImage = NULL;
  
  //if (color)
  if (buffSiz > totalBaseCount)
     streamImage = new QImage(buffer, w, h, 32, 0, 0, QImage::BigEndian);
   else
   
    streamImage = new QImage(buffer, w, h, 8, grayTable, 256, QImage::IgnoreEndian);
    
   update();
    
}

void VideoWG::paintEvent(QPaintEvent */*ev*/)
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

#include "streamwg.moc"
