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
 //#include "indi/fitsrw.h"
 
 #include <math.h>
 #include <stdlib.h>
 
 #include <QPainter>
 #include <QSlider>
 #include <QCursor>
 #include <QPen>
 #include <QPixmap>
 #include <QRadioButton>
 #include <QPushButton>
//Added by qt3to4:
#include <QMouseEvent>
#include <QPaintEvent>
 
 #include <kdebug.h>
 #include <klineedit.h>
 #include <klocale.h>

  histogramUI::histogramUI(QDialog *parent) : QDialog(parent)
 {
   setupUi(parent);
   setModal(false);
   
   /*minSlider->setMinimum(0);
   minSlider->setMaximum(BARS-1);
   minSlider->setValue(0);
   
   maxSlider->setMinimum(0);
   maxSlider->setMaximum(BARS-1);
   maxSlider->setValue(BARS-1);*/

   //histFrame->setCursor(Qt::CrossCursor);
   //histFrame->setMouseTracking(true);
   setMouseTracking(true);
 }

 FITSHistogram::FITSHistogram(QWidget *parent) : QDialog(parent)
 {
   viewer = (FITSViewer *) parent;
   ui = new histogramUI(this);

   histArray = NULL;

   type = 0;
   napply=0;
   
   /*connect(ui->minSlider, SIGNAL(valueChanged(int)), this, SLOT(updateBoxes()));
   connect(ui->minSlider, SIGNAL(valueChanged(int)), this, SLOT(updateIntenFreq(int )));
   connect(ui->maxSlider, SIGNAL(valueChanged(int)), this, SLOT(updateBoxes()));
   connect(ui->maxSlider, SIGNAL(valueChanged(int)), this, SLOT(updateIntenFreq(int )));*/
   connect(ui->applyB, SIGNAL(clicked()), this, SLOT(applyScale()));

   //constructHistogram();
   
   updateBoxes();
   
}
 
 FITSHistogram::~FITSHistogram() 
{
free (histArray);

}
 
void FITSHistogram::updateBoxes()
{
/*

	double fits_min=0, fits_max=0;

	viewer->image->getFITSMinMax(&fits_min, &fits_max);

        if (ui->minSlider->value() == BARS)
	 ui->minOUT->setText(QString::number((int) fits_max));
	else
   	 ui->minOUT->setText(QString::number( (int) ( ceil (ui->minSlider->value() * binSize) + fits_min)));
	 
	if (ui->maxSlider->value() == BARS)
	 ui->maxOUT->setText(QString::number((int) fits_max));
	else
   	 ui->maxOUT->setText(QString::number( (int) ( ceil (ui->maxSlider->value() * binSize) + fits_min)));

        update();

*/
}

void FITSHistogram::applyScale()
{
/*
  int swap;
  double fits_min=0, fits_max=0;

  int min = ui->minSlider->value();
  int max = ui->maxSlider->value();

  viewer->image->getFITSMinMax(&fits_min, &fits_max);

  FITSHistogramCommand *histC;
  //kDebug() << "Width " << viewer->image->width << endl;
  
  if (min > max)
  {
    swap = min;
    min  = max;
    max  = swap;
  }

   min  = (int) (min * binSize + fits_min);
   max  = (int) (max * binSize + fits_min);
  
  napply++;
  
  // Auto
  if (ui->autoR->isChecked())
    type = 0;
  // Linear
  else if (ui->linearR->isChecked())
    type = 1;
  // Log
  else if (ui->logR->isChecked())
    type = 2;
  // Exp
  else if (ui->sqrtR->isChecked())
    type = 3;
  
  histC = new FITSHistogramCommand(viewer, this, type, min, max);
  viewer->history->addCommand(histC);
    */
}
 
void FITSHistogram::constructHistogram(int hist_height, int hist_width)
{
 
   int id;
  double fits_min=0, fits_max=0;
  double fits_w=0, fits_h=0;
  float *buffer = viewer->image->getImageBuffer();

  viewer->image->getFITSSize(&fits_w, &fits_h);
  viewer->image->getFITSMinMax(&fits_min, &fits_max);

  int pixel_range = (int) (fits_max - fits_min);
 
 histArray = (histArray == NULL) ? (int *) calloc(hist_width , sizeof(int)) :
       					           (int *) realloc(histArray, (hist_width+1) * sizeof(int));

  // Panic
 if (histArray == NULL)
	return;

  binSize = ((double) hist_width / (double) pixel_range); 

    if (binSize == 0 || buffer == NULL)
     return;

     for (int i=0; i < fits_w * fits_h; i++)
     {
        id = (int) ((buffer[i] - fits_min) * binSize);
        if (id >= hist_width) id = hist_width - 1;
        histArray[id]++;
	// When binSize > 1, we need to increase the next ID because it will be skipped.
	// Example: We have 0-200 pixel values, and we want to display them over 400 pixels. Therefore, binSize = 2
	// Pixel value 19 will have id=38, while the next pixel value 20 will have have id=40, thus leaving histArray[39] always empty, when it's
	// Suppose to correspond to pixel value 19 as well (Two pixels per value, or binSize 2)
	if (binSize > 1) histArray[id+1]++;
     }

    // Normalize histogram height. i.e. the maximum value will take the whole height of the widget
    histFactor = ((double) hist_height) / ((double) findMax(hist_width));

    for (int i=0; i < hist_width; i++)
		histArray[i] = (int) (((double) histArray[i]) * histFactor);

     ui->update();
}


void FITSHistogram::updateIntenFreq(int x)
{
 #if 0
  int index = (int) ceil(x * binSize);
    
  ui->intensityOUT->setText(QString::number((int) ( index + viewer->stats.min)));
  
  ui->frequencyOUT->setText(QString::number(histArray[x]));

 #endif
  
}


int FITSHistogram::findMax(int hist_width)
{
	int max =-1e9;

  	for (int i=0; i < hist_width; i++)
    		if (histArray[i] > max) max = histArray[i];
    
  	return max;
}

FITSHistogramCommand::FITSHistogramCommand(QWidget * parent, FITSHistogram *inHisto, int newType, int lmin, int lmax)
{
 #if 0
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
 #endif
}

FITSHistogramCommand::~FITSHistogramCommand() 
{
  free(buffer);
  delete (oldImage);
}
            
void FITSHistogramCommand::execute()
{
  #if 0
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
  #endif
}

void FITSHistogramCommand::unexecute()
{
 #if 0
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
 #endif
}

QString FITSHistogramCommand::name() const
{

 #if 0
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
 
 #endif
 return i18n("Unknown");
 
}

#include "fitshistogram.moc"
