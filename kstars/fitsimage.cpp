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
#include <kpixmap.h> 
#include <ktempfile.h>
#include <kimageeffect.h> 
#include <kmenubar.h>
#include <kprogress.h>
#include <kstatusbar.h>

#include <qfile.h>
#include <qvbox.h>
#include <qcursor.h>
#include <qpixmap.h>
#include <qframe.h>

#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>

#include "fitsimage.h"
#include "fitsviewer.h"
//#include "focusdialog.h" 
#include "ksutils.h"

/* Load info */
typedef struct
{
  uint replace;    /* replacement for blank/NaN-values */
  uint use_datamin;/* Use DATAMIN/MAX-scaling if possible */
  uint compose;    /* compose images with naxis==3 */
} FITSLoadVals;

static FITSLoadVals plvals =
{
  0,        /* Replace with black */
  0,        /* Do autoscale on pixel-values */
  0         /* Dont compose images */
};

FITSImage::FITSImage(QWidget * parent, const char * name) : QScrollView(parent, name), zoomFactor(1.2)
{
  viewer = (FITSViewer *) parent;
  reducedImgBuffer = NULL;
  displayImage     = NULL;
  
  imgFrame = new FITSFrame(this, viewport());
  addChild(imgFrame);
  
  currentZoom = 0.0;
  grayTable=new QRgb[256];
  for (int i=0;i<256;i++)
        grayTable[i]=qRgb(i,i,i);
  
  viewport()->setMouseTracking(true);
  imgFrame->setMouseTracking(true);
  
}

FITSImage::~FITSImage()
{
  free(reducedImgBuffer);
  delete(displayImage);
}
	
/*void FITSImage::drawContents ( QPainter * p, int clipx, int clipy, int clipw, int cliph )
{
  //kdDebug() << "in draw contents " << endl;
  //imgFrame->update();
  
}*/

/**Bitblt the image onto the viewer widget */
/*void FITSImage::paintEvent (QPaintEvent *ev)
{
 //kdDebug() << "in paint event " << endl;
 //bitBlt(imgFrame, 0, 0, &qpix);
}*/

/* Resize event */
void FITSImage::resizeEvent (QResizeEvent */*ev*/)
{
	updateScrollBars();
}

void FITSImage::contentsMouseMoveEvent ( QMouseEvent * e )
{

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
   //kdDebug() << "X= " << x << " -- Y= " << y << endl;      
   
  if (currentZoom > 0)
  {
    x /= pow(zoomFactor, currentZoom);
    y /= pow(zoomFactor, currentZoom);
  }
  else if (currentZoom < 0)
  {
    x *= pow(zoomFactor, abs((int) currentZoom));
    //kdDebug() << "The X power is " << pow(zoomFactor, abs(currentZoom)) << " -- X final = " << x << endl;
    y *= pow(zoomFactor, abs((int) currentZoom));
  }
  
  if (x < 0 || x > width)
    validPoint = false;
  
  //kdDebug() << "regular x= " << e->x() << " -- X= " << x << " -- imgFrame->x()= " << imgFrame->x() << " - displayImageWidth= " << viewer->displayImage->width() << endl;
  
  
  if (y < 0 || y > height)
    validPoint = false;
  else    
  // invert the Y since we read FITS buttom up
  y = height - y;
  
  //kdDebug() << " -- X= " << x << " -- Y= " << y << endl;
  
  if (viewer->imgBuffer == NULL)
   kdDebug() << "viewer buffer is NULL " << endl;
  
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
}

void FITSImage::viewportResizeEvent ( QResizeEvent * /*e*/)
{
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
		
}

void FITSImage::reLoadTemplateImage()
{
  *displayImage = templateImage->copy();
}

void FITSImage::saveTemplateImage()
{
  templateImage = new QImage(displayImage->copy());
}

void FITSImage::destroyTemplateImage()
{
  delete (templateImage);
}

