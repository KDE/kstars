/***************************************************************************
                          FITSViewer.cpp  -  A FITSViewer for KStars
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
#include <kcommand.h>
#include <klineedit.h>
#include <klistview.h>

#include <qfile.h>
#include <qvbox.h>
#include <qcursor.h>
#include <qstringlist.h>
#include <qlistview.h>
#include <qradiobutton.h>

#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>

#include "fitsviewer.h"
#include "fitsimage.h"
#include "fitsprocess.h"
#include "fitshistogram.h"
#include "conbridlg.h"
#include "statform.h"
#include "imagereductiondlg.h"
#include "fitsheaderdialog.h"
#include "fitsfilter.h"
#include "ksutils.h"

#define FITS_GETBITPIX16(p,val) val = ((p[1] << 8) | (p[0]))
#define FITS_GETBITPIX32(p,val) val = \
          ((p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0])

FITSViewer::FITSViewer (const KURL *url, QWidget *parent, const char *name)
	: KMainWindow (parent, name)
{
    currentURL = *url;
    imgBuffer = NULL;

    /* Setup image widget */    
    image = new FITSImage(this);
    setCentralWidget(image);
    
    /* Load image into buffer */
    if ( (imgBuffer = loadData (currentURL.path().ascii(), imgBuffer)) == NULL)  { close(); return; }
    /* Display image in the central widget */
    if (image->loadFits(currentURL.path().ascii()) == -1) { close(); return; }
    
    /* Initiliaze menu actions */
    history = new KCommandHistory(actionCollection());
    history->setUndoLimit(5);
    history->setRedoLimit(5);

    new KAction( i18n("Image Reduction"), "blend", KShortcut( "Ctrl+R" ), this, SLOT( imageReduction()), actionCollection(), "image_reduce");
    new KAction( i18n("Brightness/Contrast"), "airbrush", KShortcut( "Ctrl+T" ), this, SLOT( BrightContrastDlg()), actionCollection(), "image_brightness_contrast");
    new KAction( i18n("Filters"), "filter", KShortcut( "Ctrl+L" ), this, SLOT( fitsFilter()), actionCollection(), "image_filters");
    new KAction ( i18n("Histogram"), "wizard", KShortcut("Ctrl+H"), this, SLOT (imageHistogram()), actionCollection(), "image_histogram");
       
    KStdAction::save(this, SLOT(fileSave()), actionCollection());
    KStdAction::saveAs(this, SLOT(fileSaveAs()), actionCollection());
    KStdAction::close(this, SLOT(close()), actionCollection());
    KStdAction::copy(this, SLOT(fitsCOPY()), actionCollection());
    KStdAction::zoomIn(image, SLOT(fitsZoomIn()), actionCollection());
    KStdAction::zoomOut(image, SLOT(fitsZoomOut()), actionCollection());
    new KAction( i18n( "&Default Zoom" ), "viewmagfit.png", KShortcut( "Ctrl+D" ),
		image, SLOT(fitsZoomDefault()), actionCollection(), "zoom_default" );
    new KAction( i18n( "Statistics"), "math_matrix.png", 0, this, SLOT(fitsStatistics()), actionCollection(), "image_stats");
    new KAction( i18n( "FITS Header"), "frame_spreadsheet.png", 0, this, SLOT(fitsHeader()), actionCollection(), "fits_editor");
    
   /* Create GUI */  
   createGUI("fitsviewer.rc");
    
   setCaption(currentURL.filename());
   statusBar()->insertItem("                               ", 0, true);
   statusBar()->setItemAlignment(0 , Qt::AlignLeft);
   statusBar()->insertItem(".......", 1);

   /* Get initial statistics */
   calculateStats();
   
   /* resize in accord with KDE rules */
   resize(640, 480);
}

FITSViewer::~FITSViewer()
{
   free(imgBuffer);
}

void FITSViewer::show_fits_errors()
{
  char *msg;
  /* Write out error messages of FITS-Library */
  while ((msg = fits_get_error ()) != NULL)
     KMessageBox::error(0, msg);
}

