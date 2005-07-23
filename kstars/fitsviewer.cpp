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
#include <qclipboard.h>
#include <qimage.h>

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
#include "ksutils.h"
#include "Options.h"

extern int fits_ieee32_intel;
extern int fits_ieee32_motorola;
extern int fits_ieee64_intel;
extern int fits_ieee64_motorola;

#define FITS_GETBITPIX16(p,val) val = ((p[0] << 8) | (p[1]))
#define FITS_GETBITPIX32(p,val) val = \
          ((p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3])
	  
#define FITS_GETBITPIXM32(p,val) \
 { if (fits_ieee32_intel) {unsigned char uc[4]; \
   uc[0] = p[3]; uc[1] = p[2]; uc[2] = p[1]; uc[3] = p[0]; \
   val = *(FITS_BITPIXM32 *)uc; } \
   else if (fits_ieee32_motorola) { val = *(FITS_BITPIXM32 *)p; } \
   else if (fits_ieee64_motorola) {FITS_BITPIXM64 m64; \
   unsigned char *uc= (unsigned char *)&m64; \
   uc[0]=p[0]; uc[1]=p[1]; uc[2]=p[2]; uc[3]=p[3]; uc[4]=uc[5]=uc[6]=uc[7]=0; \
   val = (FITS_BITPIXM32)m64; } \
   else if (fits_ieee64_intel) {FITS_BITPIXM64 i64; \
   unsigned char *uc= (unsigned char *)&i64; \
   uc[0]=uc[1]=uc[2]=uc[3]=0; uc[7]=p[3]; uc[6]=p[2]; uc[5]=p[1]; uc[4]=p[0]; \
   val = (FITS_BITPIXM32)i64;}\
}
	  
#define FITS_GETBITPIXM64(p,val) \
 { if (fits_ieee64_intel) {unsigned char uc[8]; \
   uc[0] = p[7]; uc[1] = p[6]; uc[2] = p[5]; uc[3] = p[4]; \
   uc[4] = p[3]; uc[5] = p[2]; uc[6] = p[1]; uc[7] = p[0]; \
   val = *(FITS_BITPIXM64 *)uc; } else val = *(FITS_BITPIXM64 *)p; }	  

FITSViewer::FITSViewer (const KURL *url, QWidget *parent, const char *name)
	: KMainWindow (parent, name)
{
    image      = NULL;
    currentURL = *url;
    imgBuffer  = NULL;
    histo      = NULL;
    Dirty      = 0;
    
    /* Initiliaze menu actions */
    history = new KCommandHistory(actionCollection());
    history->setUndoLimit(10);
    history->setRedoLimit(10);
    history->documentSaved();
    connect(history, SIGNAL(documentRestored()), this, SLOT(fitsRestore()));
    
    /* Setup image widget */    
    image = new FITSImage(this);
    setCentralWidget(image);
   
    statusBar()->insertItem("", 0);
    statusBar()->setItemFixed(0, 100);
    statusBar()->insertItem("", 1);
    statusBar()->setItemFixed(1, 100);
    statusBar()->insertItem("", 2);
    statusBar()->setItemFixed(2, 100);
    statusBar()->insertItem(i18n("Welcome to KStars FITS Viewer"), 3, 1, true);
    statusBar()->setItemAlignment(3 , Qt::AlignLeft);
    
    /* FITS initializations */
    if (!initFITS())
    {
     close();
     return;
    }
     
    QFile tempFile;
    
    if (KSUtils::openDataFile( tempFile, "imgreduction.png" ) )
    {
    	new KAction( i18n("Image Reduction"), tempFile.name(), KShortcut( "Ctrl+R" ), this, SLOT( imageReduction()), actionCollection(), "image_reduce");
	tempFile.close();
    }
    else
    	new KAction( i18n("Image Reduction"), "blend", KShortcut( "Ctrl+R" ), this, SLOT( imageReduction()), actionCollection(), "image_reduce");
	
    /*if (KSUtils::openDataFile( tempFile, "bricon.png" ) )
    {
    	new KAction( i18n("Brightness/Contrast"), tempFile.name(), KShortcut( "Ctrl+T" ), this, SLOT( BrightContrastDlg()), actionCollection(), "image_brightness_contrast");
	tempFile.close();
    }
    else*/
       	new KAction( i18n("Brightness/Contrast"), "contrast+", KShortcut( "Ctrl+T" ), this, SLOT( BrightContrastDlg()), actionCollection(), "image_brightness_contrast");
	
    if (KSUtils::openDataFile( tempFile, "histogram.png" ) )
    {
    	new KAction ( i18n("Histogram"), tempFile.name(), KShortcut("Ctrl+H"), this, SLOT (imageHistogram()), actionCollection(), "image_histogram");
	tempFile.close();
    }
    else
        new KAction ( i18n("Histogram"), "wizard", KShortcut("Ctrl+H"), this, SLOT (imageHistogram()), actionCollection(), "image_histogram");
    
    KStdAction::open(this, SLOT(fileOpen()), actionCollection());   
    KStdAction::save(this, SLOT(fileSave()), actionCollection());
    KStdAction::saveAs(this, SLOT(fileSaveAs()), actionCollection());
    KStdAction::close(this, SLOT(slotClose()), actionCollection());
    KStdAction::copy(this, SLOT(fitsCOPY()), actionCollection());
    KStdAction::zoomIn(image, SLOT(fitsZoomIn()), actionCollection());
    KStdAction::zoomOut(image, SLOT(fitsZoomOut()), actionCollection());
    new KAction( i18n( "&Default Zoom" ), "viewmagfit.png", KShortcut( "Ctrl+D" ),
		image, SLOT(fitsZoomDefault()), actionCollection(), "zoom_default" );
    new KAction( i18n( "Statistics"), "sum", 0, this, SLOT(fitsStatistics()), actionCollection(), "image_stats");
    new KAction( i18n( "FITS Header"), "frame_spreadsheet.png", 0, this, SLOT(fitsHeader()), actionCollection(), "fits_editor");
    
   /* Create GUI */  
   createGUI("fitsviewer.rc");
     
   /* initially resize in accord with KDE rules */
   resize(640, 480); 
}

