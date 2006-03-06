/***************************************************************************
                          FITSImage.cpp  -  FITS Image
                             -------------------
    begin                : Thu Jan 22 2004
    copyright            : (C) 2004 by Jasem Mutlaq
    email                : mutlaqja@ikarustech.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Some code fragments were adapted from Peter Kirchgessner's FITS plugin*
 *   See http://members.aol.com/pkirchg for more details.                  *
 ***************************************************************************/


#include <klocale.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <kaction.h>
#include <kaccel.h>
#include <kdebug.h>
#include <ktoolbar.h> 
#include <kapplication.h>

#include <ktempfile.h>
#include <kimageeffect.h> 
#include <kmenubar.h>
#include <kprogressdialog.h>
#include <kstatusbar.h>

#include <QPaintEvent>
#include <QScrollArea>

#include <qfile.h>
#include <qcursor.h>


#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>

#include "fitsimage.h"
#include "fitsviewer.h"
//#include "focusdialog.h" 
#include "ksutils.h"



FITSImage::FITSImage(QWidget * parent, const char * name) : QScrollArea(parent), zoomFactor(1.2)
{
  viewer = (FITSViewer *) parent;
  
  /* FIXME enable this when Qt 4.2 is released 
  setAlignment(Qt::AlignCenter);
 */

   image_frame = new QLabel;
   image_buffer = NULL;
   displayImage = NULL;
   setBackgroundRole(QPalette::Dark);

   currentZoom = 0.0;
   viewport()->setMouseTracking(true);
}

FITSImage::~FITSImage()
{
 // free(reducedImgBuffer);
  delete(displayImage);
}
	
/*void FITSImage::drawContents ( QPainter * p, int clipx, int clipy, int clipw, int cliph )
{
  //kDebug() << "in draw contents " << endl;
  //imgFrame->update();
  
}*/

/**Bitblt the image onto the viewer widget */
/*void FITSImage::paintEvent (QPaintEvent *ev)
{
 //kDebug() << "in paint event " << endl;
 //bitBlt(imgFrame, 0, 0, &image_pixmap);
}*/

/* Resize event */
void FITSImage::resizeEvent (QResizeEvent */*ev*/)
{
	//updateScrollBars();
}

void FITSImage::contentsMouseMoveEvent ( QMouseEvent * e )
{
 #if 0
  double x,y;
  bool validPoint = true;
  if (!displayImage) return;
  
  
   x = e->x();
   y = e->y();
  
  if (imgFrame->x() > 0)
    x -= imgFrame->x();
  
  if (imgFrame->y() > 0)
    y -= imgFrame->y();
  
    y -= 20;
    x -= 20;
   //kDebug() << "X= " << x << " -- Y= " << y << endl;      
   
  if (currentZoom > 0)
  {
    x /= pow(zoomFactor, currentZoom);
    y /= pow(zoomFactor, currentZoom);
  }
  else if (currentZoom < 0)
  {
    x *= pow(zoomFactor, abs((int) currentZoom));
    //kDebug() << "The X power is " << pow(zoomFactor, abs(currentZoom)) << " -- X final = " << x << endl;
    y *= pow(zoomFactor, abs((int) currentZoom));
  }
  
  if (x < 0 || x > width)
    validPoint = false;
  
  //kDebug() << "regular x= " << e->x() << " -- X= " << x << " -- imgFrame->x()= " << imgFrame->x() << " - displayImageWidth= " << viewer->displayImage->width() << endl;
  
  
  if (y < 0 || y > height)
    validPoint = false;
  else    
  // invert the Y since we read FITS buttom up
  y = height - y;
  
  //kDebug() << " -- X= " << x << " -- Y= " << y << endl;
  
  if (viewer->imgBuffer == NULL)
   kDebug() << "viewer buffer is NULL " << endl;
  
  if (validPoint)
  {
  viewer->statusBar()->changeItem(QString("%1 , %2").arg( (int) x).arg( (int) y), 0);
	viewer->statusBar()->changeItem( KGlobal::locale()->formatNumber( viewer->imgBuffer[(int) (y * width + x)], 3 ), 1 );
  setCursor(Qt::CrossCursor);
  }
  else
  {
  //viewer->statusBar()->changeItem(QString("(X,Y)"), 0);
  setCursor(Qt::ArrowCursor);
  }
 #endif
}

void FITSImage::viewportResizeEvent ( QResizeEvent * /*e*/)
{
 #if 0
        int w, h, conW, conH, x, y;
	if (!displayImage) return;
	
	w = viewport()->width();
        h = viewport()->height();
	
	conW = (int) (currentWidth  + 40);
        conH = (int) (currentHeight + 40);
	
	if ( w > conW )
	   x = (int) ( (w - conW) / 2.);
        else
           x = 0;
	if ( h > conH )
          y = (int) ( (h - conH) / 2.);
       else
          y = 0;
	
	// do new movement
        moveChild( imgFrame, x, y );
	
 #endif	
}

