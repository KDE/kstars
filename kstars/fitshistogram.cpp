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

   type = 0;
   napply=0;
   
   /*connect(ui->minSlider, SIGNAL(valueChanged(int)), this, SLOT(updateBoxes()));
   connect(ui->minSlider, SIGNAL(valueChanged(int)), this, SLOT(updateIntenFreq(int )));
   connect(ui->maxSlider, SIGNAL(valueChanged(int)), this, SLOT(updateBoxes()));
   connect(ui->maxSlider, SIGNAL(valueChanged(int)), this, SLOT(updateIntenFreq(int )));*/
   connect(ui->applyB, SIGNAL(clicked()), this, SLOT(applyScale()));

   constructHistogram(viewer->image->getImageBuffer());
   
   updateBoxes();
   
}
 
 FITSHistogram::~FITSHistogram() {}
 
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
 
void FITSHistogram::constructHistogram(float * buffer)
{
 
  int maxHeight = 0;
 int height    = ui->histFrame->height(); 
  int id;
  int index;
  double fits_min=0, fits_max=0;
  double fits_w=0, fits_h=0;

  viewer->image->getFITSSize(&fits_w, &fits_h);
  viewer->image->getFITSMinMax(&fits_min, &fits_max);

  int range = (int) (fits_max - fits_min);
    
  for (int i=0; i < BARS; i++)
      histArray[i] = 0;
    binSize = ( (double) range / (double) BARS); 
    
    if (binSize == 0 || buffer == NULL)
     return;
    
     for (int i=0; i < fits_w * fits_h; i++)
     {
        id = (int) ((buffer[i] - fits_min) / binSize);
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
 
 kDebug() << "Maximum height is " << maxHeight << " -- binsize " << binSize << endl;

 ui->update();
}


void FITSHistogram::paintEvent( QPaintEvent */*e*/)
{
/*
  //int height    = ui->histFrame->height(); 
  int xMin = ui->minSlider->value(), xMax = ui->maxSlider->value();
  
  QPainter painter(ui->histFrame);
  QPen pen;
  pen.setWidth(1);
  pen.setColor(Qt::white);

  painter.setPen(pen);
  
  //QPen pen( Qt::white, 1);
  // p.setPen(pen);
   
 //for (int i=0; i < BARS; i++)
    // painter.drawLine(i, height , i, height - (int) ((double) histArray[i] / (double) maxHeight)); 

  painter.drawPie(0,0, 50, 50, 30, 120);

  //bitBlt(ui->histFrame, 0, 0, histogram);
  
  //pen.setColor(Qt::blue);
  //p.setPen(pen);
  
  //p.drawLine(xMin, height - 2, xMin, height/2 -2);
  //pen.setColor(Qt::red);
  //p.setPen(pen);
  //p.drawLine(xMax, 2, xMax, height/2 -2);
  
  */
}

void FITSHistogram::mouseMoveEvent( QMouseEvent *e)
{
  int x = e->x();
  int y = e->y();
  
  //x -= ui->histFrame->x();
  //y -= ui->histFrame->y();
  
  //if (x < 0 || x >= BARS || y < 0 || y > ui->histFrame->height() )
   //return;
  
  updateIntenFreq(x);
  
}

void FITSHistogram::updateIntenFreq(int x)
{
 #if 0
  int index = (int) ceil(x * binSize);
    
  ui->intensityOUT->setText(QString::number((int) ( index + viewer->stats.min)));
  
  ui->frequencyOUT->setText(QString::number(histArray[x]));

 #endif
  
}


int FITSHistogram::findMax()
{
int max =0;
/*
  
  
  for (int i=0; i < BARS; i++)
    if (histArray[i] > max) max = histArray[i];
    */
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