FITSViewer::~FITSViewer()
{
   free(imgBuffer);
}

bool  FITSViewer::initFITS()
{

    free(imgBuffer);
    imgBuffer = NULL;
    image->clearMem();
    
  /* Load image into buffer */
    if ( (imgBuffer = loadData (currentURL.path().ascii(), imgBuffer)) == NULL)  { close(); return false; }
    /* Display image in the central widget */
    if (image->loadFits(currentURL.path().ascii()) == -1) { close(); return false; }

    /* Clear history */
    history->clear();
    
    /* Set new file caption */
    setCaption(currentURL.fileName());
    
    /* Get initial statistics */
    calculateStats();
    
    image->viewport()->resize(image->viewport()->width() + 5, image->viewport()->height());
    image->viewportResizeEvent(NULL);
    
    return true;

}

void FITSViewer::slotClose()
{

  if (Dirty)
  {
    
  QString caption = i18n( "Save Changes to FITS?" );
		QString message = i18n( "The current FITS file has unsaved changes.  Would you like to save before closing it?" );
		int ans = KMessageBox::warningYesNoCancel( 0, message, caption, KStdGuiItem::save(), KStdGuiItem::discard() );
		if ( ans == KMessageBox::Yes )
			fileSave();	
		else if ( ans == KMessageBox::No ) 
			fitsRestore();
   }
   
   if (Dirty == 0)
    close();
}

void FITSViewer::closeEvent(QCloseEvent *ev)
{

  if (Dirty)
  {
    
  QString caption = i18n( "Save Changes to FITS?" );
		QString message = i18n( "The current FITS file has unsaved changes.  Would you like to save before closing it?" );
		int ans = KMessageBox::warningYesNoCancel( 0, message, caption, KStdGuiItem::save(), KStdGuiItem::discard() );
		if ( ans == KMessageBox::Yes )
			fileSave();	
		else if ( ans == KMessageBox::No ) 
			fitsRestore();
   }
   
   if (Dirty == 0)
    ev->accept();
   else 
    ev->ignore();

}

void FITSViewer::show_fits_errors()
{
  char *msg;
  /* Write out error messages of FITS-Library */
  while ((msg = fits_get_error ()) != NULL)
     KMessageBox::error(0, msg);
}

