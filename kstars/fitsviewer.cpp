/***************************************************************************
                          fitsviewer.cpp  -  An FitsViewer for KStars
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

#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <qfile.h>

#include "fitsviewer.h"

#include "ksutils.h"

/* Load info */
typedef struct
{
  uint replace;    /* replacement for blank/NaN-values */
  uint use_datamin;/* Use DATAMIN/MAX-scaling if possible */
  uint compose;    /* compose images with naxis==3 */
} FITSLoadVals;

typedef struct
{
  int run;
} FITSLoadInterface;

static FITSLoadVals plvals =
{
  0,        /* Replace with black */
  0,        /* Do autoscale on pixel-values */
  0         /* Dont compose images */
};

FitsViewer::FitsViewer (const KURL *url, QWidget *parent, const char *name)
	: KMainWindow (parent, name)
{
    
    currentURL = *url;
    currentContrast =0;
    currentBrightness =0;
    imgBuffer = NULL;
    
    if (loadImage () == -1)
    {
	close();
	return;
    }
      
    KStdAction::save(this, SLOT(fileSave()), actionCollection());
    KStdAction::saveAs(this, SLOT(fileSaveAs()), actionCollection());
    KStdAction::close(this, SLOT(close()), actionCollection());
    
    KStdAction::undo(this, SLOT(fitsUNDO()), actionCollection());
    KStdAction::redo(this, SLOT(fitsREDO()), actionCollection());
    KStdAction::copy(this, SLOT(fitsCOPY()), actionCollection());
    
    new KAction( i18n("Image Reduction"), "blend", KShortcut( "Ctrl+R" ), this, SLOT( imageReduction()), actionCollection(), "image_reduce");
    new KAction( i18n("Brightness/Contrast"), "airbrush", KShortcut( "Ctrl+T" ), this, SLOT( BrightContrastDlg()), actionCollection(), "image_brightness_contrast");
    new KAction( i18n("Filters"), "filter", KShortcut( "Ctrl+L" ), this, SLOT( imageFilters()), actionCollection(), "image_filters");
  
   createGUI("fitsviewer.rc");
   
   setCaption(currentURL.filename());
   
   resize(image.width(), image.height() + toolBar()->height() + menuBar()->height());
}

FitsViewer::~FitsViewer()
{
 if (imgBuffer)
 	free(imgBuffer);
}

int FitsViewer::loadImage()
{
 int image_ID, *nl;
 char filename[currentURL.path().length()];
 uint picnum;
 int   k, n_images, max_images, hdu_picnum;
 int   compose;
 FILE *fp;
 FITS_FILE *ifp;
 FITS_HDU_LIST *hdu;

 strcpy(filename, currentURL.path().ascii());
 
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

 n_images = 0;
 max_images = 10;

 for (picnum = 1; picnum <= ifp->n_pic; )
 {
   /* Get image info to see if we can compose them */
   hdu = fits_image_info (ifp, picnum, &hdu_picnum);
   if (hdu == NULL) break;

   /* Get number of FITS-images to compose */
   compose = (   plvals.compose && (hdu_picnum == 1) && (hdu->naxis == 3)
              && (hdu->naxisn[2] > 1) && (hdu->naxisn[2] <= 4));
   if (compose)
     compose = hdu->naxisn[2];
   else
     compose = 1;  /* Load as GRAY */

   image_ID = loadFits (filename, ifp, picnum, compose);

   /* Write out error messages of FITS-Library */
   show_fits_errors ();

   if (image_ID == -1) break;
   n_images++;

   picnum += compose;
 }

 /* Write out error messages of FITS-Library */
 show_fits_errors ();

 fits_close (ifp);
 
 image_ID = (n_images > 0) ? 0 : -1;

 return (image_ID);

}

void check_load_vals (void)

{
  if (plvals.replace > 255) plvals.replace = 255;
}

void FitsViewer::show_fits_errors()
{
  char *msg;
  /* Write out error messages of FITS-Library */
  while ((msg = fits_get_error ()) != NULL)
     KMessageBox::error(0, msg);
}

int FitsViewer::loadFits (char *filename, FITS_FILE *ifp, uint picnum, uint ncompose)
{
 register unsigned char *dest, *src; 
 unsigned char *data, *data_end;
 int width, height, val;
 int i, j;
 double a, b;
 int err = 0;
 FITS_HDU_LIST *hdulist;
 FITS_PIX_TRANSFORM trans;

 hdulist = fits_seek_image (ifp, (int)picnum);
 if (hdulist == NULL) return (-1);

 width = hdulist->naxisn[0];  /* Set the size of the FITS image */
 height = hdulist->naxisn[1];

 data = (unsigned char *) malloc (height * width * ncompose * sizeof(unsigned char));
 if (data == NULL) return (-1);
 data_end = data + height * width * ncompose;

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

 image.create(width, height, 32);
 
 /* FITS stores images with bottom row first. Therefore we have */
 /* to fill the image from bottom to top. */

 if (ncompose == 1)
 {
   dest = data + height * width;
   
   for (i = 0; i < height ; i++)
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
       image.setPixel(j, i, qRgb(val, val, val));
      }
   }
 }
 else
 {
  KMessageBox::error(0, i18n("Error: KStars currently supports loading one image per FITS file. The current FITS file contains %1 images").arg(ncompose));
  free (data);
  return (-1);
 }

 imgBuffer = data;

 if (err)
   KMessageBox::error(0, i18n("EOF encountered on reading"));

 return (err ? -1 : 0);
}

