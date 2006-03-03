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

#include <qfile.h>
#include <q3vbox.h>
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
#if 0
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
#endif

FITSImage::FITSImage(QWidget * parent, const char * name) : QScrollArea(parent), zoomFactor(1.2)
{
  viewer = (FITSViewer *) parent;
//  reducedImgBuffer = NULL;
  displayImage     = NULL;
  
  image_frame = new FITSFrame(this, NULL);//viewport());

  setBackgroundRole(QPalette::Dark);
  setWidget(image_frame);

  //addChild(imgFrame);
  
  currentZoom = 0.0;
  //grayTable=new QRgb[256];
  //for (int i=0;i<256;i++)
     //   grayTable[i]=qRgb(i,i,i);
  
  viewport()->setMouseTracking(true);
  image_frame->setMouseTracking(true);
  
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

  long naxes[2] = { 128, 64 };   /* image is 300 pixels wide by 200 rows */
  unsigned char array[128][64];

  displayImage = new QImage(128, 64, QImage::Format_Indexed8);

  displayImage->setNumColors(256);
 
 for (int i=0; i < 256; i++)
   displayImage->setColor(i, qRgb(i,i,i));

/* Initialize the values in the image with a linear ramp function */
    for (int jj = 0; jj < 64; jj++)
        for (int ii = 0; ii < 128; ii++)
	{
            array[jj][ii] = ii + jj;
	    displayImage->setPixel(ii, jj, ((int) array[jj][ii]));
	}

 
 currentWidth = 300;
 currentHeight = 200;

 image_pixmap = QPixmap::fromImage(*displayImage);



 #if 0
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
 KProgressDialog fitsProgress(this, i18n("FITS Viewer"), i18n("Loading FITS..."));
 
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
 
 fitsProgress.progressBar()->setMinimum(0);
 fitsProgress.progressBar()->setMaximum(height);
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
      
      fitsProgress.progressBar()->setValue(height - i);
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
 #endif

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

FITSFrame::FITSFrame(FITSImage * img, QWidget * parent, const char * name) : QFrame(parent, name, Qt::WNoAutoErase)
{
  image = img;
  //setPaletteBackgroundColor(image->viewport()->paletteBackgroundColor());
}

FITSFrame::~FITSFrame() {}

void FITSFrame::paintEvent(QPaintEvent * /*e*/)
{
 
 bitBlt(this, 20, 20, &(image->image_pixmap));
 resize( (int) (image->currentWidth + 40), (int) (image->currentHeight + 40));

}



#include "fitsimage.moc"