float * FITSViewer::loadData(const char *filename, float *buffer)
{
  FILE *fp;
  FITS_FILE *ifp;
  FITS_HDU_LIST *hdulist;
  unsigned char *tempData, *tempDataPtr;
  register FITS_BITPIX16  pixval_16  =0;
  register FITS_BITPIX32  pixval_32  =0;
  register FITS_BITPIXM32 pixval_m32 =0;
  register FITS_BITPIXM64 pixval_m64 =0;
  int totalCount;
  int width, height, bpp, bitpix;
 
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
  
  totalCount = width * height;
  
  bpp    = hdulist->bpp;
  bitpix = hdulist->bitpix;
  
  buffer           = (float          *) malloc (height * width * sizeof(float)); 
  tempData         = (unsigned char  *) malloc (height * width * bpp * sizeof(unsigned char));
  if (buffer == NULL || tempData == NULL)
  {
    KMessageBox::error(0, i18n("Not enough memory to load FITS."));
    return (NULL);
  }
  tempDataPtr      = tempData;
  
  if (fread(tempData, 1, width * height * bpp, ifp->fp) != (unsigned int) (width * height * bpp))
  {
    KMessageBox::error(0, i18n("Unable to read FITS data from file. %1.\n").arg(strerror(errno)));
    return (NULL);
  }
  
  switch (bitpix)
  {
   case 8:
    for (int i=0; i < totalCount; i++)
      buffer[i] = tempData[i];
    break;
    
   case 16:
    for (int i=0; i < totalCount ; i++)
    {
       FITS_GETBITPIX16(tempData, pixval_16);
       buffer[i] = pixval_16;//ntohs(pixval_16);
       tempData+=2;
     }
     break;
      
   case 32:
   for (int i=0; i < totalCount ; i++)
    {
    FITS_GETBITPIX32(tempData, pixval_32);
    //pixval_32 = ntohl(pixval_32);
    if (isnan(pixval_32)) pixval_32 = 0;
    buffer[i] = pixval_32;
    tempData+=4;
   }
    break;
    
   case -32:
   for (int i=0; i < totalCount ; i++)
    {
    if (fits_nan_32 (tempData))
     pixval_m32 = 0;
    else
     FITS_GETBITPIXM32(tempData, pixval_m32);
    buffer[i] = pixval_m32;    
    tempData+=4;
    }
    break;
    
    case -64:
    for (int i=0; i < totalCount ; i++)
    {
    if (fits_nan_64 (tempData))
     pixval_m64 = 0;
    else
     FITS_GETBITPIXM64(tempData, pixval_m64);
    buffer[i] = pixval_m64;
    tempData+=8;
   }
    break;
  }
 
  fits_close(ifp);
  free(tempDataPtr); 
  return buffer;                                              

}

