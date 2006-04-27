/***************************************************************************
                          fitshistogram.cpp  -  FITS Historgram
                          -----------------
    begin                : Thu Mar 4th 2004
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
 ***************************************************************************/
 
 #include "fitshistogram.h"
 #include "fitsviewer.h"
 #include "fitsimage.h"
 #include "indi/fitsrw.h"
 
 #include <math.h>
 #include <stdlib.h>
 
 #include <qpainter.h>
 #include <qslider.h>
 #include <qcursor.h>
 #include <qpen.h>
 #include <qpixmap.h>
 #include <qradiobutton.h>
 #include <qpushbutton.h>
 
 #include <kdebug.h>
 #include <klineedit.h>
 #include <klocale.h>
 
 
 
 FITSHistogram::FITSHistogram(QWidget *parent, const char * name) : histDialog(parent, name)
 {
   viewer = (FITSViewer *) parent;
   
   setModal(false);
   
   minSlider->setMinValue(0);
   minSlider->setMaxValue(BARS-1);
   minSlider->setValue(0);
   
   maxSlider->setMinValue(0);
   maxSlider->setMaxValue(BARS-1);
   maxSlider->setValue(BARS-1);
   
   type = 0;
   napply=0;
   
   histFrame->setCursor(Qt::CrossCursor);
   histFrame->setMouseTracking(true);
   setMouseTracking(true);
   
   connect(minSlider, SIGNAL(valueChanged(int)), this, SLOT(updateBoxes()));
   connect(minSlider, SIGNAL(valueChanged(int)), this, SLOT(updateIntenFreq(int )));
   connect(maxSlider, SIGNAL(valueChanged(int)), this, SLOT(updateBoxes()));
   connect(maxSlider, SIGNAL(valueChanged(int)), this, SLOT(updateIntenFreq(int )));
   connect(applyB, SIGNAL(clicked()), this, SLOT(applyScale()));

   constructHistogram(viewer->imgBuffer);
   
   updateBoxes();
   
}
 
 FITSHistogram::~FITSHistogram() {}
 
void FITSHistogram::updateBoxes()
{
        if (minSlider->value() == BARS)
	 minOUT->setText(QString("%1").arg((int) viewer->stats.max));
	else
   	 minOUT->setText(QString("%1").arg( (int) ( ceil (minSlider->value() * binSize) + viewer->stats.min)));
	 
	if (maxSlider->value() == BARS)
	 maxOUT->setText(QString("%1").arg((int) viewer->stats.max));
	else
   	 maxOUT->setText(QString("%1").arg( (int) ( ceil (maxSlider->value() * binSize) + viewer->stats.min)));

        update();
}

void FITSHistogram::applyScale()
{
  int swap;
  int min = minSlider->value();
  int max = maxSlider->value();
  
  FITSHistogramCommand *histC;
  //kdDebug() << "Width " << viewer->image->width << endl;
  
  if (min > max)
  {
    swap = min;
    min  = max;
    max  = swap;
  }
  
   min  = (int) (min * binSize + viewer->stats.min);
   max  = (int) (max * binSize + viewer->stats.min);

  
  napply++;
  
  // Auto
  if (autoR->isOn())
    type = 0;
  // Linear
  else if (linearR->isOn())
    type = 1;
  // Log
  else if (logR->isOn())
    type = 2;
  // Exp
  else if (sqrtR->isOn())
    type = 3;
  
  histC = new FITSHistogramCommand(viewer, this, type, min, max);
  viewer->history->addCommand(histC);
    
}
 
