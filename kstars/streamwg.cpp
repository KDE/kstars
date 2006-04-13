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
#include <QResizeEvent>
#include <QPaintEvent>
#include <QCloseEvent>
#include <QByteArray>
#include <QImageWriter>

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#define STREAMBUFSIZ		1024

FILE *wfp;

/*StreamWGUI::StreamWGUI(QWidget *parent) : QFrame (parent)
{

 setupUi(parent);

 foreach (QByteArray format, QImageWriter::supportedImageFormats())
     imgFormatCombo->addItem(QString(format));

}*/
  
 StreamWG::StreamWG(INDIStdDevice *inStdDev, QWidget * parent) : QWidget(parent)
 {
 
   //QFrame *page   = plainPage();
   //ui		  = new StreamWGUI(page);
   setupUi(this);
   stdDev         = inStdDev;
   streamWidth    = streamHeight = -1;
   processStream  = colorFrame = false;

   streamFrame      = new VideoWG(videoFrame);
      
  KIconLoader *icons = KGlobal::iconLoader();
  
  playPix    = icons->loadIcon( "player_play", K3Icon::Toolbar );
  pausePix   = icons->loadIcon( "player_pause", K3Icon::Toolbar );
  capturePix = icons->loadIcon( "frame_image", K3Icon::Toolbar );
  
  foreach (QByteArray format, QImageWriter::supportedImageFormats())
     imgFormatCombo->addItem(QString(format));

  playB->setPixmap(pausePix);	
  captureB->setPixmap(capturePix);
  
  connect(playB, SIGNAL(clicked()), this, SLOT(playPressed()));
  connect(captureB, SIGNAL(clicked()), this, SLOT(captureImage()));

  //videoFrame->resize(640, 480);

  //kDebug() << "Video Frame Width " << videoFrame->width() << endl;
   
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
  
  resize(wd + layout()->margin() * 2 , ht + playB->height() + layout()->margin() * 4 + layout()->spacing());  
  streamFrame->resize(wd, ht);
}

void StreamWG::resizeEvent(QResizeEvent *ev)
{
  streamFrame->resize(ev->size().width() - layout()->margin() * 2, ev->size().height() - playB->height() - layout()->margin() * 4 - layout()->spacing());

}

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
  KUrl currentFileURL;
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
	  
	streamFrame->kPix.save(fname, fmt.ascii());
	
	//set rwx for owner, rx for group, rx for other
	chmod( fname.ascii(), S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH );

	if ( tmpfile.name() == fname )
	{ //need to upload to remote location
	
	  if ( ! KIO::NetAccess::upload( tmpfile.name(), currentFileURL, (QWidget*) 0 ) )
	  {
		QString message = i18n( "Could not upload image to remote location: %1", currentFileURL.prettyURL() );
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
	kPix.convertFromImage(streamImage->scaled(width(), height(), Qt::KeepAspectRatio));
	delete (streamImage);
	streamImage = NULL;
   }
   
   bitBlt(this, 0, 0, &kPix);
   
}

#include "streamwg.moc"
