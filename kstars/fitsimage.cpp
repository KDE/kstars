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

FITSImage::FITSImage(QWidget * parent, const char * name) : QScrollView(parent, name)
{
  viewer = (FITSViewer *) parent;
  reducedImgBuffer = NULL;
  currentImage     = NULL;
  
  imgFrame = new QFrame(viewport());
  addChild(imgFrame);
  
  viewport()->setMouseTracking(true);
  imgFrame->setMouseTracking(true);
  
}

FITSImage::~FITSImage()
{
  free(reducedImgBuffer);
}
	
void FITSImage::drawContents ( QPainter * p, int clipx, int clipy, int clipw, int cliph )
{
  //kdDebug() << "in draw contents " << endl;
  bitBlt(imgFrame, 0, 0, &qpix);
}

/**Bitblt the image onto the viewer widget */
void FITSImage::paintEvent (QPaintEvent *ev)
{
// kdDebug() << "in paint event " << endl;
 bitBlt(imgFrame, 0, 0, &qpix);
}

/* Resize event */
void FITSImage::resizeEvent (QResizeEvent *ev)
{
	updateScrollBars();
}

void FITSImage::contentsMouseMoveEvent ( QMouseEvent * e )
{

  int x,y;
  bool validPoint = true;
  if (!currentImage) return;
  
  if (imgFrame->x() > 0)
    x = e->x() - imgFrame->x();
  else
    x = e->x();
  
  if (x < 0 || x > currentImage->width())
    validPoint = false;
  
  //kdDebug() << "regular x= " << e->x() << " -- X= " << x << " -- imgFrame->x()= " << imgFrame->x() << " - currentImageWidth= " << viewer->currentImage->width() << endl;
  if (imgFrame->y() > 0)
    y = e->y() - imgFrame->y();
  else
    y = e->y();
  if (y < 0 || y > currentImage->height())
    validPoint = false;
  else    
  // invert the Y since we read FITS buttom up
  y = currentImage->height() - y;
  
  //kdDebug() << "X= " << x << " -- Y= " << y << endl;
  
  if (viewer->imgBuffer == NULL)
   kdDebug() << "viewer buffer is NULL " << endl;
  
  if (validPoint)
  {
  viewer->statusBar()->changeItem(QString("(%1,%2) Value= %3").arg(x,4).arg(y,-4).arg(viewer->imgBuffer[y * imgFrame->width() + x], -6), 0);
  setCursor(Qt::CrossCursor);
  }
  else
  {
  viewer->statusBar()->changeItem(QString("(X,Y)"), 0);
  setCursor(Qt::ArrowCursor);
  }
}

void FITSImage::viewportResizeEvent ( QResizeEvent * e)
{
        int w, h, x, y;
	if (!currentImage) return;
	
	w = viewport()->width();
        h = viewport()->height();
        if ( w > contentsWidth() )
           x = (int) ( (w - contentsWidth()) / 2.);
        else
           x = 0;
        if ( h > contentsHeight() )
          y = (int) ( (h - contentsHeight()) / 2.);
       else
          y = 0;
	 
	//kdDebug() << "X= " << x << " -- Y= " << y << endl;
        moveChild( imgFrame, x, y );

}

void FITSImage::reLoadTemplateImage()
{
  *currentImage = templateImage->copy();
}

void FITSImage::saveTemplateImage()
{
  templateImage = new QImage(currentImage->copy());
  //*templateImage = currentImage->copy();
}

void FITSImage::destroyTemplateImage()
{
  delete (templateImage);
}

void FITSImage::rescale()
{
  int val;
  
  viewer->calculateStats();
  
  for (int i=0; i < height; i++)
      for (int j=0; j < width; j++)
  {
      val = (int) (255. * ( (double) (viewer->imgBuffer[i * width + j] - viewer->stats.min) / (double) (viewer->stats.max - viewer->stats.min)));
      currentImage->setPixel(j, height - i - 1, qRgb(val, val, val));
  }
  
  convertImageToPixmap();
  update();
}


int FITSImage::loadFits (const char *filename)
{
 FILE *fp;
 FITS_FILE *ifp;
 FITS_HDU_LIST *hdulist;
 FITS_PIX_TRANSFORM trans;
 register unsigned char *dest; 
 unsigned char *data;
 int i, j, val;
 double a, b;
 int err = 0;
 
 fp = fopen (filename, "rb");
 if (!fp)
 {
   KMessageBox::error(0, i18n("Can't open file for reading"));
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

 currentImage = new QImage();
 KProgressDialog fitsProgress(this, 0, i18n("FITS Viewer"), i18n("Loading FITS..."));
 
 hdulist = fits_seek_image (ifp, 1);
 if (hdulist == NULL) return (-1);

 width  = hdulist->naxisn[0]; 
 height = hdulist->naxisn[1];
 bitpix = hdulist->bitpix;
 bpp    = hdulist->bpp;
 
 imgFrame->setGeometry(0, 0, width, height);
 
 data = (unsigned char  *) malloc (height * width * sizeof(unsigned char));
 if (data == NULL)
 {
  KMessageBox::error(0, i18n("Not enough memory to load FITS."));
  return (-1);
 }
 
 /* If the transformation from pixel value to */
 /* data value has been specified, use it */
 if (   plvals.use_datamin
     && hdulist->used.datamin && hdulist->used.datamax
     && hdulist->used.bzero && hdulist->used.bscale)
 {
   a = (hdulist->datamin - hdulist->bzero) / hdulist->bscale;
   b = (hdulist->datamax - hdulist->bzero) / hdulist->bscale;
   if (a < b) trans.pixmin = a, trans.pixmax = b;
   else trans.pixmin = b, trans.pixmax = a;
 }
 else
 {
   trans.pixmin = hdulist->pixmin;
   trans.pixmax = hdulist->pixmax;
 }
 trans.datamin = 0.0;
 trans.datamax = 255.0;
 trans.replacement = plvals.replace;
 trans.dsttyp = 'c';

 currentImage->create(width, height, 32);
 
 currentRect.setX(0);
 currentRect.setY(0);
 currentRect.setWidth(width);
 currentRect.setHeight(height);
 
 fitsProgress.progressBar()->setTotalSteps(height);
 fitsProgress.setMinimumWidth(300);
 fitsProgress.show();

 /* FITS stores images with bottom row first. Therefore we have */
 /* to fill the image from bottom to top. */
   dest = data + height * width;
   
   for (i = height - 1; i >= 0 ; i--)
   {
     /* Read FITS line */
     dest -= width;
     if (fits_read_pixel (ifp, hdulist, width, &trans, dest) != width)
     {
       err = 1;
       break;
     }
     
       for (j = 0 ; j < width; j++)
       {
       val = dest[j];
       currentImage->setPixel(j, i, qRgb(val, val, val));
      }
      
      fitsProgress.progressBar()->setProgress(height - i);
   }
 
 reducedImgBuffer = data;
 convertImageToPixmap();
 
 if (err)
   KMessageBox::error(0, i18n("EOF encountered on reading."));
 
 return (err ? -1 : 0);
}

void FITSImage::convertImageToPixmap()
{
    qpix = kpix.convertToPixmap ( *(currentImage) );
}
