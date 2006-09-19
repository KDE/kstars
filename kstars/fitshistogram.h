/***************************************************************************
                          fitshistogram.h  -  FITS Historgram
                          ---------------
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
 
#ifndef FITSHISTOGRAM
#define FITSHISTOGRAM
 
#include "ui_fitshistogramui.h"
#include <kcommand.h>

#include <QPixmap>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QDialog>
 
#define CIRCLE_DIM	16
 
class FITSViewer;
class QPixmap;
 
 class histogramUI : public QDialog, public Ui::FITSHistogramUI
 {
   Q_OBJECT

    public:
     histogramUI(QDialog *parent=0);

 };

 class FITSHistogram : public QDialog
 {
   Q_OBJECT
   
   public:
    FITSHistogram(QWidget *parent);
    ~FITSHistogram();
    
    void constructHistogram(int hist_width, int hist_height);
    int  findMax(int hist_width);
    int type;
    int napply;
    double histFactor;
    int *histArray;

    
    private:
    
    double binSize;
    histogramUI *ui;
    int histogram_height, histogram_width;
    double fits_min, fits_max;

    FITSViewer * viewer;
    
    public slots:
    void applyScale();
    void updateBoxes(int lowerLimit, int upperLimit);
    void updateIntenFreq(int x);
    
    
 };
 
 class FITSHistogramCommand : public KCommand
{
  public:
        FITSHistogramCommand(QWidget * parent, FITSHistogram *inHisto, int newType, int lmin, int lmax);
	~FITSHistogramCommand();
            
        void execute();
        void unexecute();
        QString name() const;

    
    private:
        FITSHistogram *histo;
        int type;
	int min, max;
	float *buffer;
        FITSViewer *viewer;
	QImage *oldImage;
};
 
 
#endif