void FITSImage::clearMem()
{

  free(reducedImgBuffer);
  delete (displayImage);
  reducedImgBuffer = NULL;
  displayImage     = NULL;

}


int FITSImage::loadFits (const char *filename)
{
 FILE *fp;
 FITS_FILE *ifp;
 FITS_HDU_LIST *hdl;
 // TODO add KStars options for transformation
 FITS_PIX_TRANSFORM trans;
 register unsigned char *dest; 
 //register unsigned char *tempBuffer;
 unsigned char *data;
 int i, j;
 double a, b;
 int err = 0;
 
 fp = fopen (filename, "rb");
 if (!fp)
 {
   KMessageBox::error(0, i18n("Cannot open file for reading"));
   return (-1);
 }
 fclose (fp);

 ifp = fits_open (filename, "r");
 if (ifp == NULL)
 {
   KMessageBox::error(0, i18n("Error during open of FITS file"));
   return (-1);
 }
 if (ifp->n_pic <= 0)
 {
   KMessageBox::error(0, i18n("FITS file keeps no displayable images"));
   fits_close (ifp);
   return (-1);
 }

 //displayImage  = new QImage();
 KProgressDialog fitsProgress(this, 0, i18n("FITS Viewer"), i18n("Loading FITS..."));
 
 hdl = fits_seek_image (ifp, 1);
 if (hdl == NULL) return (-1);

 width  = hdl->naxisn[0]; 
 height = hdl->naxisn[1]; 
 currentWidth = hdl->naxisn[0]; 
 currentHeight = hdl->naxisn[1]; 
 bitpix = hdl->bitpix;
 bpp    = hdl->bpp;
 
 imgFrame->setGeometry(0, 0, width, height);
 
 data = (unsigned char  *) malloc (height * width * sizeof(unsigned char));
 //tempBuffer = (unsigned char  *) malloc (height * width * sizeof(unsigned char));
 if (data == NULL)
 {
  KMessageBox::error(0, i18n("Not enough memory to load FITS."));
  return (-1);
 }
 
 /* If the transformation from pixel value to */
 /* data value has been specified, use it */
 if (   plvals.use_datamin
     && hdl->used.datamin && hdl->used.datamax
     && hdl->used.bzero && hdl->used.bscale)
 {
   a = (hdl->datamin - hdl->bzero) / hdl->bscale;
   b = (hdl->datamax - hdl->bzero) / hdl->bscale;
   if (a < b) trans.pixmin = a, trans.pixmax = b;
   else trans.pixmin = b, trans.pixmax = a;
 }
 else
 {
   trans.pixmin = hdl->pixmin;
   trans.pixmax = hdl->pixmax;
 }
 trans.datamin = 0.0;
 trans.datamax = 255.0;
 trans.replacement = plvals.replace;
 trans.dsttyp = 'c';
 
 //displayImage->create(width, height, 32); 
 currentRect.setX(0);
 currentRect.setY(0);
 currentRect.setWidth(width);
 currentRect.setHeight(height);
 
 fitsProgress.progressBar()->setTotalSteps(height);
 fitsProgress.setMinimumWidth(300);
 fitsProgress.show();

 delete (displayImage);
 
 displayImage = new QImage(width, height, 8, 256, QImage::IgnoreEndian);
 for (int i=0; i < 256; i++)
   displayImage->setColor(i, grayTable[i]);
//displayImage = new QImage();
//displayImage->create(width, height, 32);


 /* FITS stores images with bottom row first. Therefore we have */
 /* to fill the image from bottom to top. */
   dest = data + height * width;
   
   for (i = height - 1; i >= 0 ; i--)
   {
     /* Read FITS line */
     dest -= width;
     if (fits_read_pixel (ifp, hdl, width, &trans, dest) != width)
     {
       err = 1;
       break;
     }
     
     //for (j=0; j < width; j++)
     //{
       //val = dest[j];
       //displayImage->setPixel(j, i, qRgb(val, val, val));
     //}
       for (j = 0 ; j < width; j++)
         displayImage->setPixel(j, i, dest[j]);
      
      fitsProgress.progressBar()->setProgress(height - i);
   }
 
 reducedImgBuffer = data;
 convertImageToPixmap();
 viewportResizeEvent(NULL);
 
 if (err)
   KMessageBox::error(0, i18n("EOF encountered on reading."));

 fits_record_list * FRList = hdl->header_record_list;
 while (FRList != NULL)
 {
   viewer->record << QString((char *) FRList->data); 
   FRList = FRList->next_record;
 }
 //memcpy(viewer->record, hdl->header_record_list->data , FITS_RECORD_SIZE);
 
 fits_close(ifp);
 return (err ? -1 : 0);
}

