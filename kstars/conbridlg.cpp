/***************************************************************************
                          conbridlg.h  -  Contrast/Brightness Dialog
                             -------------------
    begin                : Fri Feb 6th 2004
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
 *  									   *
 ***************************************************************************/
 
 #include <klocale.h>
 #include <kimageeffect.h> 
 #include <kdebug.h>
 
 #include <qslider.h>
 #include <qimage.h>
 #include <qdatetime.h>
 #include <knuminput.h>
 
 #include <stdlib.h>
 
 #include "contrastbrightnessgui.h"
 #include "conbridlg.h"
 #include "fitsviewer.h"
 #include "fitsimage.h"
 
 #define REFRESH 500
 
//TODO find a better and faster way to implement this, this operation can be memory and CPU intensive.

ContrastBrightnessDlg::ContrastBrightnessDlg(QWidget *parent) :
    KDialogBase(KDialogBase::Plain, i18n( "Brightness/Contrast" ), Ok|Cancel, Ok, parent )
{
    
  contrast = brightness = 0;
  viewer = (FITSViewer *) parent;
  displayImage = viewer->image->displayImage;
  width  = displayImage->width();
  height = displayImage->height();
  
  
  ConBriDlg = new ConBriForm(this);
  if (!ConBriDlg) return;
  
  localImgBuffer = (unsigned char *) malloc (width * height * sizeof(unsigned char));
  if (!localImgBuffer)
  {
    kdDebug() << "Not enough memory for local image buffer" << endl;
    return;
  }
  for (int i=0; i < height; i++)
   memcpy(localImgBuffer + i * width, displayImage->scanLine(i), width);
  
  setMainWidget(ConBriDlg);
  this->show();
  
  connect(ConBriDlg->conSlider, SIGNAL( valueChanged(int)), this, SLOT (setContrast(int )));
  connect(ConBriDlg->briSlider, SIGNAL( valueChanged(int)), this, SLOT (setBrightness(int)));
  
}

ContrastBrightnessDlg::~ContrastBrightnessDlg()
{
  free (localImgBuffer);
}

void ContrastBrightnessDlg::range(int min, int max, int & num)
{
   if (num < min) num = min;
   else if (num > max) num = max;
}

void ContrastBrightnessDlg::setContrast(int contrastValue)
{
  int val=0;
  if (!viewer) return;
  QColor myCol;
  //unsigned char * data = viewer->image->templateImage->bits();
  contrast = contrastValue;
  
   // Apply Contrast and brightness
   for (int i=0; i < height; i++)
           for (int j=0; j < width; j++)
	   {
		val  = localImgBuffer[ i * width + j];
		if (contrast)
		{
			if (val < 128)
			{
		  		val -= contrast;
		 		range(0, 127, val);
		        }
		        else
		        {
		                val += contrast;
		                range(128, 255, val);
		        }
	        }
		if (brightness)
		{
			myCol.setRgb(val,val,val);
                	if ( brightness < 0 )
                		myCol = myCol.dark(100+(128-brightness));
                	else
                		myCol = myCol.light(100+(brightness));
			val   = myCol.red();
		}

		displayImage->setPixel(j, i, val);
	   }

  viewer->image->zoomToCurrent();
			  
}

void ContrastBrightnessDlg::setBrightness(int brightnessValue)
{
  int val = 0;
  if (!viewer) return;
  QColor myCol;
  //unsigned char * data = viewer->image->templateImage->bits();
  brightness = brightnessValue;

  // Apply Contrast and brightness
  for (int i=0; i < height; i++)
           for (int j=0; j < width; j++)
	   {
		val  = localImgBuffer[ i * width + j];
		if (contrast)
		{
			if (val < 128)
			{
		  		val -= contrast;
		                range(0, 127, val);
		        }
		        else
		       {
		                val += contrast;
		                range(128, 255, val);
		       }
		 }
		if (brightness)
		{
			myCol.setRgb(val,val,val);
                	if ( brightness < 0 )
                		myCol = myCol.dark(100+(128-brightness));
                	else
                		myCol = myCol.light(100+(brightness));
			val   = myCol.red();
		}
		 

		displayImage->setPixel(j, i, val);
	   }
  
  viewer->image->zoomToCurrent();
 
}

QSize ContrastBrightnessDlg::sizeHint() const
{
  return QSize(400,130);
}

#include "conbridlg.moc"