void FITSHistogram::constructHistogram(float * buffer)
{
  int maxHeight = 0;
  int height    = histFrame->height(); 
  int id;
  int index;
  int range = (int) (viewer->stats.max - viewer->stats.min);
    
  for (int i=0; i < BARS; i++)
      histArray[i] = 0;
    binSize = ( (double) range / (double) BARS); 
    
    //kdDebug() << "#2" << endl; 
    if (binSize == 0 || buffer == NULL)
     return;
    
     for (int i=0; i < viewer->stats.width * viewer->stats.height; i++)
     {
        id = (int) ((buffer[i] - viewer->stats.min) / binSize);
        if (id >= BARS) id = BARS - 1;
        histArray[id]++;
	
     }
     
     if (binSize < 1)
     for (int i=0; i < BARS - 1; i++)
         if (histArray[i] == 0)
	 {
	    index = (int) ceil(i * binSize);
            if (index == (int) (ceil ((i+1) * binSize)))
                histArray[i] = histArray[i+1];
         }
  
 maxHeight = findMax() / height;
 
 kdDebug() << "Maximum height is " << maxHeight << " -- binsize " << binSize << endl;
 
 histogram = new QPixmap(500, 150, 1);
 histogram->fill(Qt::black);
 QPainter p(histogram);
 QPen pen( white, 1);
 p.setPen(pen);
   
 for (int i=0; i < BARS; i++)
     p.drawLine(i, height , i, height - (int) ((double) histArray[i] / (double) maxHeight)); 
  
}

void FITSHistogram::paintEvent( QPaintEvent */*e*/)
{
  int height    = histFrame->height(); 
  int xMin = minSlider->value(), xMax = maxSlider->value();
  
  QPainter p(histFrame);
  QPen pen;
  pen.setWidth(1);
  
  bitBlt(histFrame, 0, 0, histogram);
  
  pen.setColor(blue);
  p.setPen(pen);
  
  p.drawLine(xMin, height - 2, xMin, height/2 -2);
  pen.setColor(red);
  p.setPen(pen);
  p.drawLine(xMax, 2, xMax, height/2 -2);
  
  
}

void FITSHistogram::mouseMoveEvent( QMouseEvent *e)
{
  int x = e->x();
  int y = e->y();
  
  x -= histFrame->x();
  y -= histFrame->y();
  
  if (x < 0 || x >= BARS || y < 0 || y > histFrame->height() )
   return;
  
  updateIntenFreq(x);
  
}

void FITSHistogram::updateIntenFreq(int x)
{

  int index = (int) ceil(x * binSize);
    
  intensityOUT->setText(QString("%1").arg((int) ( index + viewer->stats.min)));
  
  frequencyOUT->setText(QString("%1").arg(histArray[x]));
  
}


int FITSHistogram::findMax()
{
  int max =0;
  
  for (int i=0; i < BARS; i++)
    if (histArray[i] > max) max = histArray[i];
    
  return max;
}

FITSHistogramCommand::FITSHistogramCommand(QWidget * parent, FITSHistogram *inHisto, int newType, int lmin, int lmax)
{
  viewer    = (FITSViewer *) parent;
  type      = newType;
  histo     = inHisto;
  oldImage  = new QImage();
  // TODO apply maximum compression against this buffer
  buffer = (float *) malloc (viewer->image->width * viewer->image->height * sizeof(float));
   
  //if (buffer == NULL)
   //return;
   min = lmin;
   max = lmax;
}

FITSHistogramCommand::~FITSHistogramCommand() 
{
  free(buffer);
  delete (oldImage);
}
            
