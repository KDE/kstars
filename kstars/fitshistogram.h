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
 
 #include "histdialog.h"
 #include <kcommand.h>
 
 #define BARS 500
 
 class FITSViewer;
 class TQPixmap;
 
 class FITSHistogram : public histDialog
 {
   Q_OBJECT
   
   public:
    FITSHistogram(TQWidget *parent, const char * name = 0);
    ~FITSHistogram();
    
    void constructHistogram(float *buffer);
    int  findMax();
    int type;
    int napply;
    
    private:
    int histArray[BARS]; 
    double binSize;

    FITSViewer * viewer;
    TQPixmap *histogram;
       
    
    protected:
    void paintEvent( TQPaintEvent *e);
    void mouseMoveEvent( TQMouseEvent *e);
    
    
    public slots:
    void applyScale();
    void updateBoxes();
    void updateIntenFreq(int x);
    
    
 };
 
 class FITSHistogramCommand : public KCommand
{
  public:
        FITSHistogramCommand(TQWidget * parent, FITSHistogram *inHisto, int newType, int lmin, int lmax);
	~FITSHistogramCommand();
            
        void execute();
        void unexecute();
        TQString name() const;

    
    private:
        FITSHistogram *histo;
        int type;
	int min, max;
	float *buffer;
        FITSViewer *viewer;
	TQImage *oldImage;
};
 
 
 #endif
 

  