unsigned int * FITSViewer::loadData(const char *filename, unsigned int *buffer)
{
  FILE *fp;
  FITS_FILE *ifp;
  FITS_HDU_LIST *hdulist;
  unsigned char *tempData, *tempDataPtr;
  short pixval_16;
  long  pixval_32;
  int width, height, bpp;
 
 fp = fopen (filename, "rb");
 if (!fp)
 {
   KMessageBox::error(0, i18n("Cannot open file for reading"));
   return (NULL);
 }
 fclose (fp);

 ifp = fits_open (filename, "r");
 if (ifp == NULL)
 {
   KMessageBox::error(0, i18n("Error during open of FITS file"));
   return (NULL);
 }
 if (ifp->n_pic <= 0)
 {
   KMessageBox::error(0, i18n("FITS file keeps no displayable images"));
   fits_close (ifp);
   return (NULL);
 }
  
  // We only deal with 1 image in a FITS for now.
  hdulist = fits_seek_image (ifp, 1);
  if (hdulist == NULL) return (NULL);
  
  width  = hdulist->naxisn[0]; 
  height = hdulist->naxisn[1];
  bpp    = hdulist->bpp;

  buffer           = (unsigned int   *) malloc (height * width * sizeof(unsigned int )); 
  tempData         = (unsigned char  *) malloc (height * width * bpp * sizeof(unsigned char));
  if (buffer == NULL || tempData == NULL)
  {
    KMessageBox::error(0, i18n("Not enough memory to load FITS."));
    return (NULL);
  }
  tempDataPtr      = tempData;
  
  fread(tempData, 1, width * height * bpp, ifp->fp); 
  
  //TODO change this to bitpix to check for all possiblities.
  switch (bpp)
  {
   case 1:
    for (int i=0; i < width * height; i++)
      buffer[i] = (int) tempData[i];
    break;
    
   case 2:
    for (int i=0; i < height ; i++)
      for (int j=0; j < width ; j++)
      {
        FITS_GETBITPIX16(tempData, pixval_16);
        buffer[i * width + j] = ntohs(pixval_16);
	tempData+=2;
      }
      break;
      
   case 4:
   for (int i=0; i < width * height ; i++)
    {
    FITS_GETBITPIX32(tempData, pixval_32);
    buffer[i] = ntohl(pixval_32);
    tempData+=4;
   }
    break;
  }
 
  fits_close(ifp);
  free(tempDataPtr); 
  return buffer;                                              

}

void FITSViewer::calculateStats()
{
  kdDebug() << "Calculating statistics..." << endl;
  stats.min 	= min(stats.minAt);
  stats.max 	= max(stats.maxAt);
  stats.average = average();
  stats.stddev  = stddev();
  stats.bitpix  = image->bitpix;
  stats.width   = image->width;
  stats.height  = image->height;
  
  kdDebug() << "Min: " << stats.min << " - Max: " << stats.max << endl;
  kdDebug() << "Average: " << stats.average << " - stddev: " << stats.stddev << endl;
  kdDebug() << "Width: " << stats.width << " - Height " << stats.height << " - bitpix " << stats.bitpix << endl;

}

double FITSViewer::min(int & minIndex)
{
  if (!imgBuffer) return -1;
  int width   = image->currentRect.width();
  int height  = image->currentRect.height();
  double lmin = imgBuffer[image->currentRect.y() * width + image->currentRect.x()];
  int index=0; 
  
  for (int i= image->currentRect.y() ; i < height; i++)
    for (int j= image->currentRect.x(); j < width; j++)
    {
       index = (i * width) + j;
       if (imgBuffer[index] < lmin)
       {
         minIndex = index;
         lmin = imgBuffer[index];
       }
       
    }
    
    return lmin;
}