void FitsViewer::paintEvent(QPaintEvent *ev)
{
	bitBlt (this, 0, toolBar()->height() + menuBar()->height() +1, &pix);
}

void FitsViewer::resizeEvent (QResizeEvent *ev)
{
	pix = kpix.convertToPixmap ( image.smoothScale ( size().width() , size().height() - toolBar()->height() - menuBar()->height()) );	

}

void FitsViewer::calculateStats()
{
  kdDebug() << "Calculating statistics..." << endl;
  stats.min 	= min();
  stats.max 	= max();
  stats.average = average();
  stats.stddev  = stddev();
  
  kdDebug() << "Min: " << stats.min << " - Max: " << stats.max << endl;
  kdDebug() << "Average: " << stats.average << " - stddev: " << stats.stddev << endl;

}

double FitsViewer::min()
{
  if (!imgBuffer) return -1;
  double lmin = imgBuffer[currentRect.y() * image.width() + currentRect.x()];
  int index=0;
  
  for (int i= currentRect.y() ; i < currentRect.height(); i++)
    for (int j= currentRect.x(); j < currentRect.width(); j++)
    {
       index = (i * image.width()) + j;
       if (imgBuffer[index] < lmin) lmin = imgBuffer[index];
    }
    
    return lmin;
}

double FitsViewer::max()
{
  if (!imgBuffer) return -1;
  double lmax = imgBuffer[currentRect.y() * image.width() + currentRect.x()];
  int index=0;
  
  for (int i= currentRect.y() ; i < currentRect.height(); i++)
    for (int j= currentRect.x(); j < currentRect.width(); j++)
    {
       index = (i * image.width()) + j;
       if (imgBuffer[index] > lmax) lmax = imgBuffer[index];
    }
    
    return lmax;
}

double FitsViewer::average()
{
  double sum=0;
  int index=0;
  if (!imgBuffer) return -1;

  for (int i= currentRect.y() ; i < currentRect.height(); i++)
    for (int j= currentRect.x(); j < currentRect.width(); j++)
    {
       index = (i * image.width()) + j;
       sum += imgBuffer[index];
    }
    
    return (sum / (currentRect.width() * currentRect.height() ));
}

double FitsViewer::stddev()
{
  int index=0;
  double lsum=0;
  if (!imgBuffer) return -1;
  
  for (int i= currentRect.y() ; i < currentRect.height(); i++)
    for (int j= currentRect.x(); j < currentRect.width(); j++)
    {
       index = (i * image.width()) + j;
       lsum += (imgBuffer[index] - stats.average) * (imgBuffer[index] - stats.average);
    }
    
  return (sqrt(lsum/(currentRect.width() * currentRect.height() - 1)));

}

void FitsViewer::keyPressEvent (QKeyEvent *ev)
{
        QImage Tempimage = image.copy();
	int val;
	
	ev->accept();  //make sure key press events are captured.
	switch (ev->key())
	{
		case Key_Up   : currentContrast+=1;
				if (currentContrast > 127) currentContrast = 127;
				kdDebug() << "Value before contrast " << qRed(image.pixel(0,0)) << endl;
				for (int i=0; i < image.height(); i++)
				  for (int j=0; j < image.width(); j++)
				  {
				    val = qRed(image.pixel(j,i));
				    val = (val - 255) * (-0.01 * -currentContrast) + val;
				    if (val < 0) val =0;
				    if (val > 255) val = 255;
				    Tempimage.setPixel(j, i, qRgb(val, val, val));
				  }
				  
				  kdDebug() << "Value after contrast " << qRed(image.pixel(0,0)) << endl;
		 		break;
		case Key_Down : currentContrast-=1;
				if (currentContrast < -127) currentContrast = -127;
				for (int i=0; i < image.height(); i++)
				  for (int j=0; j < image.width(); j++)
				  {
				    val = qRed(image.pixel(j,i));
				    val = (val - 255) * (-0.01 * -currentContrast) + val;
				    if (val < 0) val =0;
				    if (val > 255) val = 255;
				    Tempimage.setPixel(j, i, qRgb(val, val, val));
				  }
		  		break;
		case Key_H    : KImageEffect::contrastHSV(image); break;
		case Key_S    : KImageEffect::sharpen(image); break;
		case Key_B    : KImageEffect::blur(image); break;
		case Key_Right: currentBrightness+=5;
				if (currentBrightness > 100) currentBrightness = 100;
				KImageEffect::intensity(Tempimage, (currentBrightness/100.));
				break;
		case Key_Left : currentBrightness-=5;
				if (currentBrightness < -100) currentBrightness = -100;
				KImageEffect::intensity(Tempimage, (currentBrightness/100.));
				break;
		
		default : ev->ignore();
	}
	
	        kdDebug() << "Current contrast is " << currentContrast << " - Current brightness is " << currentBrightness << endl;
		pix = kpix.convertToPixmap ( Tempimage.smoothScale ( size().width() , size().height() - toolBar()->height() - menuBar()->height()) );	
		bitBlt (this, 0, toolBar()->height() + menuBar()->height() +1, &pix);
		//resizeEvent(NULL);
		//paintEvent(NULL);

}

void FitsViewer::fileSave()
{

}

void FitsViewer::fileSaveAs()
{

}

void FitsViewer::fitsUNDO()
{

}

void FitsViewer::fitsREDO()
{

}
	
void FitsViewer::fitsCOPY()
{

}

void FitsViewer::imageReduction()
{


}

void FitsViewer::BrightContrastDlg()
{

}

void FitsViewer::imageFilters()
{


}