void FITSImage::convertImageToPixmap()
{
    qpix = kpix.convertToPixmap ( *(displayImage) );
}

void FITSImage::zoomToCurrent()
{

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
 	qpix = kpix.convertToPixmap (displayImage->smoothScale( (int) cwidth, (int) cheight));
        //imgFrame->resize( (int) width, (int) height);
        viewportResizeEvent (NULL);
	imgFrame->update();
 }
 else
 {
   qpix = kpix.convertToPixmap ( *displayImage );
   imgFrame->update();
  }
 
}


void FITSImage::fitsZoomIn()
{
   currentZoom++;
   viewer->actionCollection()->action("view_zoom_out")->setEnabled (true);
   if (currentZoom > 5)
     viewer->actionCollection()->action("view_zoom_in")->setEnabled (false);
   
   currentWidth  *= zoomFactor; //pow(zoomFactor, abs(currentZoom)) ;
   currentHeight *= zoomFactor; //pow(zoomFactor, abs(currentZoom));

   //kdDebug() << "Current width= " << currentWidth << " -- Current height= " << currentHeight << endl;
   
   qpix = kpix.convertToPixmap (displayImage->smoothScale( (int) currentWidth, (int) currentHeight));
   imgFrame->resize( (int) currentWidth, (int) currentHeight);

   update();
   viewportResizeEvent (NULL);  
   //updateScrollBars();
}

void FITSImage::fitsZoomOut()
{
  currentZoom--;
  if (currentZoom < -5)
     viewer->actionCollection()->action("view_zoom_out")->setEnabled (false);
  viewer->actionCollection()->action("view_zoom_in")->setEnabled (true);
  
  currentWidth  /= zoomFactor; //pow(zoomFactor, abs(currentZoom));
  currentHeight /= zoomFactor;//pow(zoomFactor, abs(currentZoom));
  
  qpix = kpix.convertToPixmap (displayImage->smoothScale( (int) currentWidth, (int) currentHeight));
  imgFrame->resize( (int) currentWidth, (int) currentHeight);
   
  update();
  viewportResizeEvent (NULL);
  //updateScrollBars();
}

void FITSImage::fitsZoomDefault()
{

  viewer->actionCollection()->action("view_zoom_out")->setEnabled (true);
  viewer->actionCollection()->action("view_zoom_in")->setEnabled (true);
  
  currentZoom   = 0;
  currentWidth  = width;
  currentHeight = height;
  
  qpix = kpix.convertToPixmap (*displayImage);
  imgFrame->resize( (int) currentWidth, (int) currentHeight);
  
  update();
  viewportResizeEvent (NULL);
  //updateScrollBars();

}

FITSFrame::FITSFrame(FITSImage * img, QWidget * parent, const char * name) : QFrame(parent, name, Qt::WNoAutoErase)
{
  image = img;
  setPaletteBackgroundColor(image->viewport()->paletteBackgroundColor());
}

FITSFrame::~FITSFrame() {}

void FITSFrame::paintEvent(QPaintEvent * /*e*/)
{
 
 bitBlt(this, 20, 20, &(image->qpix));
 resize( (int) (image->currentWidth + 40), (int) (image->currentHeight + 40));

}



#include "fitsimage.moc"