double FITSViewer::max(int & maxIndex)
{
  if (!imgBuffer) return -1;
  int width   = image->currentRect.width();
  int height  = image->currentRect.height();
  double lmax = imgBuffer[image->currentRect.y() * width + image->currentRect.x()];
  int index=0;
  
  for (int i= image->currentRect.y() ; i < height; i++)
    for (int j= image->currentRect.x(); j < width; j++)
    {
       index = (i * width) + j;
       if ( imgBuffer[index] > lmax)
       {
        maxIndex = index;
        lmax = imgBuffer[index];
       }
    }
    
    return lmax;
}

double FITSViewer::average()
{
  int index=0;
  double sum=0;  
  int width  = image->currentRect.width();
  int height = image->currentRect.height();
  if (!imgBuffer) return -1;
  
  for (int i= image->currentRect.y() ; i < height; i++)
    for (int j= image->currentRect.x(); j < width; j++)
    {
       index = (i * width) + j;
       sum += imgBuffer[index];
    }
    
    return (sum / (width * height ));
}

double FITSViewer::stddev()
{
  int index=0;
  double lsum=0;
  int width  = image->currentRect.width();
  int height = image->currentRect.height();
  if (!imgBuffer) return -1;
  
  for (int i= image->currentRect.y() ; i < height; i++)
    for (int j= image->currentRect.x(); j < width; j++)
    {
       index = (i * width) + j;
       lsum += (imgBuffer[index] - stats.average) * (imgBuffer[index] - stats.average);
    }
    
  return (sqrt(lsum/(width * height - 1)));

}

void FITSViewer::keyPressEvent (QKeyEvent *ev)
{
        //QImage Tempimage = imageList.at(undo+1)->copy();
	
	ev->accept();  //make sure key press events are captured.
	switch (ev->key())
	{
		//case Key_H    : KImageEffect::contrastHSV(image); break;
		//case Key_S    : KImageEffect::sharpen(image); break;
		//case Key_B    : KImageEffect::blur(image); break;
	
		default : ev->ignore();
	}
	
}

void FITSViewer::fileSave()
{

}

void FITSViewer::fileSaveAs()
{

}

void FITSViewer::fitsCOPY()
{

}

void FITSViewer::imageReduction()
{

  QStringList darkFiles, flatFiles;
  int darkCombineMode = 0 , flatCombineMode = 0;
  QListViewItem *file;
  
  image->saveTemplateImage();
  ImageReductionDlg irDialog(this);
    
  if (irDialog.exec() == QDialog::Accepted)
  {
    if (irDialog.darkListView->childCount() == 0 && 
        irDialog.flatListView->childCount() == 0)
	{
	 image->destroyTemplateImage();
	 return;
	}
	 
    darkCombineMode = irDialog.darkAverageB->isChecked() ? 0 : 1;
    flatCombineMode = irDialog.flatAverageB->isChecked() ? 0 : 1;
    
    file = irDialog.darkListView->firstChild();
    while (file)
    {
      darkFiles << file->text(0);
      file = file->nextSibling();
    }
    
    file = irDialog.flatListView->firstChild();
    while (file)
    {
      flatFiles << file->text(0);
      file = file->nextSibling();
    }
    
    FITSProcess reduc(this, darkFiles, flatFiles, darkCombineMode, flatCombineMode);
    calculateStats();
    image->rescale(FITSImage::FITSLinear, (int) stats.min, (int) stats.max);
       
  }
  
  
  image->destroyTemplateImage();


}

void FITSViewer::BrightContrastDlg()
{
  conbriCommand *cbc;
  image->saveTemplateImage();
  ContrastBrightnessDlg conbriDlg(this);
    
  if (conbriDlg.exec() == QDialog::Rejected)
  {
    image->reLoadTemplateImage();
    image->zoomToCurrent();
    //image->convertImageToPixmap();
    //image->update();
  }
  else
  {
    image->update();
    cbc = new conbriCommand(this, image->displayImage, image->templateImage);
    history->addCommand(cbc, false);

  }
  
  image->destroyTemplateImage();
    
}

