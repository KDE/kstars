/***************************************************************************
                          FITSViewer.cpp  -  A FITSViewer for KStars
                             -------------------
    begin                : Thu Jan 22 2004
    copyright            : (C) 2004 by Jasem Mutlaq
    email                : mutlaqja@ikarustech.com

 2006-03-03	Using CFITSIO, Porting to Qt4
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#include <klocale.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <kaction.h>
#include <kstdaction.h>

#include <kdebug.h>
#include <ktoolbar.h>
#include <kapplication.h>
#include <ktempfile.h>
#include <kimageeffect.h>
#include <kmenubar.h>
#include <kstatusbar.h>
#include <kcommand.h>
#include <klineedit.h>
#include <kicon.h>


#include <qfile.h>
#include <q3vbox.h>
#include <qcursor.h>
#include <qstringlist.h>
#include <q3listview.h>
#include <qradiobutton.h>
#include <qclipboard.h>
#include <qimage.h>

#include <QKeyEvent>
#include <QCloseEvent>
#include <QTreeWidget>
#include <QHeaderView>

#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>

#include "fitsviewer.h"
#include "fitsimage.h"
#include "fitshistogram.h"
#include "ui_statform.h"
#include "ui_fitsheaderdialog.h"
#include "ksutils.h"
#include "Options.h"

FITSViewer::FITSViewer (const KUrl *url, QWidget *parent, const char *name)
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

    statusBar()->insertItem(QString(), 0);
    statusBar()->setItemFixed(0, 100);
    statusBar()->insertItem(QString(), 1);
    statusBar()->setItemFixed(1, 100);
    statusBar()->insertItem(QString(), 2);
    statusBar()->setItemFixed(2, 100);
    statusBar()->insertItem(QString(), 3);
    statusBar()->setItemFixed(3, 50);
    statusBar()->insertPermanentItem(i18n("Welcome to KStars FITS Viewer"), 4, 1);
    statusBar()->setItemAlignment(4 , Qt::AlignLeft);

    /* FITS initializations */
    if (!initFITS())
    {
     close();
     return;
    }

    QFile tempFile;
    KAction *action;

     /*if (KSUtils::openDataFile( tempFile, "bricon.png" ) )
    {
    	new KAction( i18n("Brightness/Contrast"), tempFile.name(), KShortcut( "Ctrl+T" ), this, SLOT( BrightContrastDlg()), actionCollection(), "image_brightness_contrast");
	tempFile.close();
    }
    else
       	action = new KAction(KIcon("contrast+"),  i18n("Brightness/Contrast"), actionCollection(), "image_brightness_contrast");
       	connect(action, SIGNAL(triggered(bool)), SLOT( BrightContrastDlg()));
       	action->setShortcut(KShortcut( Qt::CTRL+Qt::Key_T ));*/

    if (KSUtils::openDataFile( tempFile, "histogram.png" ) )
    {
    	action = new KAction(KIcon(tempFile.fileName()),  i18n("Histogram"), actionCollection(), "image_histogram");
    	connect(action, SIGNAL(triggered(bool) ), SLOT (imageHistogram()));
    	action->setShortcut(KShortcut( Qt::CTRL+Qt::Key_H ));
	tempFile.close();
    }
    else {
        action = new KAction(KIcon("wizard"),  i18n("Histogram"), actionCollection(), "image_histogram");
        connect(action, SIGNAL(triggered(bool)), SLOT (imageHistogram()));
        action->setShortcut(KShortcut( Qt::CTRL+Qt::Key_H ));
    }

    KStdAction::open(this, SLOT(fileOpen()), actionCollection());
    KStdAction::save(this, SLOT(fileSave()), actionCollection());
    KStdAction::saveAs(this, SLOT(fileSaveAs()), actionCollection());
    KStdAction::close(this, SLOT(slotClose()), actionCollection());
    KStdAction::copy(this, SLOT(fitsCOPY()), actionCollection());
    KStdAction::zoomIn(image, SLOT(fitsZoomIn()), actionCollection());
    KStdAction::zoomOut(image, SLOT(fitsZoomOut()), actionCollection());
    action = new KAction(KIcon("viewmagfit.png"),  i18n( "&Default Zoom" ), actionCollection(), "zoom_default" );
    connect(action, SIGNAL(triggered(bool) ), image, SLOT(fitsZoomDefault()));
    action->setShortcut(KShortcut( Qt::CTRL+Qt::Key_D ));
    action = new KAction(KIcon("sum"),  i18n( "Statistics"), actionCollection(), "image_stats");
    connect(action, SIGNAL(triggered(bool)), SLOT(fitsStatistics()));
    action = new KAction(KIcon("frame_spreadsheet.png"),  i18n( "FITS Header"), actionCollection(), "fits_editor");
    connect(action, SIGNAL(triggered(bool) ), SLOT(fitsHeader()));

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

    /* Display image in the central widget */
    if (image->loadFits(currentURL.path().toAscii()) == -1) { close(); return false; }

    /* Clear history */
    history->clear();

    /* Set new file caption */
    setWindowTitle(currentURL.fileName());

   statusBar()->changeItem( QString("%1 x %2").arg( (int) image->stats.dim[0]).arg( (int) image->stats.dim[1]), 2);

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
//  while ((msg = fits_get_error ()) != NULL)
   //  KMessageBox::error(0, msg);
}

float * FITSViewer::loadData(const char *filename, float *buffer)
{
/*  FILE *fp;
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
                                              */
  return NULL;

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

   KUrl fileURL = KFileDialog::getOpenUrl( QDir::homePath(), "*.fits *.fit *.fts|Flexible Image Transport System");

  if (fileURL.isEmpty())
    return;


  currentURL = fileURL;

  initFITS();

}