void FITSViewer::calculateStats()
{
  /*kdDebug() << "Calculating statistics..." << endl;*/
  stats.min 	= min(stats.minAt);
  stats.max 	= max(stats.maxAt);
  stats.average = average();
  stats.stddev  = stddev();
  stats.bitpix  = image->bitpix;
  stats.width   = image->width;
  stats.height  = image->height;
  
  /*kdDebug() << "Min: " << stats.min << " - Max: " << stats.max << endl;
  kdDebug() << "Average: " << stats.average << " - stddev: " << stats.stddev << endl;
  kdDebug() << "Width: " << stats.width << " - Height " << stats.height << " - bitpix " << stats.bitpix << endl;*/
  
  statusBar()->changeItem( QString("%1 x %2").arg( (int) stats.width).arg( (int) stats.height), 2);

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

void FITSViewer::fileOpen()
{

  if (Dirty)
  {
    
  QString caption = i18n( "Save Changes to FITS?" );
		QString message = i18n( "The current FITS file has unsaved changes.  Would you like to save before closing it?" );
		int ans = KMessageBox::warningYesNoCancel( 0, message, caption, KStdGuiItem::save(), KStdGuiItem::discard() );
		if ( ans == KMessageBox::Yes )
			fileSave();	
		else if ( ans == KMessageBox::No ) 
			fitsRestore();
   }
   
   KURL fileURL = KFileDialog::getOpenURL( QDir::homeDirPath(), "*.fits *.fit *.fts|Flexible Image Transport System");
  
  if (fileURL.isEmpty())
    return;

  
  currentURL = fileURL;
  
  initFITS();

}

void FITSViewer::fileSave()
{
  
  FITS_FILE *ifp;
  QString recordList;
  KURL backupCurrent = currentURL;
  QString bitpixRec;
  FITS_BITPIX16  pixval_16  =0;
  FITS_BITPIX32  pixval_32  =0;
  FITS_BITPIXM32 pixval_m32 =0;
  FITS_BITPIXM64 pixval_m64 =0;
  unsigned char *transData;
  int index=0, i=0, transCount = 0, totalCount= image->width * image->height;
  
  QString currentDir = Options::fitsSaveDirectory();
  
  //kdDebug() << "We doing stats BEFORE we save!! " << endl;
  //calculateStats();
  
  // If no changes made, return.
  if (Dirty == 0 && !currentURL.isEmpty())
    return;
  
  if (currentURL.isEmpty())
  {
  	currentURL = KFileDialog::getSaveURL( currentDir, "*.fits |Flexible Image Transport System");
	// if user presses cancel
	if (currentURL.isEmpty())
	{
	  currentURL = backupCurrent;
	  return;
	}
	if (currentURL.path().contains('.') == 0) currentURL.setPath(currentURL.path() + ".fits");
	
	if (QFile::exists(currentURL.path()))
        {
            int r=KMessageBox::warningContinueCancel(static_cast<QWidget *>(parent()),
            i18n( "A file named \"%1\" already exists. "
                  "Overwrite it?" ).arg(currentURL.fileName()),
            i18n( "Overwrite File?" ),
            i18n( "&Overwrite" ) );
  
             if(r==KMessageBox::Cancel) return;
         }
   }

  if ( currentURL.isValid() )
  {
        transData = (unsigned char *) malloc (sizeof(unsigned char) * totalCount * image->bpp);
	if (transData == NULL)
	{
  		KMessageBox::error(0, i18n("Error: Low memory. Saving is aborted."));
		return;
  	}
  
  	ifp = fits_open (currentURL.path().ascii(), "w");
        if (ifp == NULL)
        {
          KMessageBox::error(0, i18n("Error during open of FITS file."));
          return;
        }
    	
	setbuf(ifp->fp, NULL);
	
	bitpixRec.sprintf("BITPIX  =                    %d /Modified by KStars                               ", image->bitpix);
	bitpixRec.truncate(80);
	
	for (unsigned int j=0; j < record.count(); j++)
	{
	  recordList = record[j];
	  
	  if ( (index = recordList.find("BITPIX")) != -1)
	  	recordList.replace(index, FITS_CARD_SIZE, bitpixRec);
	
	  fwrite(recordList.ascii(), 1, FITS_RECORD_SIZE, ifp->fp);
        }
	
	switch (image->bitpix)
	{
	   case 8:
		for (i= image->height - 1; i >= 0; i--)
		fwrite(image->displayImage->scanLine(i), 1, image->width, ifp->fp);
		break;
		
		
	   case 16:
    		for (i= 0, transCount = 0 ; i < totalCount ; i++, transCount += 2)
    		{
       			pixval_16 = (unsigned short) imgBuffer[i];
			transData[transCount]   = ((unsigned char*) &pixval_16)[1];
			transData[transCount+1] = ((unsigned char*) &pixval_16)[0];
		}
		// Now we need to write all uchars to file. We have 2 bytes per pixel
		transCount = 0;
		totalCount *= 2;
		
		for (i=0, transCount = 0; i < totalCount; i += transCount)
		   transCount = fwrite( transData + i , 1, totalCount - i, ifp->fp);
       			
     		break;
      
   	  case 32:
	        for (i=0, transCount = 0 ; i < totalCount ; i++, transCount += 4)
    		{
       			pixval_32 = (unsigned int) imgBuffer[i];
			transData[transCount]       = ((unsigned char*) &pixval_32)[3];
			transData[transCount+1]     = ((unsigned char*) &pixval_32)[2];
			transData[transCount+2]     = ((unsigned char*) &pixval_32)[1];
			transData[transCount+3]     = ((unsigned char*) &pixval_32)[0];
		}
		
		// Now we need to write all uchars to file. We have 4 bytes per pixel
		transCount = 0;
		totalCount *= 4;
		
		for (i=0, transCount = 0; i < totalCount; i += transCount)
		   transCount = fwrite( transData + i , 1, totalCount - i, ifp->fp);
    		break;
    
   	case -32:
		for (i=0, transCount = 0 ; i < totalCount ; i++, transCount += 4)
    		{
       			pixval_m32 = imgBuffer[i];
			transData[transCount]       = ((unsigned char*) &pixval_m32)[3];
			transData[transCount+1]     = ((unsigned char*) &pixval_m32)[2];
			transData[transCount+2]     = ((unsigned char*) &pixval_m32)[1];
			transData[transCount+3]     = ((unsigned char*) &pixval_m32)[0];
			
		}
		
		// Now we need to write all uchars to file. We have 4 bytes per pixel
		transCount = 0;
		totalCount *= 4;
		
		for (i=0, transCount = 0; i < totalCount; i += transCount)
		   transCount = fwrite( transData + i , 1, totalCount - i, ifp->fp);
   		
    		break;
    
    	case -64:
    		for (i=0, transCount = 0 ; i < totalCount ; i++, transCount += 8)
    		{
       			pixval_m64 = imgBuffer[i];
			transData[transCount]   = 0;
			transData[transCount+1] = 0;
			transData[transCount+2] = 0;
			transData[transCount+3] = 0;
			transData[transCount+4] = ((unsigned char*) &pixval_m32)[3];
			transData[transCount+5] = ((unsigned char*) &pixval_m32)[2];
			transData[transCount+6] = ((unsigned char*) &pixval_m32)[1];
			transData[transCount+7] = ((unsigned char*) &pixval_m32)[0];
			
		}
		
		// Now we need to write all uchars to file. We have 4 bytes per pixel
		transCount = 0;
		totalCount *= 8;
		
		for (i=0, transCount = 0; i < totalCount; i += transCount)
		   transCount = fwrite( transData + i , 1, totalCount - i, ifp->fp);
	        break;	
	}
		
	fits_close(ifp);
	
	statusBar()->changeItem(i18n("File saved."), 3);
	
	free(transData);
	Dirty = 0;
	history->clear();
	fitsRestore();
	//updateImgBuffer();
  }
  else
  {
		QString message = i18n( "Invalid URL: %1" ).arg( currentURL.url() );
		KMessageBox::sorry( 0, message, i18n( "Invalid URL" ) );
  }
	

}

void FITSViewer::fileSaveAs()
{
  
  currentURL = "";
  fileSave();
}

void FITSViewer::fitsCOPY()
{
   kapp->clipboard()->setImage(*image->displayImage);
}

void FITSViewer::updateImgBuffer()
{
  int width = image->width;
  int height = image->height;
  
  for (int i=0; i < height; i++)
    for (int j=0; j < width; j++)
       imgBuffer[i * width + j] = (int) *(image->displayImage->scanLine(height - i - 1) + j);
       //image->reducedImgBuffer[i * width + j];

   calculateStats();
}

void FITSViewer::imageReduction()
{
  FITSProcessCommand *cbc;
  FITSHistogramCommand *hbc;
  QStringList darkFiles, flatFiles, darkflatFiles;
  int darkCombineMode = 0 , flatCombineMode = 0, darkflatCombineMode =0;
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
     
    darkCombineMode    = irDialog.darkAverageB->isChecked() ? 0 : 1;
    flatCombineMode    = irDialog.flatAverageB->isChecked() ? 0 : 1;
    darkflatCombineMode= irDialog.darkflatAverageB->isChecked() ? 0 : 1;
     
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
    
    file = irDialog.darkflatListView->firstChild();
    while (file)
    {
      darkflatFiles << file->text(0);
      file = file->nextSibling();
    }
      
      cbc = new FITSProcessCommand(this);
      FITSProcess reduc(this, darkFiles, flatFiles, darkflatFiles, darkCombineMode, flatCombineMode, darkflatCombineMode);
      reduc.reduce();
      history->addCommand(cbc, false);
      calculateStats();
      hbc = new FITSHistogramCommand(this, NULL, FITSImage::FITSLinear, (int) stats.min, (int) stats.max);
      history->addCommand(hbc);
      fitsChange();
  }
  
  image->destroyTemplateImage();

}