void FITSImage::reLoadTemplateImage()
{
  /*displayImage = templateImage->copy(); */
}

void FITSImage::saveTemplateImage()
{
  //templateImage = new QImage(displayImage->copy());
}

void FITSImage::destroyTemplateImage()
{
  //delete (templateImage);
}

void FITSImage::clearMem()
{

 #if 0
  free(reducedImgBuffer);
  delete (displayImage);
  reducedImgBuffer = NULL;
  displayImage     = NULL;

 #endif

}


int FITSImage::loadFits (const char *filename)
{
 
  int status=0, nulval=0, anynull=0;
  long fpixel[2], nelements, naxes[2];
  double bscale, bzero;
  float val=0;
  fitsfile* fptr;

  if (fits_open_image(&fptr, filename, READWRITE, &status))
  {
	fits_report_error(stderr, status);
	return -1;
  }

  if (fits_get_img_param(fptr, 2, &(stats.bitpix), &(stats.ndim), naxes, &status))
  {
	fits_report_error(stderr, status);
	return -1;
  }

  stats.dim[0] = naxes[0];
  stats.dim[1] = naxes[1];

  kDebug() << "bitpix: " << stats.bitpix << " dim[0]: " << stats.dim[0] << " dim[1]: " << stats.dim[1] << " ndim: " << stats.ndim << endl;

  // Get Min Max failed, scaling is not possible
  if (getMinMax(fptr))
    return -1;

  bscale = 255. / (stats.max - stats.min);
  bzero  = (-stats.min) * (255. / (stats.max - stats.min));

  delete (image_buffer);
  delete (displayImage);

  image_buffer = new float[stats.dim[0] * stats.dim[1]];
  
  displayImage = new QImage(stats.dim[0], stats.dim[1], QImage::Format_Indexed8);

  displayImage->setNumColors(256);
 
  for (int i=0; i < 256; i++)
     displayImage->setColor(i, qRgb(i,i,i));

 nelements = stats.dim[0] * stats.dim[1];
 //fpixel = new long[2];
 fpixel[0] = 1;
 fpixel[1] = 1;

 if (fits_read_pix(fptr, TFLOAT, fpixel, nelements, &nulval, image_buffer, &anynull, &status))
 {
	fits_report_error(stderr, status);
	return -1;
 }

    /* Fill in pixel values using indexed map */
    for (int j = 0; j < stats.dim[1]; j++)
        for (int i = 0; i < stats.dim[0]; i++)
	{
		val = image_buffer[j * stats.dim[0] + i];
		//displayImage->setPixel(i, j, ((int) image_buffer[j * stats.dim[0] + i]));
		displayImage->setPixel(i, j, ((int) (val * bscale + bzero)));
	}
	
 image_pixmap = QPixmap::fromImage(*displayImage);
 image_frame->setPixmap(image_pixmap);
 setWidget(image_frame);
 
 return 0;
}

int FITSImage::getMinMax( fitsfile *fptr )
{
           /* pointer to the FITS file, defined in fitsio.h */
    int status,  anynull, nfound=0;
    long fpixel, nbuffer, npixels, ii;
    float nullval, buffer[1000];

    status = 0;

  if (fits_read_key_dbl(fptr, "DATAMIN", &(stats.min), NULL, &status))
  	fits_report_error(stderr, status);
  else
	nfound++;


  if (fits_read_key_dbl(fptr, "DATAMAX", &(stats.max), NULL, &status))
  	fits_report_error(stderr, status);
  else
	nfound++;
  
  // If we found both keywords, no need to calculate them
  if (nfound == 2)
	return 0;

    npixels  = stats.dim[0] * stats.dim[1];         /* number of pixels in the image */
    fpixel   = 1;
    nullval  = 0;                /* don't check for null values in the image */
    stats.min  = 1.0E30;
    stats.max  = -1.0E30;

    while (npixels > 0)
    {
      nbuffer = npixels;
      if (npixels > 1000)
        nbuffer = 1000;     /* read as many pixels as will fit in buffer, snippet from CFITSIO example */

      if ( fits_read_img(fptr, TFLOAT, fpixel, nbuffer, &nullval, buffer, &anynull, &status) )
      {
            fits_report_error(stderr, status);
	    return status;
      }

      for (ii = 0; ii < nbuffer; ii++)
      {
        if ( buffer[ii] < stats.min )
            stats.min = buffer[ii];

        if ( buffer[ii] > stats.max )
            stats.max = buffer[ii];
      }
      npixels -= nbuffer;    /* increment remaining number of pixels */
      fpixel  += nbuffer;    /* next pixel to be read in image */
    }

    kDebug() << "DATAMIN: " << stats.min << " - DATAMAX: " << stats.max << endl;
    return 0;
}