void FITSHistogramCommand::execute()
{
  float val, bufferVal;
  double coeff;
  FITSImage *image = viewer->image;
  int width  = image->width;
  int height = image->height;
  
  memcpy(buffer, viewer->imgBuffer,image->width * image->height * sizeof(float));
  *oldImage = image->displayImage->copy();
 
  switch (type)
  {
    case FITSImage::FITSAuto:
    case FITSImage::FITSLinear:
    for (int i=0; i < height; i++)
      for (int j=0; j < width; j++)
      {
              bufferVal = viewer->imgBuffer[i * width + j];
	      if (bufferVal < min) bufferVal = min;
	      else if (bufferVal > max) bufferVal = max;
	      //val = (int) (255. * ((double) (bufferVal - min) / (double) (max - min)));
	      //val = (int) (max * ((double) (bufferVal - min) / (double) (max - min)));
	      //val = (int) (bufferVal - min) * (max - min) + min;
	      //if (val < min) val = min;
	      //else if (val > max) val = max;
	      //image->reducedImgBuffer[i * width + j] = val;
	      viewer->imgBuffer[i * width + j] = bufferVal;
      	      
      }
     break;
     
    case FITSImage::FITSLog:
    //coeff = 255. / log(1 + max);
    coeff = max / log(1 + max);
    
    for (int i=0; i < height; i++)
      for (int j=0; j < width; j++)
      {
              bufferVal = viewer->imgBuffer[i * width + j];
	      if (bufferVal < min) bufferVal = min;
	      else if (bufferVal > max) bufferVal = max;
	      val = (coeff * log(1 + bufferVal));
	      if (val < min) val = min;
	      else if (val > max) val = max;
	      viewer->imgBuffer[i * width + j] = val;
	      //image->reducedImgBuffer[i * width + j] = val;
      	      //displayImage->setPixel(j, height - i - 1, val);
      }
      break;
      
    case FITSImage::FITSSqrt:
    //coeff = 255. / sqrt(max);
    coeff = max / sqrt(max);
    
    for (int i=0; i < height; i++)
      for (int j=0; j < width; j++)
      {
              bufferVal = (int) viewer->imgBuffer[i * width + j];
	      if (bufferVal < min) bufferVal = min;
	      else if (bufferVal > max) bufferVal = max;
	      val = (int) (coeff * sqrt(bufferVal));
	      //image->reducedImgBuffer[i * width + j] = val;
	      viewer->imgBuffer[i * width + j] = val;
      	      //displayImage->setPixel(j, height - i - 1, val);
      }
      
      break;
    
     
    default:
     break;
  }
       
   //int lmin= image->reducedImgBuffer[0];
   float lmin= viewer->imgBuffer[0];
   float lmax= viewer->imgBuffer[0];
   int totalPix = width * height;
   
   for (int i=1; i < totalPix; i++)
    //if ( image->reducedImgBuffer[i] < lmin) lmin = image->reducedImgBuffer[i];
    if ( viewer->imgBuffer[i] < lmin) lmin = viewer->imgBuffer[i];
    else if (viewer->imgBuffer[i] > lmax) lmax = viewer->imgBuffer[i];
    
   double datadiff = 255.;
   double pixdiff = lmax - lmin;
   double offs = -lmin * datadiff / pixdiff;
   double scale = datadiff / pixdiff;
   int tdata = 0;
  
     
 for (int i=0; i < height; i++)
  for (int j=0; j < width; j++)
  {
           //bp8 = (FITS_BITPIX8) image->reducedImgBuffer[i * width + j];
	   //bp8 = (FITS_BITPIX8) 
           tdata = (long) (viewer->imgBuffer[i * width + j] * scale + offs);
           if (tdata < 0) tdata = 0;
           else if (tdata > 255) tdata = 255;
	   image->displayImage->setPixel(j, height - i - 1, tdata);
  }
  
  viewer->calculateStats();
  
  //viewer->updateImgBuffer();
  if (histo != NULL)
  {
  	histo->constructHistogram(viewer->imgBuffer);
  	histo->update();
  	histo->updateBoxes();
  }
  
  viewer->image->zoomToCurrent();
  viewer->fitsChange();
  
}

void FITSHistogramCommand::unexecute()
{
  memcpy( viewer->imgBuffer, buffer, viewer->image->width * viewer->image->height * sizeof(float));
  viewer->calculateStats();
  *viewer->image->displayImage = oldImage->copy();
  viewer->image->zoomToCurrent();
  
  if (histo != NULL)
  {
  	histo->constructHistogram(viewer->imgBuffer);
  	histo->update();
  	histo->updateBoxes();
  }
}

QString FITSHistogramCommand::name() const
{

 switch (type)
 {
  case 0:
    return i18n("Auto Scale");
    break;
  case 1:
    return i18n("Linear Scale");
    break;
  case 2:
    return i18n("Logarithmic Scale");
    break;  
  case 3:
    return i18n("Square Root Scale");
    break;
  default:
   break;
 }
 
 return i18n("Unknown");

}



 

#include "fitshistogram.moc"