void FITSViewer::imageHistogram()
{

  histCommand *histC;
  image->saveTemplateImage();
  FITSHistogram hist(this);
    
  if (hist.exec() == QDialog::Rejected)
  {
    image->reLoadTemplateImage();
    image->zoomToCurrent();
  }
  else
  {
    //image->update();
    histC = new histCommand(this, hist.type, image->displayImage, image->templateImage);
    history->addCommand(histC, false);

  }
  
  image->destroyTemplateImage();
  
}

void FITSViewer::fitsUndo()
{







}

void FITSViewer::fitsRedo()
{









}

void FITSViewer::fitsStatistics()
{
  statForm stat(this);// = new statForm(this, 0, Qt::WDestructiveClose);
  
  stat.widthOUT->setText(QString("%1").arg(stats.width));
  stat.heightOUT->setText(QString("%1").arg(stats.height));
  stat.bitpixOUT->setText(QString("%1").arg(stats.bitpix));
  stat.maxOUT->setText(QString("%1").arg(stats.max));
  stat.minOUT->setText(QString("%1").arg(stats.min));
  stat.atMaxOUT->setText(QString("%1").arg(stats.maxAt));
  stat.atMinOUT->setText(QString("%1").arg(stats.minAt));
  stat.meanOUT->setText(QString("%1").arg(stats.average));
  stat.stddevOUT->setText(QString("%1").arg(stats.stddev));
  
  stat.exec();

}

void FITSViewer::fitsHeader()
{
   QStringList cards;
   QString record;
   QString property;
   int equal, slash;

   fitsHeaderDialog header(this);
   header.headerView->setSorting(-1);
   
   record = QString((char *) image->hdulist->header_record_list->data);
   
   for (int i=0; i < FITS_RECORD_SIZE / FITS_CARD_SIZE; i++)
   {
     property = record.left(FITS_CARD_SIZE);
     
     equal = property.find('=');
     cards << property.left(equal);
     slash = property.find("'");
     if (slash != -1)
       slash = property.find("'", slash + 1) + 1;
     else
       slash = property.find('/') - 1;
       
     cards << property.mid(equal + 2, slash - (equal + 2));
     cards << property.mid(slash + 1, FITS_CARD_SIZE - (slash + 1));
     
     record.remove(0, FITS_CARD_SIZE);
   }
   
   //kdDebug() << "Testing " << cards[0] << " -- " << cards[1] << " -- " << cards[2] << endl;
   //kdDebug() << "Testing " << cards[3] << " -- " << cards[4] << " -- " << cards[5] << endl;
   
   for (int i= cards.count() - 3; i >=0 ; i-=3)
       new QListViewItem( header.headerView, cards[i], cards[i+1], cards[i+2]);
   
      
  /* if (image->hdulist->used.datamax)
    new QListViewItem( header.headerView, "DATAMAX", QString("%1").arg(image->hdulist->datamax));
   if (image->hdulist->used.datamin)
    new QListViewItem( header.headerView, "DATAMIN", QString("%1").arg(image->hdulist->datamin));
   if (image->hdulist->used.bzero)
    new QListViewItem( header.headerView, "BZERO", QString("%1").arg(image->hdulist->bzero));
      if (image->hdulist->used.bscale)
    new QListViewItem( header.headerView, "BSCALE", QString("%1").arg(image->hdulist->bscale));
    
   new QListViewItem( header.headerView, "NAXIS2", QString("%1").arg(image->hdulist->naxisn[1]));
   new QListViewItem( header.headerView, "NAXIS1", QString("%1").arg(image->hdulist->naxisn[0]));
   new QListViewItem( header.headerView, "NAXIS", QString("%1").arg(image->hdulist->naxis)); 
   new QListViewItem( header.headerView, "BITPIX", QString("%1").arg(image->hdulist->bitpix));
   new QListViewItem( header.headerView, "SIMPLE", image->hdulist->used.simple ? "T" : "F");*/
   
   header.exec();

}


void FITSViewer::fitsFilter()
{

  FITSFilter filter(this);
  filter.exec();

}

#include "fitsviewer.moc"
