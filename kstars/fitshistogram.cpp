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
   
   minSlider->setMinValue(0);
   minSlider->setMaxValue(BARS);
   minSlider->setValue(0);
   
   maxSlider->setMinValue(0);
   maxSlider->setMaxValue(BARS);
   maxSlider->setValue(BARS);
   
   type = 0;
   
   histFrame->setCursor(Qt::CrossCursor);
   histFrame->setMouseTracking(true);
   setMouseTracking(true);
   
   connect(minSlider, SIGNAL(valueChanged(int)), this, SLOT(updateBoxes()));
   connect(maxSlider, SIGNAL(valueChanged(int)), this, SLOT(updateBoxes()));
   connect(applyB, SIGNAL(clicked()), this, SLOT(applyScale()));

   constructHistogram(viewer->imgBuffer);
   
   minOUT->setText(QString("%1").arg(minSlider->value() * binSize));
   maxOUT->setText(QString("%1").arg(maxSlider->value() * binSize));
   
 }
 
 FITSHistogram::~FITSHistogram() {}
 
void FITSHistogram::updateBoxes()
{

   minOUT->setText(QString("%1").arg(minSlider->value() * binSize));
   maxOUT->setText(QString("%1").arg(maxSlider->value() * binSize));

   update();

}

void FITSHistogram::applyScale()
{
  int swap;
  int min = minSlider->value();
  int max = maxSlider->value();
  
  //kdDebug() << "Width " << viewer->image->width << endl;
  
  if (min > max)
  {
    swap = min;
    min  = max;
    max  = swap;
  }
  
  min *= binSize;
  max *= binSize;
  
  // Auto
  if (autoR->isOn())
  {
    viewer->image->rescale(FITSImage::FITSAuto, min, max);
    type = 0;
  }
  // Linear
  else if (linearR->isOn())
  {
    viewer->image->rescale(FITSImage::FITSLinear, min, max);
    type = 1;
  } 
  // Log
  else if (logR->isOn())
  {
    viewer->image->rescale(FITSImage::FITSLog, min, max);
    type = 2;
  }
  // Exp
  else if (expoR->isOn())
  {
    viewer->image->rescale(FITSImage::FITSExp, min, max);
    type = 3;
  }
  else if (sqrtR->isOn())
  {
    viewer->image->rescale(FITSImage::FITSSqrt, min, max);
    type = 4;
  }
    
}
 
void FITSHistogram::constructHistogram(unsigned int * buffer)
{
  int maxHeight = 0;
  int height    = histFrame->height(); 
  int id;
  binSize = (int) ( ((double) viewer->stats.max - viewer->stats.min) / ( (double) BARS));
  
  for (int i=0; i < BARS; i++)
    histArray[i] = 0;
    
  for (int i=0; i < viewer->stats.width * viewer->stats.height; i++)
  {
     id = (buffer[i] - viewer->stats.min) / binSize;
     if (id >= BARS) id = BARS - 1;
     histArray[id]++;
  }
 
 maxHeight = findMax() / height;
 histogram = new QPixmap(500, 150, 1);
 histogram->fill(Qt::white);
 QPainter p(histogram);
 QPen pen( black, 1);
 p.setPen(pen);
 for (int i=0; i < BARS; i++)
     p.drawLine(i, height , i, height - (int) ((double) histArray[i] / (double) maxHeight)); 
  
}

void FITSHistogram::paintEvent( QPaintEvent *e)
{

  int height    = histFrame->height(); 
  int tolerance = 5;
  int xMin = minSlider->value(), xMax = maxSlider->value();
  
  QPainter p(histFrame);
  QPen pen;
  pen.setWidth(1);
  
  bitBlt(histFrame, 0, 0, histogram);
  
  pen.setColor(blue);
  p.setPen(pen);
  
  if (xMin <= minSlider->minValue() + tolerance)
    xMin = minSlider->minValue() + tolerance;
  else if (xMin >= minSlider->maxValue() - tolerance)
    xMin = minSlider->maxValue() - tolerance;
  
  if (xMax <= maxSlider->minValue() + tolerance)
    xMax = maxSlider->minValue() + tolerance;
  else if (xMax >= maxSlider->maxValue() - tolerance)
    xMax = maxSlider->maxValue() - tolerance;
  
  p.drawLine(xMin, 2, xMin, height -2);
  pen.setColor(red);
  p.setPen(pen);
  p.drawLine(xMax, 2, xMax, height -2);
  
  
}

void FITSHistogram::mouseMoveEvent( QMouseEvent *e)
{
  int x = e->x();
  int y = e->y();
  
  x -= histFrame->x();
  y -= histFrame->y();
  
  if (x < 0 || x > histFrame->width() || y < 0 || y > histFrame->height() )
   return;
  
  //kdDebug() << "X= " << x << " -- Y= " << y << endl;
  intensityOUT->setText(QString("%1").arg(x * binSize));
  frequencyOUT->setText(QString("%1").arg(histArray[x]));

}


int FITSHistogram::findMax()
{
  int max =0;
  
  for (int i=0; i < BARS; i++)
    if (histArray[i] > max) max = histArray[i];
    
  return max;
}

histCommand::histCommand(QWidget * parent, int newType, QImage* newIMG, QImage *oldIMG, unsigned int * old_buffer)
{
  viewer    = (FITSViewer *) parent;
  type      = newType;
  newImage  = new QImage();
  oldImage  = new QImage();
  *newImage = newIMG->copy();
  *oldImage = oldIMG->copy();
  oldBuffer = old_buffer;
}

histCommand::~histCommand() {}
            
void histCommand::execute()
{
  viewer->image->displayImage = newImage;
  viewer->image->zoomToCurrent();
}

void histCommand::unexecute()
{
  viewer->imgBuffer           = oldBuffer;
  viewer->image->displayImage = oldImage;
  viewer->image->zoomToCurrent();
}

QString histCommand::name() const
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
    return i18n("Exponential Scale");
    break;
  case 4:
    return i18n("Square Root Scale");
    break;
  default:
   break;
 }
 
 return i18n("Unknown");

}



 

#include "fitshistogram.moc"
