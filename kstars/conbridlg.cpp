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
 
 #include <tqslider.h>
 #include <tqimage.h>
 #include <tqdatetime.h>
 #include <knuminput.h>
 
 #include <stdlib.h>
 
 #include "contrastbrightnessgui.h"
 #include "conbridlg.h"
 #include "fitsviewer.h"
 #include "fitsimage.h"
 
 #define REFRESH 500
 
//TODO find a better and faster way to implement this, this operation can be memory and CPU intensive.

ContrastBrightnessDlg::ContrastBrightnessDlg(TQWidget *parent) :
    KDialogBase(KDialogBase::Plain, i18n( "Brightness/Contrast" ), Ok|Cancel, Ok, parent )
{
    
  float pixdiff, datadiff;
  contrast = brightness = 0;
  viewer = (FITSViewer *) parent;
  displayImage = viewer->image->displayImage;
  tempImage    = new TQImage(displayImage->copy());
  width  = displayImage->width();
  height = displayImage->height();
  
  datadiff = 255;
  pixdiff  = viewer->stats.max - viewer->stats.min;
  offs = - (viewer->stats.min * datadiff / pixdiff);
  scale = datadiff / pixdiff;
  
  ConBriDlg = new ConBriForm(this);
  if (!ConBriDlg) return;
  
  localImgBuffer = (float *) malloc (width * height * sizeof(float));
  if (!localImgBuffer)
  {
    kdDebug() << "Not enough memory for local image buffer" << endl;
    return;
  }
  
  memcpy(localImgBuffer, viewer->imgBuffer, width * height * sizeof(float));
  
  setMainWidget(ConBriDlg);
  show();
  
  connect(ConBriDlg->conSlider, TQT_SIGNAL( valueChanged(int)), this, TQT_SLOT (setContrast(int )));
  connect(ConBriDlg->briSlider, TQT_SIGNAL( valueChanged(int)), this, TQT_SLOT (setBrightness(int)));
  
}

ContrastBrightnessDlg::~ContrastBrightnessDlg()
{
  delete (tempImage);
}

void ContrastBrightnessDlg::range(int min, int max, int & num)
{
   if (num < min) num = min;
   else if (num > max) num = max;
}

void ContrastBrightnessDlg::range(float min, float max, float & num)
{
   if (num < min) num = min;
   else if (num > max) num = max;
}


#include <math.h>

void ContrastBrightnessDlg::setContrast(int contrastValue)
{
  int val = 0, index=0, totalPix = width * height;
  int min = (int) viewer->imgBuffer[0], max = 0;
  if (!viewer) return;
  TQColor myCol;
  contrast = contrastValue;

 
  // Apply Contrast and brightness
  for (int i=0 ; i < height ; i++)
           for (int j=0; j < width; j++)
	   {
		val  = (int) *(viewer->image->templateImage->scanLine(i) + j);
		
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
                		val += brightness;
                	else
			{      
			        myCol = myCol.light(100+(brightness));
				val   = myCol.red();
			}
			
			range(0, 255, val);
		}
		 
		localImgBuffer[(height - i - 1) * width + j] = (val - offs) / scale;
	   }
	   
	   
  
  for (int i=0; i < totalPix; i++)
  {
    if (localImgBuffer[i] < min) min = (int) localImgBuffer[i];
    else if (localImgBuffer[i] > max) max = (int) localImgBuffer[i];
  }
  
  float pixdiff_b  = max - min;
  float offs_b     = - (min * 255 / pixdiff_b);
  float scale_b    = 255 / pixdiff_b;
  
  for (int i=0; i < height; i++)
  	for (int j=0; j < width; j++)
	{
	        index = i * width + j;
		val = (int) (localImgBuffer[index] * scale_b + offs_b);
		range(0, 255, val);
  		displayImage->setPixel(j, height - i - 1, val);
	}
  
  viewer->image->zoomToCurrent();			  
}

void ContrastBrightnessDlg::setBrightness(int brightnessValue)
{
  int val = 0, index=0, totalPix = width * height;
  int min = (int) viewer->imgBuffer[0], max = 0;
  if (!viewer) return;
  TQColor myCol;
  brightness = brightnessValue;

  // Apply Contrast and brightness
  for (int i=0 ; i < height ; i++)
           for (int j=0; j < width; j++)
	   {
		val  = (int) *(viewer->image->templateImage->scanLine(i) + j);
		
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
                		val += brightness;
                	else
			{      
			        myCol = myCol.light(100+(brightness));
				val   = myCol.red();
			}
			
			range(0, 255, val);
		}
		 
		localImgBuffer[(height - i - 1) * width + j] = (val - offs) / scale;
		
		
	   }
  
  for (int i=0; i < totalPix; i++)
  {
    if (localImgBuffer[i] < min) min = (int) localImgBuffer[i];
    else if (localImgBuffer[i] > max) max = (int) localImgBuffer[i];
  }
  
  float pixdiff_b  = max - min;
  float offs_b     = - (min * 255 / pixdiff_b);
  float scale_b    = 255 / pixdiff_b;
  
  for (int i=0; i < height; i++)
  	for (int j=0; j < width; j++)
	{
	        index = i * width + j;
		val = (int) (localImgBuffer[index] * scale_b + offs_b);
		range(0, 255, val);
  		displayImage->setPixel(j, height - i - 1, val);
	}
   
  viewer->image->zoomToCurrent();
 
}

TQSize ContrastBrightnessDlg::sizeHint() const
{
  return TQSize(400,130);
}

#include "conbridlg.moc"