void FITSViewer::fileSave()
{
 /*
  FITS_FILE *ifp;
  QString recordList;
  KUrl backupCurrent = currentURL;
  QString bitpixRec;
  FITS_BITPIX16  pixval_16  =0;
  FITS_BITPIX32  pixval_32  =0;
  FITS_BITPIXM32 pixval_m32 =0;
  FITS_BITPIXM64 pixval_m64 =0;
  unsigned char *transData;
  int index=0, i=0, transCount = 0, totalCount= image->width * image->height;

  QString currentDir = Options::fitsSaveDirectory();

  //kDebug() << "We doing stats BEFORE we save!! " << endl;
  //calculateStats();

  // If no changes made, return.
  if (Dirty == 0 && !currentURL.isEmpty())
    return;

  if (currentURL.isEmpty())
  {
  	currentURL = KFileDialog::getSaveUrl( currentDir, "*.fits |Flexible Image Transport System");
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

  	ifp = fits_open (currentURL.path().toAscii(), "w");
        if (ifp == NULL)
        {
          KMessageBox::error(0, i18n("Error during open of FITS file."));
          return;
        }

	setbuf(ifp->fp, NULL);

	bitpixRec.sprintf("BITPIX  =                   %02d /Modified by KStars                              ", image->bitpix);
	bitpixRec.truncate(80);

	for (int j=0; j < record.count(); j++)
	{
	  recordList = record[j];

	  if ( (index = recordList.indexOf("BITPIX")) != -1)
	  	recordList.replace(index, FITS_CARD_SIZE, bitpixRec);

	  fwrite(recordList.toAscii(), 1, FITS_RECORD_SIZE, ifp->fp);
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

	totalCount = (totalCount * image->bpp) % FITS_RECORD_SIZE;
	if (totalCount)
  	{
    		while (totalCount++ < FITS_RECORD_SIZE)
      		putc (0, ifp->fp);
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

*/
}

void FITSViewer::fileSaveAs()
{

  currentURL = QString();
  fileSave();
}

void FITSViewer::fitsCOPY()
{
   kapp->clipboard()->setImage(*image->displayImage);
}

void FITSViewer::updateImgBuffer()
{
 #if 0
  int width = image->width;
  int height = image->height;

  for (int i=0; i < height; i++)
    for (int j=0; j < width; j++)
       imgBuffer[i * width + j] = (int) *(image->displayImage->scanLine(height - i - 1) + j);
       //image->reducedImgBuffer[i * width + j];

   calculateStats();
 #endif
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
    //histo->constructHistogram(imgBuffer);
    //histo->updateBoxes();
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
 setWindowTitle(currentURL.fileName());
 }

void FITSViewer::fitsChange()
{

 Dirty = 1;

 setWindowTitle(currentURL.fileName() + i18n(" [modified]"));
}

void FITSViewer::fitsStatistics()
{
  QDialog statDialog;
  Ui::statForm stat;
  stat.setupUi(&statDialog);

  //calculateStats();

  stat.widthOUT->setText(QString::number(image->stats.dim[0]));
  stat.heightOUT->setText(QString::number(image->stats.dim[1]));
  stat.bitpixOUT->setText(QString::number(image->stats.bitpix));
  stat.maxOUT->setText(QString::number(image->stats.max));
  stat.minOUT->setText(QString::number(image->stats.min));
  stat.meanOUT->setText(QString::number(image->stats.average));
  stat.stddevOUT->setText(QString::number(image->stats.stddev));

  statDialog.exec();
}

void FITSViewer::fitsHeader()
{/*
   QStringList cards;
   QString recordList;
   QString property;
   int equal, slash;

   QDialog fitsHeaderDialog;
   Ui::fitsHeaderDialog header;
   header.setupUi(&fitsHeaderDialog);
   header.headerView->setSortingEnabled(false);
   header.headerView->header()->setDefaultAlignment(Qt::AlignHCenter);

   for (int i=0; i < record.count(); i++)
   {
     recordList = record[i];
     //recordList = QString((char *) record);

   	for (int j=0; j < FITS_RECORD_SIZE / FITS_CARD_SIZE; j++)
   	{
     	       property = recordList.left(FITS_CARD_SIZE);

	       equal = property.indexOf('=');

	       if (equal == -1)
	       {
	        if (property.contains(" ") != FITS_CARD_SIZE)
	         	cards << property << QString() << QString();
		 recordList.remove(0, FITS_CARD_SIZE);
		 if (property.indexOf("END") != -1)
		  break;
		 else
		  continue;
	       }


     	       cards << property.left(equal);
     	       slash = property.indexOf("'");
     	       if (slash != -1)
       		slash = property.indexOf("'", slash + 1) + 1;
     	       else
       		slash = property.indexOf('/') - 1;

     		cards << property.mid(equal + 2, slash - (equal + 2)).simplified().remove("'");
     		cards << property.mid(slash + 1, FITS_CARD_SIZE - (slash + 1)).simplified();
		recordList.remove(0, FITS_CARD_SIZE);

	}

   }

   QTreeWidgetItem *tempItem;

   for (int k= cards.count() - 3; k >=0 ; k-=3)
	{
		   tempItem = new QTreeWidgetItem(header.headerView);
		   tempItem->setText(0, cards[k]);
		   tempItem->setText(1, cards[k+1]);
		   tempItem->setText(2, cards[k+2]);
	}


   fitsHeaderDialog.exec();
*/
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

FITSChangeCommand::~FITSChangeCommand() 
{
  delete newImage;
  delete oldImage;
}

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