void FITSViewer::BrightContrastDlg()
{
  FITSChangeCommand *cbc;
  image->saveTemplateImage();
  ContrastBrightnessDlg conbriDlg(this);
    
  if (conbriDlg.exec() == QDialog::Rejected)
  {
    image->reLoadTemplateImage();
    image->zoomToCurrent();
  }
  else
  {
    memcpy(imgBuffer , conbriDlg.localImgBuffer, stats.width * stats.height * 4);
    free(conbriDlg.localImgBuffer);
    fitsChange();
    image->update();
    cbc = new FITSChangeCommand(this, CONTRAST_BRIGHTNESS, image->displayImage, image->templateImage);
    history->addCommand(cbc, false);

  }
  
  image->destroyTemplateImage();
    
}

void FITSViewer::imageHistogram()
{

  /*FITSHistogramCommand *histC;
  unsigned int * backupBuf = (unsigned int *) malloc (image->width * image->height * sizeof(unsigned int));
  if (backBuf == NULL)
  {
       KMessageBox::error(0, i18n("Not enough memory to complete the operation."));
       return;
  }
  memcpy(backupBuf, imgBuffer, width * height);*/
   
  //image->saveTemplateImage();
  
  if (histo == NULL)
  {
    histo = new FITSHistogram(this);
    histo->show();
  }
  else
  {
    histo->constructHistogram(imgBuffer);
    histo->updateBoxes();
    histo->show();
  }
    
  /*if (hist.exec() == QDialog::Rejected)
  {
    if (hist.napply > 0)
      for (int i=0; i < hist.napply; i++)
        history->undo();
    else
    {
    	image->reLoadTemplateImage();
    	image->zoomToCurrent();
    }
    //free (backupBuf);
  }
  else
  {
    if (hist.napply > 0) fitsChange();
    //histC = new FITSHistogramCommand(this, hist.type, backupBuf, image->displayImage, image->templateImage);
    //history->addCommand(histC, false);

  }
  
  image->destroyTemplateImage();*/
  
}