void FITSImage::convertImageToPixmap()
{
    //FIXME commented out next line to build as KPixmapIO is gone -- annma 1006-02-20
    //image_pixmap = kpix.convertToPixmap ( *(displayImage) );
}

void FITSImage::zoomToCurrent()
{

 #if 0

 double cwidth, cheight;
 
 if (currentZoom >= 0)
 {
   cwidth  = ((double) displayImage->width()) * pow(zoomFactor, currentZoom) ;
   cheight = ((double) displayImage->height()) * pow(zoomFactor, currentZoom);
 }
 else
 { 
   cwidth  = ((double) displayImage->width()) / pow(zoomFactor, abs((int) currentZoom)) ;
   cheight = ((double) displayImage->height()) / pow(zoomFactor, abs((int) currentZoom));
 }
  
 if (cwidth != displayImage->width() || cheight != displayImage->height())
 {
	//FIXME commented out next line to build as KPixmapIO is gone -- annma 1006-02-20
 	//image_pixmap = kpix.convertToPixmap (displayImage->smoothScale( (int) cwidth, (int) cheight));
        //imgFrame->resize( (int) width, (int) height);
        viewportResizeEvent (NULL);
	imgFrame->update();
 }
 else
 {
   //FIXME commented out next line to build as KPixmapIO is gone -- annma 1006-02-20
   //image_pixmap = kpix.convertToPixmap ( *displayImage );
   imgFrame->update();
  }

 #endif
 
}


void FITSImage::fitsZoomIn()
{
 #if 0
   currentZoom++;
   viewer->actionCollection()->action("view_zoom_out")->setEnabled (true);
   if (currentZoom > 5)
     viewer->actionCollection()->action("view_zoom_in")->setEnabled (false);
   
   currentWidth  *= zoomFactor; //pow(zoomFactor, abs(currentZoom)) ;
   currentHeight *= zoomFactor; //pow(zoomFactor, abs(currentZoom));

   //kDebug() << "Current width= " << currentWidth << " -- Current height= " << currentHeight << endl;
   //FIXME commented out next line to build as KPixmapIO is gone -- annma 1006-02-20
   //image_pixmap = kpix.convertToPixmap (displayImage->smoothScale( (int) currentWidth, (int) currentHeight));
   imgFrame->resize( (int) currentWidth, (int) currentHeight);

   update();
   viewportResizeEvent (NULL);  
   //updateScrollBars();

 #endif
}

void FITSImage::fitsZoomOut()
{
 #if 0
  currentZoom--;
  if (currentZoom < -5)
     viewer->actionCollection()->action("view_zoom_out")->setEnabled (false);
  viewer->actionCollection()->action("view_zoom_in")->setEnabled (true);
  
  currentWidth  /= zoomFactor; //pow(zoomFactor, abs(currentZoom));
  currentHeight /= zoomFactor;//pow(zoomFactor, abs(currentZoom));
  //FIXME commented out next line to build as KPixmapIO is gone -- annma 1006-02-20
  //image_pixmap = kpix.convertToPixmap (displayImage->smoothScale( (int) currentWidth, (int) currentHeight));
  imgFrame->resize( (int) currentWidth, (int) currentHeight);
   
  update();
  viewportResizeEvent (NULL);
  //updateScrollBars();

 #endif
}

void FITSImage::fitsZoomDefault()
{
  #if 0
  viewer->actionCollection()->action("view_zoom_out")->setEnabled (true);
  viewer->actionCollection()->action("view_zoom_in")->setEnabled (true);
  
  currentZoom   = 0;
  currentWidth  = width;
  currentHeight = height;
  //FIXME commented out next line to build as KPixmapIO is gone -- annma 1006-02-20
  //image_pixmap = kpix.convertToPixmap (*displayImage);
  imgFrame->resize( (int) currentWidth, (int) currentHeight);
  
  update();
  viewportResizeEvent (NULL);
  //updateScrollBars();
#endif
}

#if 0

FITSFrame::FITSFrame(FITSImage * img, QWidget * parent, const char * name) : QFrame(parent, name)//, Qt::WNoAutoErase)
{
  image = img;
  //setPaletteBackgroundColor(image->viewport()->paletteBackgroundColor());
QLabel *imageLabel = new QLabel(this);
    imageLabel->setText("Hello this is some label");

}

FITSFrame::~FITSFrame() {}

#if 0
void FITSFrame::paintEvent(QPaintEvent * /*e*/)
{
 
 bitBlt(this, 20, 20, &(image->image_pixmap));
 resize( (int) (image->currentWidth + 40), (int) (image->currentHeight + 40));
 kDebug() << "in paint event for FITS Frame" << endl;

}
#endif

#endif

#include "fitsimage.moc"