void FITSViewer::fitsRestore() 
{
 
 Dirty = 0;
 setCaption(currentURL.fileName());
 }

void FITSViewer::fitsChange() 
{
 
 Dirty = 1;
 
 setCaption(currentURL.fileName() + i18n(" [modified]"));
}

void FITSViewer::fitsStatistics()
{
  statForm stat(this);
  
  calculateStats();
  
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
   QString recordList;
   QString property;
   int equal, slash;

   fitsHeaderDialog header(this);
   header.headerView->setSorting(-1);
   header.headerView->setColumnAlignment(1, Qt::AlignHCenter);
   
   for (unsigned int i=0; i < record.count(); i++)
   {
     recordList = record[i];
     //recordList = QString((char *) record);
   
   	for (int j=0; j < FITS_RECORD_SIZE / FITS_CARD_SIZE; j++)
   	{
     	       property = recordList.left(FITS_CARD_SIZE);
	       
	       equal = property.find('=');
	       
	       if (equal == -1)
	       {
	        if (property.contains(" ") != FITS_CARD_SIZE)
	         	cards << property << "" << "";
		 recordList.remove(0, FITS_CARD_SIZE);
		 if (property.find("END") != -1)
		  break;
		 else
		  continue;
	       }
	       
	       
     	       cards << property.left(equal);
     	       slash = property.find("'");
     	       if (slash != -1)
       		slash = property.find("'", slash + 1) + 1;
     	       else
       		slash = property.find('/') - 1;
       
     		cards << property.mid(equal + 2, slash - (equal + 2)).simplifyWhiteSpace().remove("'");
     		cards << property.mid(slash + 1, FITS_CARD_SIZE - (slash + 1)).simplifyWhiteSpace();
		recordList.remove(0, FITS_CARD_SIZE);
		
	}
	
   }
   
   for (int k= cards.count() - 3; k >=0 ; k-=3)
       		   new QListViewItem( header.headerView, cards[k], cards[k+1], cards[k+2]);
  
   
   header.exec();

}


FITSChangeCommand::FITSChangeCommand(QWidget * parent, int inType, QImage* newIMG, QImage *oldIMG)
{
  viewer    = (FITSViewer *) parent;
  newImage  = new QImage();
  oldImage  = new QImage();
  *newImage = newIMG->copy();
  *oldImage = oldIMG->copy();
  type = inType;
}

FITSChangeCommand::~FITSChangeCommand() {}
            
void FITSChangeCommand::execute()
{

  viewer->image->displayImage = newImage;
  viewer->image->zoomToCurrent();
  viewer->fitsChange();

}

void FITSChangeCommand::unexecute()
{

  viewer->image->displayImage = oldImage;
  viewer->image->zoomToCurrent();

}

QString FITSChangeCommand::name() const
{
   switch (type)
   {
     case FITSViewer::CONTRAST_BRIGHTNESS:
            return i18n("Brightness/Contrast");
	    break;
     case FITSViewer::IMAGE_REDUCTION:
            return i18n("Image Reduction");
	    break;
     case FITSViewer::IMAGE_FILTER:
            return i18n("Image Filter");
	    break;
     default:
            return i18n("unknown");
	    break;
   }
}



#include "fitsviewer.moc"
